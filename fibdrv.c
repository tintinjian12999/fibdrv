#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include "bignum.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"


#define MAX_LENGTH 10000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;
static size_t size;

/*
static uint64_t fast_doubling(uint32_t target)
{
    if(target <= 2)
        return 1;
    uint64_t n = fast_doubling(target >> 1);
    uint64_t n1 = fast_doubling((target >> 1) + 1);
    if(target & 1)
        return n * n + n1 * n1;
    return n * ((n1 << 1) - n);
}
*/


size_t string_fast_doubling(long long k, char *out)
{
    char reg1[STR_NUM], reg2[STR_NUM], reg3[STR_NUM], reg4[STR_NUM];
    static char result[STR_NUM];
    if (k == 0) {
        strncpy(out, "0", 2);
        return 1;
    } else if (k == 1) {
        strncpy(out, "1", 2);
        return 1;
    } else if (k == 2) {
        strncpy(out, "1", 2);
        return 1;
    }
    size = string_fast_doubling(k >> 1, reg1);
    size = string_fast_doubling((k >> 1) + 1, reg2);

    if (k & 1) {
        string_mul(reg1, reg1, reg3);
        string_mul(reg2, reg2, reg4);
        string_add(reg4, reg3, result);
    } else {
        string_mul(reg2, "2", reg3);
        string_sub(reg3, reg1, reg4);
        string_mul(reg4, reg1, result);
    }
    size = strlen(result);
    strncpy(out, result, size + 1);
    return size;
}


typedef struct str {
    char numstr[256];
} str_t;


size_t string_iterative(long long k, char *out)
{
    static int i = 0;
    str_t *f = kmalloc((k + 2) * sizeof(str_t), GFP_KERNEL);
    f[0].numstr[0] = '0';
    f[0].numstr[1] = '\0';
    f[1].numstr[0] = '1';
    f[1].numstr[1] = '\0';

    for (i = 2; i <= k; i++) {
        string_add(f[i - 1].numstr, f[i - 2].numstr, f[i].numstr);
    }
    size = strlen(f[k].numstr);
    str_reverse(f[k].numstr, size);
    strncpy(out, f[k].numstr, size + 1);
    return size;
};

static uint64_t fib_time_proxy(long long k, char *buf)
{
    char out[STR_NUM];

    kt = ktime_get();
    size = string_fast_doubling(k, out);
    str_reverse(out, size);
    // size = string_iterative(k, out);;
    kt = ktime_sub(ktime_get(), kt);
    if (copy_to_user(buf, out, size + 1))
        return -EFAULT;
    return size;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    return (uint64_t) fib_time_proxy(*offset, buf);
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
