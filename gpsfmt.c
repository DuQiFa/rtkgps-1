/******************************************************************************

    Copyright © 2008-2012 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 4 February 2012

******************************************************************************/

#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "gpsfmt.h"


/*****************************************************************************
 Convert radians to degrees and seconds.
 *****************************************************************************/
double radtodegsec(double rad) {
  const double dgrd = 360.0/(2*M_PI);
  double deg, d, s;
  
  deg = dgrd*rad;
  d = (deg>=0?1.0:-1.0)*floor(fabs(deg));
  s = (deg - d) * 60.0;
  return 100.0*d + s;
}


/*****************************************************************************
 Round floating point value to 1 decimal place.
 *****************************************************************************/
float round1p(float x) {
  return roundf(10.0f*x)/10.0f;
}


/*****************************************************************************
 Print header for nmea log file.
 *****************************************************************************/
void print_hdr_nmea(FILE *stream, const char* btas) {
  char hstr[256] = {0};
  char bs[13] = {0};

  if (btas != NULL) {
    char bc[6][3] = {{0}};
    
    if (sscanf(btas, "%2s:%2s:%2s:%2s:%2s:%2s", bc[0], bc[1], bc[2], 
	       bc[3], bc[4], bc[5]) == 6)
      sprintf(bs, "%2s%2s%2s%2s%2s%2s", bc[0], bc[1], bc[2], bc[3], 
	      bc[4], bc[5]);
  }

  if (bs[0] == 0)
    sprintf(hstr, "$PRTK,RTKGPS,%s*", PACKAGE_VERSION);
  else 
    sprintf(hstr, "$PRTK,RTKGPS,%s,%12s*", PACKAGE_VERSION, bs);

  fprintf(stream, "%s%02X\r\n", hstr, string_checksum(hstr));
}


/*****************************************************************************
 Print log data fxp for file lgfl to the indicated output stream.
 *****************************************************************************/
void print_log_nmea(FILE *stream, const logfile_t *lfp, const gps_fix_t *fxp,
		    const float *gcp) {
  int n;
  char *pgga;
  char gga[256];
  char *prmc;
  char rmc[256];
  char time[32];
  char alt[32];
  char vel[16];
  char ltd, lnd;
  double lat, lon;

  for (n = 0; n < lfp->nfix; n++) {
    sprintf(time, "%02d%02d%02d.00", fxp[n].hour, fxp[n].min, fxp[n].sec);
    lat = fabs(radtodegsec(fxp[n].lat));
    ltd = (fxp[n].lat >= 0)?'N':'S';
    lon = fabs(radtodegsec(fxp[n].lng));
    lnd = (fxp[n].lng < 0)?'W':'E';
    if (lfp->fxtyp > 0) {
      if (gcp != NULL)
	sprintf(alt, "%.1f,M,%.1f,M", round1p(fxp[n].alt-gcp[n]),
		round1p(gcp[n]));
      else
	sprintf(alt, "%.1f,M,,", round1p(fxp[n].alt));
    } else {
      sprintf(alt, ",,,");
    }
    if (lfp->fxtyp > 1)
      sprintf(vel, "%06.2f", 0.539956803f*fxp[n].vel);
    else
      vel[0] = '\0';

    pgga = (fxp[n].unkwn == 0)?"$GPGGA":"$PRTK,BADFIX,GPGGA";
    prmc = (fxp[n].unkwn == 0)?"$GPRMC":"$PRTK,BADFIX,GPRMC";

    sprintf(gga, "%s,%s,%09.4f,%c,%010.4f,%c,1,,,%s,,*", 
	    pgga, time, lat, ltd, lon, lnd, alt);
    fprintf(stream, "%s%02X\r\n", gga, string_checksum(gga));

    sprintf(rmc, "%s,%s,A,%09.4f,%c,%010.4f,%c,%s,,%2.2s%2.2s%2.2s,,,*",
	    prmc, time, lat, ltd, lon, lnd, vel, lfp->date+6, lfp->date+4, 
	    lfp->date+2);
    fprintf(stream, "%s%02X\r\n", rmc, string_checksum(rmc));
  }
}


/*****************************************************************************
 Print header for native-format log file.
 *****************************************************************************/
void print_hdr_native(FILE *stream) {
  fprintf(stream, "RNGL\n");
}


/*****************************************************************************
 Print log file content in native form.
 *****************************************************************************/
void print_log_native(FILE *stream, const logfile_t *lfp,
		      const gps_fix_t *fxp, const float *gcp) {
  int n;
  
  fprintf(stream, "%s %d %d\n", lfp->date, lfp->fxtyp, lfp->nfix);
  for (n = 0; n < lfp->nfix; n++) {
    fprintf(stream, "%02d%02d%02d,%+.12e,%+.12e", fxp[n].hour, fxp[n].min, 
	    fxp[n].sec,fxp[n].lat,fxp[n].lng);
    if (lfp->fxtyp > 0) {
      fprintf(stream, ",%+.8e", fxp[n].alt);
      if (gcp != NULL)
	fprintf(stream, ",%+.3e", gcp[n]);
      else
	fprintf(stream, " %10s", " ");
    }
    if (lfp->fxtyp > 1)
      fprintf(stream, ",%+.8e", fxp[n].vel);
    fprintf(stream, "\n");
  }
}


