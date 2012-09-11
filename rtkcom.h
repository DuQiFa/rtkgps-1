/******************************************************************************

    Copyright © 2008-2010 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 20 February 2010

******************************************************************************/

#ifndef _RTKCOM_H
#define _RTKCOM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "serial.h"

typedef struct {
  short int fxtyp;
  short int unkwn0;
  short int unkwn1;
  short int mfowm;
  short int unkwn2;
  short int sntvl;
  short int gpsrx;
  short int nfile;
        int nfix;
  short int gpsms;
} status_t;

typedef struct {
  char date[9];
  char time[7];
} date_time_t;

typedef struct {
  date_time_t first;
  date_time_t last;
} log_bndry_t;

typedef struct {
  unsigned long nbytes;
  unsigned int sctrsz;
  unsigned int nmsctr; 
} memory_t;

typedef struct {
  char vrsnr[64];
  char frmwr[64];
  char dflbd[64];
  char drvrv[64];
} firmware_t;

typedef struct {
  char date[9];
  short int fxtyp;
  int nfix;
  int memp; 
} logfile_t;

typedef struct __attribute__((packed)) {
  uint8_t unkwn;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  float lat;
  float lng;
  float alt;
  float vel;
  uint32_t dist;
  uint8_t unk1;
  uint8_t nfix;
  uint16_t hdop;
  uint16_t pdop;
  uint16_t vdop;
  uint8_t sat[24];
  float angle;
} gps_fix_t;

typedef enum {
  RCERROR_SYS = 1, RCERROR_PARSE, RCERROR_CHECKSUM, RCERROR_NORSP,
  RCERROR_UNXPRSP, RCERROR_INVLDCMD, RCERROR_MEMALLOC, RCERROR_NULL
} rcerror_t;


extern void (*gdpfp)(unsigned short, unsigned short);

extern rcerror_t rcerrno;
extern int rcerrln;
extern void (*gcwrnfp)(const char *, int, const char *);

const char *gcstrerror(rcerror_t rcerr);

unsigned short fix_size(unsigned short fxtyp);
const char *fxtyp_string(unsigned short fxtyp);
const char *mfowm_string(unsigned short mfowm);
const char *gpsrx_string(unsigned short gpsrx);
const char *gpsms_string(unsigned short gpsms);

char *find_sentence(char *buf, int bsz, const char *prfx); 
uint8_t array_checksum(const char *buf, int bsz);
uint8_t string_checksum(const char *str);
int verify_array_checksum(const char *buf, int bsz);
int verify_string_checksum(const char *str);

int send_cmd(int fd, const char* buf);
char *get_cmd_response(int fd, const char *cmd, const char *pfx, 
		       char *rsp, int rsz);
char *get_sentence(int fd, long int tmt);

int get_status(int fd, status_t *status);
int get_current_utc(int fd, date_time_t *dtp);
int get_log_bndry(int fd, log_bndry_t *lgbp);
int get_memory_info(int fd, memory_t *memp);
int get_firmware_info(int fd, firmware_t *frmp);
int get_file_info(int fd, short int filen, logfile_t *lgfp);
int get_file_start_time(int fd, const logfile_t *lgfp, date_time_t *dtp);

int get_data(int fd, int memp, short int fxtyp, int nfix, 
	     gps_fix_t *gfxp, int nfxt, int nfxb);
int get_file_data(int fd, const logfile_t *lgfp, gps_fix_t *gfxp);

int set_mode(int fd, short int log, short int out);
int set_status(int fd, const status_t *status);
int set_memory_erase(int fd);

void print_bytes(FILE *stream, const char *buf, int bsz);
void disp_fix(short int ftyp, gps_fix_t gfx);

#endif
