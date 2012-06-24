/*
 *  sensors.c - sensor work as input device
 *
 *  Copyright (C) 2008 Jack Ren <jack.ren@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#include <linux/sensor-input.h>
#include <asm/atomic.h>

#define DEFAULT_POLLING_DELAY	100
//#define PEERFORMANCE 

DEFINE_MUTEX(sensor_input_lock);
static struct input_dev *sensor_input_idev;	/* input device */
static int sensor_input_usage;
static LIST_HEAD(sensors);

static int sensor_input_kthread(void *data)
{
	struct sensor_input_dev *sensor = (struct sensor_input_dev *) data;
	int delay = sensor->delay;
#ifdef PEERFORMANCE
#define TIMEOUT 4
	int count, last_count;
	unsigned long timeout = jiffies + HZ * 4;
#endif

	//daemonize(sensor->name);
#ifdef PEERFORMANCE
	count = 0;
	last_count = 0;
	timeout = jiffies + HZ * TIMEOUT;
#endif
	while (!sensor->thread_exit) {
		wait_event_timeout(sensor->wait, (delay != sensor->delay)
				   || sensor->thread_exit,
				   msecs_to_jiffies(delay));
#ifdef PEERFORMANCE
		count++;
		if (time_after(jiffies, timeout)) {
			printk("%s: %dms/packet\n", sensor->name,
			       TIMEOUT * 1000 / (count - last_count));
			last_count = count;
			timeout = jiffies + HZ * TIMEOUT;
		}
#endif
		delay = sensor->delay;
		if (mutex_trylock(&sensor_input_lock)) {
			if (sensor->on) {
				if (sensor->dev)
					sensor->report(sensor->dev);
			}
			mutex_unlock(&sensor_input_lock);
		}
	}
	if (sensor->exit) {
		sensor->exit(sensor->dev);
	}
	//complete_and_exit(&sensor->thread_exit_complete, 0);
	return 0;
}

static int sensor_input_open(struct input_dev *input)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp __maybe_unused;

	mutex_lock(&sensor_input_lock);

	sensor = input_get_drvdata(input);

	if (sensor == NULL) {
		if (sensor_input_usage == 0) {
#ifdef CONFIG_INPUT_MERGED_SENSORS
			list_for_each_entry_safe(sensor, tmp, &sensors,
						 list) {
				sensor->poweron();
				sensor->on = 1;
				sensor->thread_exit = 0;
				sensor->count++;
				sensor->thread_task =
				    kthread_create(sensor_input_kthread,
						   sensor, sensor->name);
				wake_up_process(sensor->thread_task);
			}
#endif
		}
		sensor_input_usage++;
	} else {
		if (sensor->count == 0) {
			sensor->poweron();
			sensor->on = 1;
			sensor->thread_exit = 0;
			sensor->thread_task =
			    kthread_create(sensor_input_kthread, sensor,
					   sensor->name);
			wake_up_process(sensor->thread_task);
		}
		sensor->count++;
	}

	mutex_unlock(&sensor_input_lock);
	return 0;
}

static void sensor_input_close(struct input_dev *input)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp __maybe_unused;

	mutex_lock(&sensor_input_lock);

	sensor = (struct sensor_input_dev *) input_get_drvdata(input);

	if (sensor == NULL) {
		sensor_input_usage--;
		if (sensor_input_usage == 0) {
#ifdef CONFIG_INPUT_MERGED_SENSORS
			list_for_each_entry_safe(sensor, tmp, &sensors,
						 list) {
				sensor->count--;
				if (sensor->count == 0) {
					sensor->poweroff();
					sensor->on = 0;
					//init_completion(&sensor->thread_exit_complete);
					sensor->thread_exit = 1;
					wake_up(&sensor->wait);
					//wait_for_completion_timeout(&sensor->thread_exit_complete);
					kthread_stop(sensor->thread_task);
				}
			}
#endif
		}
	} else {
		sensor->count--;
		if (sensor->count == 0) {
			sensor->poweroff();
			sensor->on = 0;
			//init_completion(&sensor->thread_exit_complete);
			sensor->thread_exit = 1;
			wake_up(&sensor->wait);
			//wait_for_completion(&sensor->thread_exit_complete);
			kthread_stop(sensor->thread_task);
		}
	}
	mutex_unlock(&sensor_input_lock);
}

