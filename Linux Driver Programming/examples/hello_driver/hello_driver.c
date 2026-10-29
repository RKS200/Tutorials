
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

/* Module initialization function */
static int __init hello_driver_init(void)
{
    pr_info("Hello Driver: Module loaded\n");

    return 0;
}

/* Module cleanup function */
static void __exit hello_driver_exit(void)
{
    pr_info("Hello Driver: Module unloaded\n");
}

module_init(hello_driver_init);
module_exit(hello_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kowshic Shankar R");
MODULE_DESCRIPTION("Simple Linux Kernel Module Example");
MODULE_VERSION("1.0");

