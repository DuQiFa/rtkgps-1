/******************************************************************************

    Copyright © 2008,2009 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 30 May 2009

******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#if ENABLE_LINUX_BT-0
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif /* ENABLE_LINUX_BT */
#include <errno.h>
#include "serial.h"

#ifndef HAVE_CFMAKERAW
static void cfmakeraw(struct termios *tiosp) {
  tiosp->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
		      |INLCR|IGNCR|ICRNL|IXON);
  tiosp->c_oflag &= ~OPOST;
  tiosp->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  tiosp->c_cflag &= ~(CSIZE|PARENB);
  tiosp->c_cflag |= CS8;
}
#endif

static struct termios origopt;


#if ENABLE_LINUX_BT-0
/*****************************************************************************
 Scan for bluetooth devices, inserting a maximum of mndv entries into btdp.
 *****************************************************************************/
int bt_scan(bt_device_t* btdp, unsigned short mndv) {
  int dvid, sckt, ndv, n;
  inquiry_info *inqp;

  if ((dvid = hci_get_route(NULL)) < 0)
    return -1;
  if ((inqp = (inquiry_info*)malloc(mndv*sizeof(inquiry_info))) == NULL)
    return -1;
  if ((ndv = hci_inquiry(dvid, 8, mndv, NULL, &inqp, IREQ_CACHE_FLUSH)) < 0) {
    free(inqp);
    return -1;
  }

  if ((sckt = hci_open_dev(dvid)) < 0)
    return -1;

  for (n = 0; n < ndv; n++) {
    btdp[n].bdaddr = inqp[n].bdaddr;
    memset(btdp[n].name, 0, 128);
    hci_read_remote_name(sckt, &inqp[n].bdaddr, 127, btdp[n].name, 0);
  }

  close(sckt);
  free(inqp);

  return ndv;
}


/*****************************************************************************
 Print bluetooth address and names of the devices in array btdp to sream strm.
 *****************************************************************************/
int bt_scan_print(FILE *strm,  const bt_device_t* btdp, unsigned short ndv) {
  char baddrstr[24];
  unsigned short int n;
  int p, c = 0;

  for (n = 0; n < ndv; n++) {
    ba2str(&btdp[n].bdaddr, baddrstr);
    p = fprintf(strm, "  %s\t%s\n", baddrstr, btdp[n].name);
    if (p < 0)
      return -1;
    else
      c += p;
  }

  return c;
}


/*****************************************************************************
 Open a connection to bluetooth address addrs on channel chn.
 *****************************************************************************/
int bt_open(const char *addrs, unsigned short chn) {
  bdaddr_t daddr;
  struct sockaddr_rc saddr = {0};
  struct timeval rtmt = {0, 500};
  struct timeval wtmt = {1, 500};
  int sck, sfl;

  if ((sck = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0)
    return -1;

  str2ba(addrs, &daddr);
  saddr.rc_family  = AF_BLUETOOTH;
  saddr.rc_bdaddr  = daddr;
  saddr.rc_channel = chn;

  if (connect(sck, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
    close(sck);
    return -1;
  }

  if ((sfl = fcntl(sck, F_GETFL, 0)) < 0)
    return -1;
  if (fcntl(sck, F_SETFL, sfl | O_NONBLOCK) < 0)
    return -1;

  if (setsockopt(sck, SOL_SOCKET, SO_RCVTIMEO, &rtmt, sizeof(rtmt)) < 0) {
    close(sck);
    return -1;
  }

  if (setsockopt(sck, SOL_SOCKET, SO_SNDTIMEO, &wtmt, sizeof(wtmt)) < 0) {
    close(sck);
    return -1;
  }

  return sck;
}
#endif /* ENABLE_LINUX_BT */


/*****************************************************************************
 Close the connection to socket sck.
 *****************************************************************************/
int bt_close(int sck) {
  return close(sck);
}


/*****************************************************************************
 Gets an OS-specific value that represents a valid serial line speed.
 Returns 0 if the specified line speed is not valid.
 *****************************************************************************/
static speed_t get_speed(unsigned int speed) {
  switch (speed) {
  case 50:
    return B50;
  case 75:
    return B75;
  case 150:
    return B150;
  case 300:
    return B300;
  case 600:
    return B600;
  case 1200:
    return B1200;
  case 2400:
    return B2400;
  case 4800:
    return B4800;
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  }
  return 0; /* invalid */
}

/*****************************************************************************
 Returns whether the specified value represents a valid serial line speed.
 *****************************************************************************/
int dev_speed_valid(unsigned int speed) {
	return 0 != get_speed(speed);
}

/*****************************************************************************
 Sets the speed of a serial connection and configures the connection.
 *****************************************************************************/
int dev_config_serial(int fd, unsigned int speed) {
  struct termios termopt;
  speed_t spd;
  /* initialize with current values (for cygwin) */
  tcgetattr(fd, &termopt);

  spd = get_speed(speed);
  cfsetispeed(&termopt, spd);
  cfsetospeed(&termopt, spd);

  cfmakeraw(&termopt);

  termopt.c_cc[VMIN]  = 0;
  termopt.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSANOW, &termopt) < 0) {
    dev_close(fd);
    return -1;
  }

  if (tcflush(fd, TCIFLUSH) < 0) {
    dev_close(fd);
    return -1;
  }

  return 0; /* success */
}