//#ifdef	CONFIG_PM
#if 0
static int sensor_input_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp;

	if (sensor_input_usage == 0)
		return 0;

	if (!mutex_trylock(&sensor_input_lock))
		return -EBUSY;

	list_for_each_entry_safe(sensor, tmp, &sensors, list) {
		sensor->poweroff();
		sensor->on = 0;
	}

	return 0;
}

static int sensor_input_resume(struct platform_device *pdev)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp;

	if (sensor_input_usage == 0)
		return 0;

	list_for_each_entry_safe(sensor, tmp, &sensors, list) {
		sensor->poweron();
		sensor->on = 1;
	}
	mutex_unlock(&sensor_input_lock);

	return 0;
}
#else
#define	sensor_input_suspend		NULL
#define	sensor_input_resume		NULL
#endif

#ifdef	CONFIG_PROC_FS
static ssize_t sensor_input_read_proc(struct file *file,
				      char *buffer, size_t length,
				      loff_t * offset)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp;

	printk("Sensor Input List: \n");
	mutex_lock(&sensor_input_lock);
	list_for_each_entry_safe(sensor, tmp, &sensors, list) {
		printk("\tname: %s\tpower:%d\n",
		       sensor->name, sensor->on ? 1 : 0);
	}
	mutex_unlock(&sensor_input_lock);

	return 0;
}

static ssize_t sensor_input_write_proc(struct file *file,
				       const char __user * buffer,
				       size_t count, loff_t * offset)
{
	static char input[PAGE_SIZE];
	int power, i = 0;
	char name[40];
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp;

	if (copy_from_user(input, buffer, PAGE_SIZE))
		return -EFAULT;

	input[PAGE_SIZE - 1] = 0;
	i = sscanf(input, "%s %d", name, &power);

	if (i == 2) {
		mutex_lock(&sensor_input_lock);
		list_for_each_entry_safe(sensor, tmp, &sensors, list) {
			if (!strcmp(name, sensor->name)) {
				sensor->on = power;
				if (power) {
					sensor->thread_exit = 0;
					kernel_thread(sensor_input_kthread,
						      sensor, 0);
					printk(KERN_INFO
					       "name %s\tpower: %d\n",
					       sensor->name, power);
				} else {
					//init_completion(&sensor->thread_exit_complete);
					sensor->thread_exit = 1;
					wake_up(&sensor->wait);
					//wait_for_completion(&sensor->thread_exit_complete);
					kthread_stop(sensor->thread_task);
				}
			}
		}
		mutex_unlock(&sensor_input_lock);
	}

	return count;
}

static struct file_operations sensor_input_proc_ops = {
	.read = sensor_input_read_proc,
	.write = sensor_input_write_proc,
};

static void create_sensor_input_proc_file(void)
{
	struct proc_dir_entry *sensor_input_proc_file =
	    create_proc_entry("driver/sensor-input", 0644, NULL);

	if (sensor_input_proc_file) {
		sensor_input_proc_file->proc_fops = &sensor_input_proc_ops;
	} else
		printk(KERN_INFO "proc file create failed!\n");
}

extern struct proc_dir_entry proc_root;
static void remove_sensor_input_proc_file(void)
{
	remove_proc_entry("driver/sensor-input", &proc_root);
}
#endif
static int __devinit sensor_input_probe(struct platform_device *pdev)
{
	int err;

	if (sensor_input_idev)
		return -EINVAL;

	sensor_input_idev = input_allocate_device();
	if (!sensor_input_idev)
		return -ENOMEM;

	sensor_input_idev->name = "sensor-input";
	sensor_input_idev->phys = "sensor-input/input0";
	sensor_input_idev->open = sensor_input_open;
	sensor_input_idev->close = sensor_input_close;

#ifdef CONFIG_INPUT_MERGED_SENSORS
	/* used as orientation sensor */
	__set_bit(EV_ABS, sensor_input_idev->evbit);
	__set_bit(ABS_RX, sensor_input_idev->absbit);
	__set_bit(ABS_RY, sensor_input_idev->absbit);
	__set_bit(ABS_RZ, sensor_input_idev->absbit);

	/* used as  acceleration sensor */
	__set_bit(ABS_X, sensor_input_idev->absbit);
	__set_bit(ABS_Y, sensor_input_idev->absbit);
	__set_bit(ABS_Z, sensor_input_idev->absbit);

	/* used as raw data */
	__set_bit(ABS_HAT1X, sensor_input_idev->absbit);
	__set_bit(ABS_HAT2X, sensor_input_idev->absbit);
	__set_bit(ABS_HAT3X, sensor_input_idev->absbit);
	__set_bit(ABS_MISC, sensor_input_idev->absbit);

	__set_bit(ABS_PRESSURE, sensor_input_idev->absbit);
	__set_bit(ABS_DISTANCE, sensor_input_idev->absbit);

	/* used as keyboard */
	__set_bit(EV_KEY, sensor_input_idev->evbit);
	__set_bit(EV_REP, sensor_input_idev->evbit);
	__set_bit(KEY_SEND, sensor_input_idev->keybit);
	__set_bit(KEY_3, sensor_input_idev->keybit);
	__set_bit(KEY_RIGHTCTRL, sensor_input_idev->keybit);
	__set_bit(KEY_HOME, sensor_input_idev->keybit);

	__set_bit(EV_SYN, sensor_input_idev->evbit);
#endif
	err = input_register_device(sensor_input_idev);
	if (err) {
		printk(KERN_ERR "register input driver error\n");
		input_free_device(sensor_input_idev);
		sensor_input_idev = NULL;
		return err;
	}

	input_set_drvdata(sensor_input_idev, NULL);

#ifdef	CONFIG_PROC_FS
	create_sensor_input_proc_file();
#endif
	return 0;
}

