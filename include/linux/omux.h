/* 
 * drivers/omux/omux.h 
 *
 * Support Open MUX Framework.
 *
 * Copyright (C) 2009 Borqs, Inc.
 * Author: Emichael Li <emichael.li@borqs.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _OMUX_H_
#define _OMUX_H_

#define TS0710_MTU  				(4096)
#define TS0710_MAX_HDR_SIZE 		(5)
#define TS0710MUX_SEND_BUF_OFFSET 	(10)

struct omux_device {
	struct list_head list;
	int magic;
	char name[32];
	int minor;
	int refcount;

	int (*omux_tx_buf) (int minor, unsigned char *buf, ssize_t len);
	ssize_t (*chars_in_tx_buffer) (void);

	struct module *owner;
};

struct omux_driver {
	struct list_head list;
	int magic;
	char *name;
	int refcount;

	int (*rx_read_buf) (unsigned char *buf, ssize_t len);
	int (*rx_handler)(void);
	struct module *owner;
};

extern int omux_tx_buf(unsigned char *buf, ssize_t len);
extern int omux_rx_read_buf(unsigned char *buf, ssize_t len);
extern int omux_rx_handler(void);
extern int omux_chars_in_tx_buffer(void);

extern int omux_register_device(struct omux_device *device);
extern int omux_unregister_device(struct omux_device *device);

extern int omux_register_driver(struct omux_driver *driver);
extern int omux_unregister_driver(struct omux_driver *driver);

#define OMUX_ATTR(_name) \
static struct kobj_attribute _name##_attr = {   \
        .attr   = {                             \
                .name = __stringify(_name),     \
                .mode = 0664,                   \
        },                                      \
        .show   = _name##_show,                 \
        .store  = _name##_store,                \
}

extern int omux_sysfs_create_group(struct attribute_group *g);
extern void omux_sysfs_remove_group(struct attribute_group *g);

#endif /* _OMUX_H_ */
