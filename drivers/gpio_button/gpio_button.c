//-----------------------------------------------------------------------------
// File:   gpio_button.c
//
// Description:
// Platform driver that detects button presses with hardware debouncing and
// provides LED control via character device and sysfs interfaces.
//
// Notes:
// - Uses Device Tree for GPIO mapping (custom,gpio-button compatible)
// - Implements hardware debouncing with 50ms timer and atomic locks
// - Creates character device /dev/gpio_button for blocking button event reads
// - Exposes sysfs attribute at /sys/.../led_status for LED state control
// - Handles active-low buttons and supports configurable LED polarity
// - Features interrupt-driven button detection with GPIO IRQ handling
// - Provides poll() support for event-driven userspace applications
// - Includes robust error handling and resource cleanup
//-----------------------------------------------------------------------------
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/atomic.h>
#include <linux/version.h>
#include <linux/timer.h>

#define DRIVER_NAME "gpio_button"

/* Map to the right timer teardown helper by kernel version */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,6,0)
#  define GPIOBTN_TIMER_DELETE(t)  timer_shutdown_sync((t))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
/* mid trees used timer_delete_sync() */
#  define GPIOBTN_TIMER_DELETE(t)  timer_delete_sync((t))
#else
#  define GPIOBTN_TIMER_DELETE(t)  del_timer_sync((t))
#endif

static struct timer_list debounce_timer;
static atomic_t debounce_active = ATOMIC_INIT(0);

static struct gpio_desc *button_gpio;
static struct gpio_desc *led_gpio;
static int irq_number;
static dev_t dev_num;
static struct cdev c_dev;
static struct class *cl;
static DECLARE_WAIT_QUEUE_HEAD(button_wait);
static atomic_t button_event_flag = ATOMIC_INIT(0);
static volatile int led_status = 0;

static void debounce_timer_callback(struct timer_list *timer)
{
	int button_state = gpiod_get_value(button_gpio);

	/* Assuming active-low button: pressed -> 0 */
	if (button_state == 0) {
		atomic_set(&button_event_flag, 1);
		wake_up(&button_wait);
	}

	/* Re-enable ISR debounce gating */
	atomic_set(&debounce_active, 0);
}

static irqreturn_t gpio_button_isr(int irq, void *dev_id)
{
	/* Ignore interrupts during debounce period */
	if (atomic_read(&debounce_active))
		return IRQ_HANDLED;

	/* Start debounce timer */
	atomic_set(&debounce_active, 1);
	mod_timer(&debounce_timer, jiffies + msecs_to_jiffies(50)); /* 50ms */

	return IRQ_HANDLED;
}

static ssize_t gpio_button_read(struct file *file, char __user *buffer,
				size_t len, loff_t *offset)
{
	char event_char;
	int ret;

	/* Block until an event arrives */
	ret = wait_event_interruptible(button_wait,
				       atomic_read(&button_event_flag));
	if (ret)
		return -ERESTARTSYS; /* interrupted */

	/* Event occurred, translate it to ASCII '1' */
	pr_info("gpio_button: %s():%d: Button event occurred\n",
		__func__, __LINE__);
	event_char = '1';

	/* Clear flag before copying to user */
	atomic_set(&button_event_flag, 0);

	if (copy_to_user(buffer, &event_char, sizeof(event_char)))
		return -EFAULT;

	return sizeof(event_char);
}

static unsigned int gpio_button_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &button_wait, wait);
	return atomic_read(&button_event_flag) ? POLLIN : 0;
}

static int gpio_button_open(struct inode *inode, struct file *file)
{
	return 0;
}

/* OK for modern kernels; .owner can be present or ignored by the tree */
static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open  = gpio_button_open,
	.read  = gpio_button_read,
	.poll  = gpio_button_poll,
};

/* sysfs: show/store for LED */
static ssize_t led_status_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", led_status);
}