static int sensor_input_remove(struct platform_device *pdev)
{
	if (!list_empty(&sensors))
		return -EBUSY;
	input_unregister_device(sensor_input_idev);
	sensor_input_idev = NULL;

#ifdef	CONFIG_PROC_FS
	remove_sensor_input_proc_file();
#endif
	return 0;
}

static struct platform_driver sensor_input_driver = {
	.probe = sensor_input_probe,
	.remove = sensor_input_remove,
	.suspend = sensor_input_suspend,
	.resume = sensor_input_resume,
	.driver = {
		   .name = "sensor_input",
		   .owner = THIS_MODULE,
		   },
};

int __init sensor_input_init(void)
{
	return platform_driver_register(&sensor_input_driver);
}

void __exit sensor_input_exit(void)
{
	platform_driver_unregister(&sensor_input_driver);
}
/* active-- rw*/
static ssize_t store_active(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	int active;
	int ret;
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d", &active);
	if(ret == 1) {
		mutex_lock(&sensor_input_lock);
		if (active == 0) {
			if(idev->on){
				idev->on=0;
				idev->poweroff();
				printk(KERN_NOTICE "gsensor: de-active sensor!\n");
			}
		} else {
			if(!idev->on){
				idev->poweron();
				idev->on=1;
				printk(KERN_NOTICE "gsensor: active sensor!\n");
			}
		}
		mutex_unlock(&sensor_input_lock);
		wake_up(&idev->wait);
	} else {
		printk(KERN_ERR "sensor: invalid parameter into file!\n");
	}
	return count;
}
static ssize_t show_active(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);
	len += sprintf(buf+len, "%s\t%d\n", idev->name, idev->on);
	return len;
}

/* interval-- rw*/
static ssize_t store_interval(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	int delay;
	int ret;
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);

	ret = sscanf(buf, "%d", &delay);
	if(ret == 1) {
		if(delay<50)
			delay = 50;
		idev->delay = delay;
		wake_up(&idev->wait);
		printk(KERN_INFO "sensor: set %s delay to %dms\n", idev->name, delay);
	}
	return count;
}
static ssize_t show_interval(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);
	len += sprintf(buf+len, "%s\t%d\n", idev->name, idev->delay);
	return len;
}

/* wake-- wo*/
static ssize_t store_wake(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);

	/* report ABS_MISC event to sensor-hal layer, no matter what value store*/
	input_report_abs(idev->dev, ABS_MISC, 1);
	return count;
}

/* data-- ro*/
static ssize_t show_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	struct sensor_input_dev* idev;
	idev = dev_get_drvdata(dev);
	len += sprintf(buf+len, "%s: x:0x%x, y:0x%x, z:0x%x\n", idev->name, idev->x, idev->y, idev->z);
	return len;
}

/* status-- ro*/
static ssize_t show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	/* TODO*/
	return 0;
}

