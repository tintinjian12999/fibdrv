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


MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ktime_t kt;

#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)
/*
static long long fib_sequence(long long k, char *buf)
{
    long long *f = kmalloc((k + 2) * sizeof(long long), GFP_KERNEL);

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}
*/


static void str_reverse(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++)
        XOR_SWAP(&str[i], &str[n - i - 1], char);
}

static void string_number_add(char *a, char *b, char *out)
{
    int carry = 0, sum, i = 0;
    size_t a_len = strlen(a), b_len = strlen(b);
    // Check string a is longer than string b
    if (a_len < b_len) {
        XOR_SWAP(a, b, char);
        XOR_SWAP(&a_len, &b_len, size_t);
    }
    for (i = 0; i < b_len; i++) {
        sum = (a[i] - '0') + (b[i] - '0') + carry;
        out[i] = (sum % 10) + '0';
        carry = sum / 10;
    }
    for (i = b_len; i < a_len; i++) {
        sum = (a[i] - '0') + carry;
        out[i] = (sum % 10) + '0';
        carry = sum / 10;
    }

    if (carry)
        out[i++] = carry + '0';
    out[i] = '\0';
}

typedef struct str {
    char numstr[256];
} str_t;

static long long fib_sequence(long long k, char *buf)
{
    int i = 0;
    str_t *f = kmalloc((k + 2) * sizeof(str_t), GFP_KERNEL);
    strncpy(f[0].numstr, "0", 1);
    f[0].numstr[1] = '\0';
    strncpy(f[1].numstr, "1", 1);
    f[1].numstr[1] = '\0';

    for (i = 2; i <= k; i++) {
        string_number_add(f[i - 1].numstr, f[i - 2].numstr, f[i].numstr);
    }
    size_t f_size = strlen(f[k].numstr);
    str_reverse(f[k].numstr, f_size);
    if (copy_to_user(buf, f[k].numstr, f_size))
        return -EFAULT;
    return f_size;
}

static long long fib_time_proxy(long long k, char *buf)
{
    kt = ktime_get();
    long long result = fib_sequence(k, buf);
    kt = ktime_sub(ktime_get(), kt);
    return result;
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
    return (ssize_t) fib_time_proxy(*offset, buf);
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