/*****************************************************************************
 Open a serial connection to device devs.
 *****************************************************************************/
int dev_open(const char *devs) {
 int fd;

  fd = open(devs, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd != -1) {
    tcgetattr(fd, &origopt);
  }
  return fd;
}


/*****************************************************************************
 Close a serial connection and return the device to its pre-connection state.
 *****************************************************************************/
int dev_close(int fd) {
  tcsetattr(fd, TCSANOW, &origopt);
  return close(fd);
}


/*****************************************************************************
 Read at most bsz bytes from file descriptor fd into buffer buf.
 *****************************************************************************/
ssize_t serial_read(int fd, char *buf, size_t bsz, long int tmt) {
  ssize_t b;
  struct timeval tv = {0,100};
  fd_set rfds;
  int slct;

  /* Set up and call select to wait for input on fd */
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);
  tv.tv_usec = tmt % 1000;
  tv.tv_sec = tmt / 1000;
  slct = select(fd+1, &rfds, NULL, NULL, &tv);
  /* Return -1 on select error */
  if (slct == -1)
    return -1;

  /* Return read count of zero if select timed out */
  if (!slct)
    return 0;
  /* Read at most bsz bytes from fd */
  b = read(fd, buf, bsz);

  /* If read returns -1 but errno is EAGAIN, set read count to zero */
  if (b < 0 && errno == EAGAIN)
    b = 0;

  return b;
}


/*****************************************************************************
 Write bsz bytes from buffer buf to from file descriptor fd.
 *****************************************************************************/
ssize_t serial_write(int fd, const char *buf, size_t bsz) {
  return write(fd, buf, bsz);
}


/*****************************************************************************
 Repeat reading from fd until timeout expires or bsz bytes read.
 *****************************************************************************/
ssize_t serial_read_repeat(int fd, char *buf, size_t bsz, long int tmt) {
  size_t n = 0;
  size_t m = bsz;
  ssize_t b;

  do {
    b = serial_read(fd, buf + n, m, tmt);
    if (b < 0)
      return -1;
    m -= b;
    n += b;
  } while (b > 0 && n < bsz);

  return n;
}


/*****************************************************************************
 Read from file descriptor fd, discarding input until string str is
 encountered.
 *****************************************************************************/
ssize_t serial_read_discard(int fd, char *buf, size_t bsz, size_t pos,
			    const char *str, long int tmt) {
  char *s = NULL;
  size_t n = pos;
  size_t m = bsz-1-n;
  size_t sl;
  ssize_t b;

  sl = strlen(str);
  buf[pos] = '\0';

  while ((s = strstr(buf, str)) == NULL) {
    /* If buffer contains more characters than sl (the length of str),
       copy the last sl characters to the start of the buffer and set
       buffer counters. */
    if (n > sl) {
      memmove(buf, buf + n - sl, sl);
      n = sl;
      m = bsz - 1 - n;
    }

    b = serial_read(fd, buf + n, m, tmt);
    if (b < 0)
      return -1;
    if (b == 0)
      return 0;

    /* Insert end of string marker into buffer */
    buf[n + b] = '\0';
    m -= b;
    n += b;
  }

  n -= (s - buf);
  memmove(buf, s, n+1); /* Move n+1 to include end of string marker */

  return n;
}


/*****************************************************************************
 Read from file descriptor fd, recording at most bsz bytes into buffer buf
 from when string bgn is read, and ending when string end is read.
 *****************************************************************************/
ssize_t serial_read_string(int fd, char *buf, size_t bsz, size_t pos,
			   const char *bgn, const char *end, long int tmt) {
  char *s = NULL;
  size_t n = pos;
  size_t m = bsz-1;
  ssize_t b;

  b = serial_read_discard(fd, buf, bsz, n, bgn, tmt);
  if (b < 0)
    return -1;
  if (b == 0)
    return 0;
  n = b;
  m = bsz-1-n;

  /* Read until timeout or string end encountered */
  while (n < bsz && (s = strstr(buf, end)) == NULL) {
    b = serial_read(fd, buf + n, m, tmt);
    if (b < 0)
      return -1;
    if (b == 0) {
      return 0;
    }
    /* Set buffer counters */
    m -= b;
    n += b;

    buf[n] = '\0';
  }

  /* Number of bytes read is zero if end string not encountered */
  if (s == NULL)
    n = 0;

  return n;
}