static DEVICE_ATTR(active, S_IRUGO | S_IWUGO, show_active, store_active);
static DEVICE_ATTR(interval, S_IRUGO | S_IWUGO, show_interval, store_interval);
static DEVICE_ATTR(wake, S_IWUGO, NULL, store_wake);
static DEVICE_ATTR(data, S_IRUGO, show_data, NULL);
static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static int sensor_create_sysfs(struct input_dev *idev, int type)
{
	int rc;
	rc = device_create_file(&idev->dev, &dev_attr_active);
	if (rc)
		goto err;

	rc = device_create_file(&idev->dev, &dev_attr_interval);
	if (rc)
		goto err_interval;

	rc = device_create_file(&idev->dev, &dev_attr_wake);
	if (rc)
		goto err_wake;

	rc = device_create_file(&idev->dev, &dev_attr_data);
	if (rc)
		goto err_data;
	/* g-sensor doesn't need status file node*/
	if (type != INPUT_G_SENSOR) {
		rc = device_create_file(&idev->dev, &dev_attr_status);
		if (rc)
			goto err_status;
	}
	return 0;

err_status:
	device_remove_file(&idev->dev, &dev_attr_data);
err_data:
	device_remove_file(&idev->dev, &dev_attr_wake);
err_wake:
	device_remove_file(&idev->dev, &dev_attr_interval);
err_interval:
	device_remove_file(&idev->dev, &dev_attr_active);
err:
	printk(KERN_ERR "sensor: failed to create sysfs!\n");
	return rc;
}
static int sensor_remove_sysfs(struct input_dev *idev, int type)
{

	device_remove_file(&idev->dev, &dev_attr_active);
	device_remove_file(&idev->dev, &dev_attr_interval);
	device_remove_file(&idev->dev, &dev_attr_wake);
	device_remove_file(&idev->dev, &dev_attr_data);

	/* g-sensor doesn't need status file node*/
	if (type != INPUT_G_SENSOR) {
		device_remove_file(&idev->dev, &dev_attr_status);
	}
	return 0;
}

