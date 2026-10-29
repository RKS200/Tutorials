#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define DEVICE "/dev/video0"
#define WIDTH  640
#define HEIGHT 480
#define NUM_BUFFERS 4

struct buffer {
    void *start;
    size_t length;
};

static struct buffer buffers[NUM_BUFFERS];

static int xioctl(int fd, unsigned long request, void *arg)
{
    int ret;

    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);

    return ret;
}

int main(void)
{
    int fd;
    FILE *outfile;
    struct v4l2_capability cap;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req;
    enum v4l2_buf_type type;
    int i;

    /* -------------------------------------------------- */
    /* 1. Open video device                              */
    /* -------------------------------------------------- */

    fd = open(DEVICE, O_RDWR);
    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------- */
    /* 2. Query device capabilities                       */
    /* -------------------------------------------------- */

    memset(&cap, 0, sizeof(cap));

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("VIDIOC_QUERYCAP");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Driver : %s\n", cap.driver);
    printf("Card   : %s\n", cap.card);

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf(stderr, "Device does not support video capture\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        fprintf(stderr, "Device does not support streaming\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------- */
    /* 3. Set video format                               */
    /* -------------------------------------------------- */

    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("VIDIOC_S_FMT");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Format: %dx%d\n",
           fmt.fmt.pix.width,
           fmt.fmt.pix.height);

    printf("Pixel format: %c%c%c%c\n",
           fmt.fmt.pix.pixelformat & 0xff,
           (fmt.fmt.pix.pixelformat >> 8) & 0xff,
           (fmt.fmt.pix.pixelformat >> 16) & 0xff,
           (fmt.fmt.pix.pixelformat >> 24) & 0xff);

    printf("Bytes per line: %u\n", fmt.fmt.pix.bytesperline);
    printf("Image size: %u\n", fmt.fmt.pix.sizeimage);

    /* -------------------------------------------------- */
    /* 4. Request MMAP buffers                           */
    /* -------------------------------------------------- */

    memset(&req, 0, sizeof(req));

    req.count = NUM_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) == -1) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Number of buffers: %u\n", req.count);

    if (req.count < 2) {
        fprintf(stderr, "Insufficient buffer memory\n");
        close(fd);
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------- */
    /* 5. Query each buffer and mmap it                  */
    /* -------------------------------------------------- */

    for (i = 0; i < req.count; i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) == -1) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            return EXIT_FAILURE;
        }

        buffers[i].length = buf.length;

        buffers[i].start = mmap(
            NULL,
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            buf.m.offset
        );

        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Buffer %d: address=%p size=%zu\n",
               i,
               buffers[i].start,
               buffers[i].length);
    }

    /* -------------------------------------------------- */
    /* 6. Queue all buffers                              */
    /* -------------------------------------------------- */

    for (i = 0; i < req.count; i++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("VIDIOC_QBUF");
            close(fd);
            return EXIT_FAILURE;
        }
    }

    printf("All buffers queued\n");

    /* -------------------------------------------------- */
    /* 7. Start streaming                                */
    /* -------------------------------------------------- */

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(fd, VIDIOC_STREAMON, &type) == -1) {
        perror("VIDIOC_STREAMON");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Streaming started\n");

    /* -------------------------------------------------- */
    /* 8. Open output YUV file                           */
    /* -------------------------------------------------- */

    outfile = fopen("output.yuv", "wb");

    if (!outfile) {
        perror("fopen");
        return EXIT_FAILURE;
    }

    /* -------------------------------------------------- */
    /* 9. Capture frames                                 */
    /* -------------------------------------------------- */

    for (int frame = 0; frame < 100; frame++) {

        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        /* Wait for a filled buffer */
        if (xioctl(fd, VIDIOC_DQBUF, &buf) == -1) {

            if (errno == EAGAIN) {
                frame--;
                continue;
            }

            perror("VIDIOC_DQBUF");
            break;
        }

        printf("Frame %d: buffer=%d bytes=%u\n",
               frame,
               buf.index,
               buf.bytesused);

        /* ---------------------------------------------- */
        /* Save captured frame                            */
        /* ---------------------------------------------- */

        fwrite(
            buffers[buf.index].start,
            1,
            buf.bytesused,
            outfile
        );

        /* ---------------------------------------------- */
        /* Give buffer back to driver                    */
        /* ---------------------------------------------- */

        if (xioctl(fd, VIDIOC_QBUF, &buf) == -1) {
            perror("VIDIOC_QBUF");
            break;
        }
    }

    /* -------------------------------------------------- */
    /* 10. Stop streaming                                */
    /* -------------------------------------------------- */

    if (xioctl(fd, VIDIOC_STREAMOFF, &type) == -1) {
        perror("VIDIOC_STREAMOFF");
    }

    /* -------------------------------------------------- */
    /* 11. Cleanup                                       */
    /* -------------------------------------------------- */

    fclose(outfile);

    for (i = 0; i < req.count; i++) {
        munmap(buffers[i].start, buffers[i].length);
    }

    close(fd);

    printf("Saved 100 frames to output.yuv\n");

    return EXIT_SUCCESS;
}
