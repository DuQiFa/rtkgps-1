/******************************************************************************

    Copyright © 2008 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 15 December 2008

******************************************************************************/

#ifndef _GPSFMT_H
#define _GPSFMT_H

#include <stdio.h>
#include <stdint.h>
#include "rtkcom.h"

typedef struct {
  uint16_t nlat;
  uint16_t nlng;
  float latmin;
  float latstp;
  float latmax;
  float lngmin;
  float lngstp;
  float lngmax;
  float qscale;
  void *filep;
  int16_t *gridp;
} geoid_height_t;


void print_hdr_nmea(FILE *stream, const char* btas);
void print_log_nmea(FILE *stream, const logfile_t *lfp, const gps_fix_t *fxp,
		    const float* gcp);
void print_hdr_native(FILE *stream);
void print_log_native(FILE *stream, const logfile_t *lfp, 
		      const gps_fix_t *fxp, const float *gcp);

int geoid_calc_open(const char *fnam, geoid_height_t *gdhtp);
float geoid_calc_correction(const geoid_height_t *gdhtp, float lat, float lng);
int geoid_calc_close(geoid_height_t *gdhtp);

#endif