int sensor_input_add(int type, char *name,
		     void (*report) (struct input_dev *),
		     void (*exit) (struct input_dev *),
		     void (*poweron) (void), void (*poweroff) (void))
{
	int err;
	struct sensor_input_dev *sensor =
	    kzalloc(sizeof(struct sensor_input_dev), GFP_KERNEL);

	struct input_dev *sensor_input_idev_ext;

	printk(KERN_NOTICE "sensor: add device of %s\n", name);
	sensor->dev = NULL;
	sensor->name = name;
	sensor->report = report;
	sensor->exit = exit;
	sensor->poweron = poweron;
	sensor->poweroff = poweroff;
	sensor->delay = DEFAULT_POLLING_DELAY;
	sensor->type = type;
	init_waitqueue_head(&sensor->wait);
	mutex_lock(&sensor_input_lock);
	list_add(&sensor->list, &sensors);
#ifdef CONFIG_INPUT_MERGED_SENSORS
	if (sensor_input_usage != 0) {
		sensor->poweron();
		sensor->on = 1;
		sensor->thread_exit = 0;
		kernel_thread(sensor_input_kthread, sensor, 0);
	}
#endif
	mutex_unlock(&sensor_input_lock);

#ifdef CONFIG_INPUT_MERGED_SENSORS
	sensor->dev = sensor_input_idev;
#else
	sensor_input_idev_ext = input_allocate_device();
	if (!sensor_input_idev_ext)
		return -ENOMEM;

	sensor_input_idev_ext->phys = "sensor-input/input0";
	sensor_input_idev_ext->open = sensor_input_open;
	sensor_input_idev_ext->close = sensor_input_close;
	sensor_input_idev_ext->name = name;

	if (type == INPUT_G_SENSOR) {
		/* used as  acceleration sensor */
		__set_bit(EV_ABS, sensor_input_idev_ext->evbit);
		__set_bit(ABS_X, sensor_input_idev_ext->absbit);
		__set_bit(ABS_Y, sensor_input_idev_ext->absbit);
		__set_bit(ABS_Z, sensor_input_idev_ext->absbit);
#if 1
		input_set_abs_params(sensor_input_idev_ext, ABS_X, -100000,
				     100000, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_Y, -100000,
				     100000, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_Z, -100000,
				     100000, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_PRESSURE,
				     0, 255, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_TOOL_WIDTH,
				     0, 15, 0, 0);
#endif
		input_set_drvdata(sensor_input_idev_ext, sensor);
		err = input_register_device(sensor_input_idev_ext);
		if (err) {
			printk(KERN_ERR "register input driver error\n");
			input_free_device(sensor_input_idev_ext);
			sensor_input_idev_ext = NULL;
			return err;
		}

	} else if (type == INPUT_GYRO_SENSOR) {
		/* used as orientation sensor */
		__set_bit(EV_ABS, sensor_input_idev_ext->evbit);

		__set_bit(ABS_RX, sensor_input_idev_ext->absbit);
		__set_bit(ABS_RY, sensor_input_idev_ext->absbit);
		__set_bit(ABS_RZ, sensor_input_idev_ext->absbit);
#if 0
		/* used as keyboard */
		__set_bit(EV_KEY, sensor_input_idev_ext->evbit);
		__set_bit(EV_REP, sensor_input_idev_ext->evbit);
		__set_bit(KEY_SEND, sensor_input_idev_ext->keybit);
		__set_bit(KEY_3, sensor_input_idev_ext->keybit);
		__set_bit(KEY_RIGHTCTRL, sensor_input_idev_ext->keybit);
		__set_bit(KEY_HOME, sensor_input_idev_ext->keybit);

		__set_bit(EV_SYN, sensor_input_idev_ext->evbit);

		/* used as raw data */
		__set_bit(ABS_HAT1X, sensor_input_idev_ext->absbit);
		__set_bit(ABS_HAT2X, sensor_input_idev_ext->absbit);
		__set_bit(ABS_HAT3X, sensor_input_idev_ext->absbit);
		__set_bit(ABS_MISC, sensor_input_idev_ext->absbit);

		input_set_abs_params(sensor_input_idev_ext, ABS_RX,
				     -100000, 100000, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_RY,
				     -100000, 100000, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_RZ,
				     -100000, 100000, 0, 0);
#endif
		input_set_drvdata(sensor_input_idev_ext, sensor);
		err = input_register_device(sensor_input_idev_ext);
		if (err) {
			printk(KERN_ERR "register input driver error\n");
			input_free_device(sensor_input_idev_ext);
			sensor_input_idev_ext = NULL;
			return err;
		}

	} else if (type == INPUT_AMBIENT_SENSOR) {
		/*used as light sensor */
		__set_bit(EV_ABS, sensor_input_idev_ext->evbit);
		__set_bit(ABS_PRESSURE, sensor_input_idev_ext->absbit);

		input_set_abs_params(sensor_input_idev_ext, ABS_PRESSURE,
				     -100000, 100000000, 0, 0);

		input_set_drvdata(sensor_input_idev_ext, sensor);
		err = input_register_device(sensor_input_idev_ext);
		if (err) {
			printk(KERN_ERR "register input driver error\n");
			input_free_device(sensor_input_idev_ext);
			sensor_input_idev_ext = NULL;
			return err;
		}

	} else if (type == INPUT_PROXIMITY_SENSOR) {
		/*used as light sensor */
		__set_bit(EV_ABS, sensor_input_idev_ext->evbit);
		__set_bit(ABS_DISTANCE, sensor_input_idev_ext->absbit);
		__set_bit(ABS_MISC, sensor_input_idev_ext->absbit);
#if 1
		input_set_abs_params(sensor_input_idev_ext, ABS_DISTANCE,
				     0, 65535, 0, 0);
		input_set_abs_params(sensor_input_idev_ext, ABS_MISC,
				     0, 65535, 0, 0);
#endif
		input_set_drvdata(sensor_input_idev_ext, sensor);
		err = input_register_device(sensor_input_idev_ext);
		if (err) {
			printk(KERN_ERR "register input driver error\n");
			input_free_device(sensor_input_idev_ext);
			sensor_input_idev_ext = NULL;
			return err;
		}

	} else {
		input_free_device(sensor_input_idev_ext);
		sensor_input_idev_ext = NULL;
		return -1;
	}
	sensor->dev = sensor_input_idev_ext;
#endif
	sensor_create_sysfs(sensor_input_idev_ext, type);
	return 0;
}

void sensor_input_del(char *name)
{
	struct sensor_input_dev *sensor;
	struct sensor_input_dev *tmp;

	mutex_lock(&sensor_input_lock);
	list_for_each_entry_safe(sensor, tmp, &sensors, list) {
		if (!strcmp(name, sensor->name)) {
			if (sensor->count != 0)
				return;
			sensor_remove_sysfs(sensor->dev,sensor->type);
			input_unregister_device(sensor->dev);
			list_del(&sensor->list);
			kfree(sensor);
			return;
		}
	}
	mutex_unlock(&sensor_input_lock);
	printk(KERN_ERR "Try to remove a unknow sensor: %s\n", name);
}

MODULE_DESCRIPTION("Sensor Input driver");
MODULE_AUTHOR("Bin Yang <bin.yang@marvell.com>");
MODULE_LICENSE("GPL");

module_init(sensor_input_init);
module_exit(sensor_input_exit);
