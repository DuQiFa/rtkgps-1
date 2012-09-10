/******************************************************************************

    Copyright © 2008,2009 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 29 May 2009

******************************************************************************/

#ifndef _SERIAL_H
#define _SERIAL_H

#include <unistd.h>
#include <stdio.h>
#if ENABLE_LINUX_BT-0
#include <bluetooth/bluetooth.h>

typedef struct {
  bdaddr_t bdaddr;
  char name[128];
} bt_device_t;


int bt_scan(bt_device_t* btdp, unsigned short mndv);
int bt_scan_print(FILE *strm,  const bt_device_t* btdp, unsigned short ndv);
int bt_open(const char *addrs, unsigned short chn);
#endif /* ENABLE_LINUX_BT */

int bt_close(int sck);

int dev_speed_valid(unsigned int speed);
int dev_open(const char *devs);
int dev_config_serial(int fd, unsigned int speed);
int dev_close(int fd);

ssize_t serial_read(int fd, char *buf, size_t bsz, long int tmt);
ssize_t serial_write(int fd, const char *buf, size_t bsz);

ssize_t serial_read_repeat(int fd, char *buf, size_t bsz, long int tmt);
ssize_t serial_read_discard(int fd, char *buf, size_t bsz, size_t pos,
			    const char *str, long int tmt);
ssize_t serial_read_string(int fd, char *buf, size_t bsz,  size_t pos,
			   const char *bgn, const char *end, long int tmt);

#endif