static ssize_t led_status_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;
	char local_buf[16];
	int ret;

	if (count >= sizeof(local_buf)) {
		pr_err("gpio_button: Input is too long (%zu bytes)\n", count);
		return -EINVAL;
	}

	memcpy(local_buf, buf, count);
	local_buf[count] = '\0';

	if (count && local_buf[count - 1] == '\n')
		local_buf[count - 1] = '\0';

	pr_info("gpio_button: Processed input: '%s'\n", local_buf);

	ret = kstrtoul(local_buf, 10, &val);
	if (ret) {
		pr_err("gpio_button: kstrtoul failed, ret=%d\n", ret);
		return ret;
	}
	if (val != 0 && val != 1) {
		pr_err("gpio_button: value must be 0 or 1, got %lu\n", val);
		return -EINVAL;
	}

	led_status = val;
	gpiod_set_value(led_gpio, led_status);
	pr_info("gpio_button: LED status set to %lu\n", val);

	return count;
}

static DEVICE_ATTR(led_status, 0664, led_status_show, led_status_store);
static struct device *sysfs_dev;

static int gpio_button_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	pr_info("gpio_button: %s():%d: Probe started\n", __func__, __LINE__);

	/* Get GPIO descriptors from DT */
	button_gpio = gpiod_get(dev, "button", GPIOD_IN);
	if (IS_ERR(button_gpio)) {
		dev_err(dev, "Failed to get BUTTON GPIO: %ld\n",
			PTR_ERR(button_gpio));
		pr_err("gpio_button: %s():%d: Button GPIO error, code: %ld\n",
		       __func__, __LINE__, PTR_ERR(button_gpio));
		return PTR_ERR(button_gpio);
	}
	pr_info("gpio_button: %s():%d: Button GPIO acquired: %d\n",
		__func__, __LINE__, desc_to_gpio(button_gpio));

	gpiod_direction_input(button_gpio);
	gpiod_set_debounce(button_gpio, 50000); /* 50 ms */

	led_gpio = gpiod_get(dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(led_gpio)) {
		dev_err(dev, "Failed to get LED GPIO: %ld\n", PTR_ERR(led_gpio));
		pr_err("gpio_button: %s():%d: LED GPIO error, code: %ld\n",
		       __func__, __LINE__, PTR_ERR(led_gpio));
		ret = PTR_ERR(led_gpio);
		goto err_led;
	}
	pr_info("gpio_button: %s():%d: LED GPIO acquired: %d\n",
		__func__, __LINE__, desc_to_gpio(led_gpio));

	/* Initialize debounce timer BEFORE enabling IRQ */
	timer_setup(&debounce_timer, debounce_timer_callback, 0);

	/* Setup interrupt */
	irq_number = gpiod_to_irq(button_gpio);
	if (irq_number < 0) {
		dev_err(dev, "Failed to get IRQ: %d\n", irq_number);
		pr_err("gpio_button: %s():%d: IRQ error, code: %d\n",
		       __func__, __LINE__, irq_number);
		ret = irq_number;
		goto err_irqnum;
	}
	pr_info("gpio_button: %s():%d: IRQ number: %d\n",
		__func__, __LINE__, irq_number);

	ret = request_irq(irq_number, gpio_button_isr,
			  IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			  DRIVER_NAME, NULL);
	if (ret) {
		dev_err(dev, "Failed to request IRQ %d\n", irq_number);
		pr_err("GPIO Driver: IRQ Request Error! Code: %d\n", ret);
		goto err_req_irq;
	}
	pr_info("gpio_button: %s():%d: IRQ registered successfully\n",
		__func__, __LINE__);

	/* Create character device */
	if (alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME)) {
		ret = -ENODEV;
		pr_err("gpio_button: %s():%d: Failed to allocate chrdev region\n",
		       __func__, __LINE__);
		goto err_alloc;
	}
	pr_info("gpio_button: %s():%d: chrdev region allocated\n",
		__func__, __LINE__);

	cdev_init(&c_dev, &fops);
	if (cdev_add(&c_dev, dev_num, 1)) {
		ret = -ENODEV;
		pr_err("GPIO Driver: Failed to add cdev\n");
		goto err_add;
	}
	pr_info("gpio_button: %s():%d: cdev added\n", __func__, __LINE__);

	cl = class_create(DRIVER_NAME);
	if (IS_ERR(cl)) {
		ret = PTR_ERR(cl);
		pr_err("gpio_button: %s():%d: Create class error, code: %d\n",
		       __func__, __LINE__, ret);
		goto err_class;
	}
	pr_info("gpio_button: %s():%d: Class created\n", __func__, __LINE__);

	/* /dev/gpio_button */
	if (!device_create(cl, NULL, dev_num, NULL, "%s", DRIVER_NAME)) {
		ret = -ENODEV;
		pr_err("gpio_button: %s():%d: device_create (chardev) failed\n",
		       __func__, __LINE__);
		goto err_dev_chardev;
	}

	/* Sysfs device for attribute */
	sysfs_dev = device_create(cl, NULL, 0, NULL, "gpio_button_sysfs");
	if (IS_ERR(sysfs_dev)) {
		ret = PTR_ERR(sysfs_dev);
		pr_err("gpio_button: %s():%d: Failed to create sysfs device\n",
		       __func__, __LINE__);
		goto err_dev_sysfs;
	}

	/* Sysfs attribute */
	ret = device_create_file(sysfs_dev, &dev_attr_led_status);
	if (ret) {
		pr_err("gpio_button: %s():%d: Failed to create sysfs attribute\n",
		       __func__, __LINE__);
		goto err_sysfs_attr;
	}

	pr_info("gpio_button: %s():%d: Probe completed successfully\n",
		__func__, __LINE__);
	return 0;

