/* Generates Fast PWM,
   Change the Frequency using <PD4,PD5> [31.25kHz, 3.90625KHz, 488.28Hz, 122.07Hz, 30.52Hz] for 8MHz FCPU,
   Change the Duty Cylce using <PD6, PD7>,
   Buttons at PDx are externally pulled up,
 */

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define PWM_STEP 32
#define BUTTONS ((1 << PD4) | (1 << PD5) | (1 << PD6) | (1 << PD7))
#define PRESCALER_MASK ((1 << CS02) | (1 << CS01) | (1 << CS00))

const uint8_t prescalers[5] = {
	(1 << CS00),                                      // 1
	(1 << CS01),                                      // 8
	(1 << CS01) | (1 << CS00),                       // 64
	(1 << CS02),                                      // 256
	(1 << CS02) | (1 << CS00)                        // 1024
};

int main(void)
{
	uint8_t fsel = 0;
	uint8_t previous, current, confirmed;

	DDRD |= (1 << PD3);  // OC2 output

	OCR2 = 127;          // Approximately 50% duty cycle

	TCCR2 = (1 << WGM00) |
		(1 << WGM01) |
		(1 << COM01) |
		prescalers[fsel];
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <libusb-1.0/libusb.h>

#define VID                 0x2b7e
#define PID                 0xb851

#define VS_INTERFACE        1
#define VS_ENDPOINT         0x81

#define FORMAT_MJPEG        1
#define FRAME_640x480       2
#define FRAME_INTERVAL      333333

#define WIDTH               640
#define HEIGHT              480

#define NUM_TRANSFERS       8
#define ISO_PACKETS         32
#define ISO_PACKET_SIZE     3072

#define FRAME_MAX_SIZE      700000

static libusb_context *ctx;
static libusb_device_handle *dev;

static unsigned char frame[FRAME_MAX_SIZE];
static size_t frame_size = 0;

static volatile int running = 1;
static volatile int frame_done = 0;

static void stop_handler(int sig)
{
    running = 0;
}

static int uvc_control(
    unsigned char request,
    unsigned char request_type,
    unsigned short value,
    unsigned short index,
    unsigned char *data,
    unsigned short length)
{
    return libusb_control_transfer(
        dev,
        request_type,
        request,
        value,
        index,
        data,
        length,
        1000
    );
}

static int uvc_probe_commit(void)
{
    unsigned char probe[26] = {0};
    int r;

    /*
     * GET_CUR VS_PROBE_CONTROL
     *
     * bmRequestType = 0xA1
     * bRequest      = GET_CUR (0x81)
     * wValue        = VS_PROBE_CONTROL (0x0100)
     * wIndex        = interface 1
     */
    r = uvc_control(
        0x81,
        0xA1,
        0x0100,
        VS_INTERFACE,
        probe,
        sizeof(probe)
    );

    if (r < 0) {
        printf("GET_CUR(PROBE) failed: %s\n",
               libusb_error_name(r));
        return -1;
    }

    printf("Probe returned %d bytes\n", r);

    /*
     * Set the format we want.
     *
     * UVC 1.0 probe structure:
     *
     * byte 0-1   bmHint
     * byte 2     format index
     * byte 3     frame index
     * byte 4-7   frame interval
     */
    probe[0] = 0x00;
    probe[1] = 0x00;

    probe[2] = FORMAT_MJPEG;
    probe[3] = FRAME_640x480;

    probe[4] = 0x35;
    probe[5] = 0x16;
    probe[6] = 0x05;
    probe[7] = 0x00;

    /*
     * Send PROBE.
     */
    r = uvc_control(
        0x01,
        0x21,
        0x0100,
        VS_INTERFACE,
        probe,
        sizeof(probe)
    );

    if (r < 0) {
        printf("SET_CUR(PROBE) failed: %s\n",
               libusb_error_name(r));
        return -1;
    }

    /*
     * Read back negotiated probe values.
     */
    memset(probe, 0, sizeof(probe));

    r = uvc_control(
        0x81,
        0xA1,
        0x0100,
        VS_INTERFACE,
        probe,
        sizeof(probe)
    );

    if (r < 0) {
        printf("GET_CUR(PROBE) failed: %s\n",
               libusb_error_name(r));
        return -1;
    }

    printf("Negotiated format index : %u\n", probe[2]);
    printf("Negotiated frame index  : %u\n", probe[3]);

    unsigned int interval =
        probe[4] |
        (probe[5] << 8) |
        (probe[6] << 16) |
        (probe[7] << 24);

    printf("Frame interval          : %u (100ns)\n",
           interval);

    unsigned int max_frame =
        probe[18] |
        (probe[19] << 8) |
        (probe[20] << 16) |
        (probe[21] << 24);

    unsigned int max_payload =
        probe[22] |
        (probe[23] << 8) |
        (probe[24] << 16) |
        (probe[25] << 24);

    printf("Max frame size          : %u\n", max_frame);
    printf("Max payload size        : %u\n", max_payload);

    /*
     * COMMIT
     */
    r = uvc_control(
        0x01,
        0x21,
        0x0200,
        VS_INTERFACE,
        probe,
        sizeof(probe)
    );

    if (r < 0) {
        printf("SET_CUR(COMMIT) failed: %s\n",
               libusb_error_name(r));
        return -1;
    }

    printf("UVC streaming parameters committed\n");

    return 0;
}

static void callback(struct libusb_transfer *transfer)
{
    for (int i = 0; i < transfer->num_iso_packets; i++) {

        struct libusb_iso_packet_descriptor *p =
            &transfer->iso_packet_desc[i];

        if (p->status != LIBUSB_TRANSFER_COMPLETED)
            continue;

        int len = p->actual_length;

        if (len < 2)
            continue;

        unsigned char *data =
            libusb_get_iso_packet_buffer_simple(transfer, i);

        /*
         * UVC payload header.
         *
         * data[0] = header length
         * data[1] = flags
         *
         * Bit 1 of byte 1 = EOF
         */
        int header_len = data[0];
        unsigned char flags = data[1];

        if (header_len < 2 || header_len >= len)
            continue;

        int payload_len = len - header_len;

        if (frame_size + payload_len >= FRAME_MAX_SIZE) {
            printf("Frame too large\n");
            frame_size = 0;
            continue;
        }

        memcpy(
            frame + frame_size,
            data + header_len,
            payload_len
        );

        frame_size += payload_len;

        /*
         * EOF = End Of Frame
         */
        if (flags & 0x02) {

            FILE *fp = fopen("frame.jpg", "wb");

            if (fp) {
                fwrite(frame, 1, frame_size, fp);
                fclose(fp);

                printf(
                    "Captured frame: %zu bytes\n",
                    frame_size
                );

                frame_done = 1;
                running = 0;
            }

            frame_size = 0;
        }
    }

    /*
     * Keep receiving transfers until we have a frame.
     */
    if (running) {
        int r = libusb_submit_transfer(transfer);

        if (r != 0) {
            printf(
                "libusb_submit_transfer failed: %s\n",
                libusb_error_name(r)
            );

            running = 0;
        }
    }
}

int main(void)
{
    int r;

    signal(SIGINT, stop_handler);

    r = libusb_init(&ctx);

    if (r != 0) {
        printf("libusb_init failed\n");
        return 1;
    }

    dev = libusb_open_device_with_vid_pid(
        ctx,
        VID,
        PID
    );

    if (!dev) {
        printf("Camera could not be opened\n");
        libusb_exit(ctx);
        return 1;
    }

    printf("Camera opened\n");

    /*
     * Detach Linux uvcvideo driver from
     * the VideoStreaming interface.
     */
    if (libusb_kernel_driver_active(dev, VS_INTERFACE) == 1) {

        printf("Detaching uvcvideo...\n");

        r = libusb_detach_kernel_driver(
            dev,
            VS_INTERFACE
        );

        if (r != 0) {
            printf(
                "Could not detach driver: %s\n",
                libusb_error_name(r)
            );

            libusb_close(dev);
            libusb_exit(ctx);
            return 1;
        }
    }

    r = libusb_claim_interface(
        dev,
        VS_INTERFACE
    );

    if (r != 0) {
        printf(
            "Could not claim interface: %s\n",
            libusb_error_name(r)
        );

        libusb_close(dev);
        libusb_exit(ctx);
        return 1;
    }

    printf("VideoStreaming interface claimed\n");

    /*
     * Tell the camera:
     *
     * MJPEG
     * 640x480
     * 30 FPS
     */
    if (uvc_probe_commit() < 0)
        goto cleanup;

    /*
     * Alternate setting 6:
     *
     * 3 x 1024 bytes
     *
     * This is the highest-bandwidth
     * isochronous setting reported by
     * this camera.
     */
    r = libusb_set_interface_alt_setting(
        dev,
        VS_INTERFACE,
        6
    );

    if (r != 0) {
        printf(
            "Could not set alternate setting: %s\n",
            libusb_error_name(r)
        );

        goto cleanup;
    }

    printf("Alternate setting 6 selected\n");

    struct libusb_transfer *transfers[NUM_TRANSFERS];

    memset(transfers, 0, sizeof(transfers));

    for (int i = 0; i < NUM_TRANSFERS; i++) {

        transfers[i] =
            libusb_alloc_transfer(ISO_PACKETS);

        if (!transfers[i]) {
            printf("Could not allocate transfer\n");
            running = 0;
            break;
        }

        unsigned char *buffer =
            malloc(ISO_PACKETS * ISO_PACKET_SIZE);

        if (!buffer) {
            printf("Could not allocate buffer\n");
            running = 0;
            break;
        }

        libusb_fill_iso_transfer(
            transfers[i],
            dev,
            VS_ENDPOINT,
            buffer,
            ISO_PACKETS * ISO_PACKET_SIZE,
            ISO_PACKETS,
            callback,
            NULL,
            1000
        );

        libusb_set_iso_packet_lengths(
            transfers[i],
            ISO_PACKET_SIZE
        );

        r = libusb_submit_transfer(transfers[i]);

        if (r != 0) {
            printf(
                "Submit failed: %s\n",
                libusb_error_name(r)
            );

            running = 0;
            break;
        }
    }

    printf("Waiting for frame...\n");

    while (running) {

        r = libusb_handle_events(ctx);

        if (r != 0) {
            printf(
                "libusb_handle_events: %s\n",
                libusb_error_name(r)
            );

            break;
        }
    }

    /*
     * Stop transfers.
     */
    for (int i = 0; i < NUM_TRANSFERS; i++) {

        if (transfers[i]) {

            libusb_cancel_transfer(
                transfers[i]
            );
        }
    }

    /*
     * Give cancelled transfers time
     * to finish.
     */
    for (int i = 0; i < 100 && !frame_done; i++) {

        struct timeval tv;

        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        libusb_handle_events_timeout(
            ctx,
            &tv
        );
    }

    for (int i = 0; i < NUM_TRANSFERS; i++) {

        if (transfers[i]) {

            free(transfers[i]->buffer);

            libusb_free_transfer(
                transfers[i]
            );
        }
    }

cleanup:

    libusb_release_interface(
        dev,
        VS_INTERFACE
    );

    /*
     * Give control back to Linux.
     */
    libusb_attach_kernel_driver(
        dev,
        VS_INTERFACE
    );

    libusb_close(dev);
    libusb_exit(ctx);

    return 0;
}
	DDRD &= ~BUTTONS;

	// External pull-ups are assumed.

	previous = PIND & BUTTONS;

	while (1)
	{
		current = PIND & BUTTONS;

		if (current != previous)
		{
			_delay_ms(20);

			confirmed = PIND & BUTTONS;

			if (current == confirmed)
			{
				// PD5: Increase duty cycle
				if ((previous & (1 << PD5)) &&
						!(confirmed & (1 << PD5)))
				{
					if (OCR2 > 255 - PWM_STEP)
						OCR2 = 255;
					else
						OCR2 += PWM_STEP;
				}

				// PD4: Decrease duty cycle
				if ((previous & (1 << PD4)) &&
						!(confirmed & (1 << PD4)))
				{
					if (OCR2 < PWM_STEP)
						OCR2 = 0;
					else
						OCR2 -= PWM_STEP;
				} 

				// PD7: Decrease frequency
				if ((previous & (1 << PD7)) &&
						!(confirmed & (1 << PD7)))
				{
					fsel = (fsel == 0) ? 4 : fsel - 1;

					TCCR2 = (TCCR2 & ~PRESCALER_MASK)
						| prescalers[fsel];
				}

				// PD6: Increase frequency
				if ((previous & (1 << PD6)) &&
						!(confirmed & (1 << PD6)))
				{
					fsel = (fsel + 1) % 5;

					TCCR2 = (TCCR2 & ~PRESCALER_MASK)
						| prescalers[fsel];
				}

				previous = confirmed;
			}
		}
	}
}