/*****************************************************************************
 Initialise geoid calculation structure.
 *****************************************************************************/
int geoid_calc_open(const char *fnam, geoid_height_t *gdhtp) {
  int fd;

  gdhtp->filep = NULL;
  gdhtp->gridp = NULL;
  if ((fd = open(fnam, O_RDONLY)) == -1)
    return -1;

  if (read(fd, gdhtp, 2*sizeof(uint16_t)+7*sizeof(float)) == -1)
    return -1;

#ifdef WORDS_BIGENDIAN
  /* If necessary, swap endianness of header values */
  bswap_16(*(__uint16_t*)(char*)&gdhtp->nlat);
  bswap_16(*(__uint16_t*)(char*)&gdhtp->nlng);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->latmin);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->latstp);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->latmax);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->lngmin);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->lngstp);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->lngmax);
  bswap_32(*(__uint32_t*)(char*)&gdhtp->qscale);
#endif

  gdhtp->filep = mmap(NULL, 2*sizeof(uint16_t)+7*sizeof(float) + 
		      gdhtp->nlat*gdhtp->nlng*sizeof(int16_t),
		      PROT_READ, MAP_PRIVATE, fd, 0);

  if (gdhtp->filep == MAP_FAILED) {
    gdhtp->filep = NULL;
    return -1;
  }

  if (close(fd))
    return -1;

  gdhtp->gridp = (int16_t *)((char *)gdhtp->filep + 
			     2*sizeof(uint16_t)+7*sizeof(float));
  return 0;
}


/*****************************************************************************
 Compute geoid correction with respect to WGS84 ellipsoid at specified
 location.
 *****************************************************************************/
float geoid_calc_correction(const geoid_height_t *gdhtp, float lat,float lng) {
  float slat, slng, x, y, x1, y1, g00, g01, g10, g11;
  unsigned short ilat0, ilng0, ilat1, ilng1;

  if (lat < gdhtp->latmin || lat > gdhtp->latmax)
    return 0.0/0.0;
  if (lng < gdhtp->lngmin || lng > gdhtp->lngmax)
    return 0.0/0.0;
  
  slat = (lat - gdhtp->latmin)/gdhtp->latstp;
  if ((ilat0 = floorf(slat + 0.5f)) == slat)
    ilat1 = ilat0;
  else {
    ilat0 = floorf(slat);
    ilat1 = ceilf(slat);
  }
  y = slat - ilat0;
  y1 = 1.0f - y;
  slng = (lng - gdhtp->lngmin)/gdhtp->lngstp;
  if ((ilng0 = floorf(slng + 0.5f)) == slng)
    ilng1 = ilng0;
  else {
    ilng0 = floorf(slng);
    ilng1 = ceilf(slng);
  }
  x = slng - ilng0;
  x1 = 1.0f - x;

#ifdef WORDS_BIGENDIAN
  {
    int16_t q00, q01, q10, q11;
    q00 = gdhtp->gridp[ilng0*gdhtp->nlat + ilat0];
    q01 = gdhtp->gridp[ilng1*gdhtp->nlat + ilat0];
    q10 = gdhtp->gridp[ilng0*gdhtp->nlat + ilat1];
    q11 = gdhtp->gridp[ilng1*gdhtp->nlat + ilat1];
    bswap_16(*(__int16_t*)(char*)&q00);
    bswap_16(*(__int16_t*)(char*)&q01);
    bswap_16(*(__int16_t*)(char*)&q10);
    bswap_16(*(__int16_t*)(char*)&q11);
    g00 = q00/gdhtp->qscale;
    g01 = q01/gdhtp->qscale;
    g10 = q10/gdhtp->qscale;
    g11 = q11/gdhtp->qscale;
  }
#else
  g00 = (gdhtp->gridp[ilng0*gdhtp->nlat + ilat0])/gdhtp->qscale;
  g01 = (gdhtp->gridp[ilng1*gdhtp->nlat + ilat0])/gdhtp->qscale;
  g10 = (gdhtp->gridp[ilng0*gdhtp->nlat + ilat1])/gdhtp->qscale;
  g11 = (gdhtp->gridp[ilng1*gdhtp->nlat + ilat1])/gdhtp->qscale;
#endif

  return g00*(x1*y1) + g01*(y1*x) + g10*(y*x1) + g11*(x*y);
}


/*****************************************************************************
 Destroy geoid calculation structure.
 *****************************************************************************/
int geoid_calc_close(geoid_height_t *gdhtp) {
  if (gdhtp->filep != NULL) {
    if (munmap(gdhtp->filep, 2*sizeof(uint16_t)+7*sizeof(float) + 
	       gdhtp->nlat*gdhtp->nlng*sizeof(int16_t)) == -1)
      return -1;
    gdhtp->filep = NULL;
  }
  gdhtp->gridp = NULL;
  return 0;
}