err_sysfs_attr:
	device_destroy(cl, 0);
err_dev_sysfs:
	device_destroy(cl, dev_num);
err_dev_chardev:
	class_destroy(cl);
err_class:
	cdev_del(&c_dev);
	unregister_chrdev_region(dev_num, 1);
err_add:
	/* fallthrough */
err_alloc:
	free_irq(irq_number, NULL);
	/* stop any pending debounce work if the ISR fired */
	GPIOBTN_TIMER_DELETE(&debounce_timer);
err_req_irq:
	/* nothing to free here beyond timer; fallthrough for timer delete */
err_irqnum:
	gpiod_put(led_gpio);
err_led:
	gpiod_put(button_gpio);
	pr_info("gpio_button: %s():%d: Probe failed, code: %d\n",
		__func__, __LINE__, ret);
	return ret;
}

static void gpio_button_remove(struct platform_device *pdev)
{
	/* Quiesce ISR, then stop any pending debounce work */
	disable_irq(irq_number);
	GPIOBTN_TIMER_DELETE(&debounce_timer);

	/* Remove sysfs attribute & devices */
	device_remove_file(sysfs_dev, &dev_attr_led_status);
	device_destroy(cl, 0);
	device_destroy(cl, dev_num);

	/* Character device teardown */
	class_destroy(cl);
	cdev_del(&c_dev);
	unregister_chrdev_region(dev_num, 1);

	/* IRQ & GPIOs */
	free_irq(irq_number, NULL);
	gpiod_put(button_gpio);
	gpiod_put(led_gpio);
}

static const struct of_device_id gpio_button_of_match[] = {
	{ .compatible = "custom,gpio-button" },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_button_of_match);

static struct platform_driver gpio_button_platform_driver = {
	.probe  = gpio_button_probe,
	.remove = gpio_button_remove,
	.driver = {
		.name           = DRIVER_NAME,
		.of_match_table = gpio_button_of_match,
	},
};

module_platform_driver(gpio_button_platform_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steve Dunnagan");
MODULE_DESCRIPTION("GPIO button and driver");
MODULE_VERSION("4.0");
