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

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#ifdef WORDS_BIGENDIAN
#include <byteswap.h>
#endif
#include <math.h>
#include "rtkcom.h"

void (*gdpfp)(unsigned short, unsigned short) = NULL;

rcerror_t rcerrno = RCERROR_NULL;
int rcerrln = -1;
void (*gcwrnfp)(const char *, int, const char *) = NULL;


/*****************************************************************************
 Return the error message string corresponding to the error number argument.
 *****************************************************************************/
const char *gcstrerror(rcerror_t rcerr) {
  char *errmsg = "\0";

  switch (rcerr) {
  case RCERROR_SYS:  errmsg = strerror(rcerr);
    break;
  case RCERROR_PARSE: errmsg = "Error parsing logger response";
    break;
  case RCERROR_CHECKSUM: errmsg = "Checksum error";
    break;
  case RCERROR_NORSP: errmsg = "No response from logger";
    break;
  case RCERROR_UNXPRSP: errmsg = "Unexpected response from logger";
    break;
  case RCERROR_INVLDCMD: errmsg = "Error in command received by logger";
    break;
  case RCERROR_MEMALLOC:  errmsg = "Memory allocation error";
    break;
  default:
    errmsg = "Invalid gpscomm error number";
  }

#ifdef DEBUG
  {
    static char errstr[512];
    sprintf(errstr, "%s at line %d in file %s", errmsg, rcerrln, __FILE__);
    return errstr;
  }
#else
  return errmsg;
#endif
}


/*****************************************************************************
 Return the number of bytes in the location record for the specified fix type.
 *****************************************************************************/
unsigned short fix_size(unsigned short fxtyp) {
  static uint8_t fxsz[5] = { 12, 16, 20, 24, 60 };

  if (fxtyp < 5)
    return fxsz[fxtyp];
  else
    return 0;
}


/*****************************************************************************
 Return a string description corresponding to the fix type argument.
 *****************************************************************************/
const char *fxtyp_string(unsigned short fxtyp) {
  switch (fxtyp) {
  case 0: return "Time,Lat,Lng";
    break;
  case 1: return "Time,Lat,Lng,Alt";
    break;
  case 2: return "Time,Lat,Lng,Alt,Vel";
    break;
  case 3: return "Time,Lat,Lng,Alt,Vel,Dist";
    break;
  case 4: return "Time,Lat,Lng,Alt,Vel,Dist,Stat";
    break;
  default: return "Invalid";
  }
}


/*****************************************************************************
 Return a string description corresponding to the memory full mode argument.
 *****************************************************************************/
const char *mfowm_string(unsigned short mfowm) {
  switch (mfowm) {
  case 0: return "Overwrite";
    break;
  case 1: return "Stop";
    break;
  default: return "Invalid";
  }
}


/*****************************************************************************
 Return a string description corresponding to the GPS receive argument.
 *****************************************************************************/
const char *gpsrx_string(unsigned short gpsrx) {
  switch (gpsrx) {
  case 0: return "Not receiving/no fix";
    break;
  case 192: return "Receiving/fix";
    break;
  default: return "Invalid";
  }
}


/*****************************************************************************
 Return a string description corresponding to the GPS mouse mode argument.
 *****************************************************************************/
const char *gpsms_string(unsigned short gpsms) {
  switch (gpsms) {
  case 0: return "Disabled";
    break;
  case 1: return "Enabled";
    break;
  default: return "Invalid";
  }
}


/*****************************************************************************
 Find the sentence starting with prfx in the buffer buf with maximum size bsz.
 *****************************************************************************/
char *find_sentence(char *buf, int bsz, const char *prfx) {
  char *bp = NULL, *ep = NULL;

  buf[bsz-1] = '\0';
  if (prfx == NULL)
    bp = strstr(buf, "$");
  else
    bp = strstr(buf, prfx);
  if (bp == NULL)
    return NULL;

  ep = strstr(bp,"\r\n");
  if (ep == NULL)
    return NULL;

  if (ep+2-buf < bsz)
    ep[2] = '\0';
  else
    return NULL;

  return bp;
}


/*****************************************************************************
 Compute the NMEA checksum for the bsz length content of buf.
 *****************************************************************************/
uint8_t array_checksum(const char *buf, int bsz) {
  uint8_t b = 0;
  int n;

  /*printf("Checksum str: |%s|%c|%c|\n", buf, buf[0], buf[bsz-1]);*/

  for (n = 0; n < bsz; n++) {
      b ^= (uint8_t)buf[n];
  }
  return b;
}


/*****************************************************************************
 Compute the NMEA checksum for the string str.
 *****************************************************************************/
uint8_t string_checksum(const char *str) {
  const char *cp = str;
  int sl = 0;

  if (cp[0] == '$')
    cp++;
  sl = strlen(cp);
  if (cp[sl-1] == '*')
    sl--;
  return array_checksum(cp, sl);
}


/*****************************************************************************
 Verify the checksum of the bsz length (including checksum) content of buf.
 *****************************************************************************/
int verify_array_checksum(const char *buf, int bsz) {
  unsigned int csc, cse;

  if (sscanf(buf+bsz-2, "%02X", &cse) < 1) {
#ifdef DEBUG
    fprintf(stderr, "\n=== Checksum Error  Could not scan explicit checksum"
	    " in string %.2s\n", buf+bsz-2);
    return 0;
#endif
  }
  csc = array_checksum(buf+1, bsz-4);

#ifdef DEBUG
  if (csc != cse)
    fprintf(stderr,"\n=== Checksum Error  Explicit: %02X  Computed: %02X\n",
	    cse, csc);
#endif

  return (csc == cse)?1:0;
}


/*****************************************************************************
 Verify the checksum of the string str.
 *****************************************************************************/
int verify_string_checksum(const char *str) {
  char *cp;

  cp = strchr(str, '*');
  return (cp == NULL)?0:verify_array_checksum(str, cp-str+3);
}


/*****************************************************************************
 Write command str to file descriptor fd.
 *****************************************************************************/
int send_cmd(int fd, const char* str) {
  char *buf;
  int sln, chk, wn;

  sln = strlen(str);
  chk = array_checksum(str, sln);
  if (chk < 0 || chk > 255) {
    rcerrno = RCERROR_CHECKSUM;
    rcerrln = __LINE__;
    return -1;
  }
  buf = malloc(sln + 5);
  if (buf == NULL) {
    rcerrno = RCERROR_MEMALLOC;
    rcerrln = __LINE__;
    return -1;
  }
  sprintf(buf, "%s%02X\r\n", str, chk);

#ifdef DEBUG
  fprintf(stderr, ">>> %s",buf);
#endif

  wn = serial_write(fd, buf, sln+4);
  if (wn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    free(buf);
    return -1;
  }
  free(buf);

  return (wn == sln+4)?wn:-1;
}


/*****************************************************************************
 Write command cmnd to file descriptor fd and read response.
 *****************************************************************************/
char *get_cmd_response(int fd, const char *cmd, const char *pfx,
		       char *rsp, int rsz) {
  short int wn, rn;

  wn = send_cmd(fd, cmd);
  if (wn < 0)
    return NULL;
  rn = serial_read_string(fd, rsp, rsz, 0, pfx, "\r\n", 2000);

  if (rn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return NULL;
  }
  if (rn == 0 && rsp[0] == '\0') {
    rcerrno = RCERROR_NORSP;
    rcerrln = __LINE__;
    return NULL;
  }
  if (rn == 0 && rsp[0] != '\0') {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return NULL;
  }

#ifdef DEBUG
  fprintf(stderr, "<<< %s", rsp);
#endif

  if (!verify_string_checksum(rsp)) {
    rcerrno = RCERROR_CHECKSUM;
    rcerrln = __LINE__;
    return NULL;
  }

  return rsp;
}


/*****************************************************************************
 Read a single NMEA sentence from file descriptor fd.
 *****************************************************************************/
char *get_sentence(int fd, long int tmt) {
  static char buf[256] = {0};
  static char snt[256] = {0};
  static short int n = 0;

  n = serial_read_string(fd, buf, 256, n, "$", "\r\n", tmt);
  if (n > 0) {
    short int m;

    m = strstr(buf, "\r\n") + 2 - buf;
    memcpy(snt, buf, m);
    snt[m] = '\0';
    n -= m;
    memmove(buf, buf+m, n);
    buf[n] = '\0';
    return snt;
  } else
    return NULL;
}


/*****************************************************************************
 Get status parameters via file descriptor fd.
 *****************************************************************************/
int get_status(int fd, status_t *status) {
  short int rn, wn, sn;
  char buf[256] = "";

  rn = serial_read_string(fd, buf, 256, 0, "$LOG108", "\r\n", 1500);
  if (rn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return -1;
  }
  if (rn == 0 && buf[0] != '\0') {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

#ifdef DEBUG
  if (rn > 0)
    fprintf(stderr, "<<< %.256s", buf);
#endif

  if (rn == 0) {
    /* Nothing received -- assume GPS mouse mode is disabled */
    status->gpsms = 0;
    /* In this mode, need to explicitly request LOG108 data */
    wn = send_cmd(fd, "$PROY108*");
    if (wn < 0) {
      rcerrno = RCERROR_SYS;
      rcerrln = __LINE__;
      return -1;
    }
    rn = serial_read_string(fd, buf, 256, 0, "$LOG108", "\r\n", 1500);
    if (rn < 0) {
      rcerrno = RCERROR_SYS;
      rcerrln = __LINE__;
      return -1;
    }
    if (rn == 0 && buf[0] == '\0') {
      rcerrno = RCERROR_NORSP;
      rcerrln = __LINE__;
      return -1;
    }
    if (rn == 0 && buf[0] != '\0') {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }

#ifdef DEBUG
    fprintf(stderr, "<<< %.256s", buf);
#endif

  } else {
    /* Text received -- GPS mouse mode is be enabled */
    status->gpsms = 1;
  }

  if (!verify_string_checksum(buf)) {
    rcerrno = RCERROR_CHECKSUM;
    rcerrln = __LINE__;
    return -1;
  }

  sn = sscanf(buf, "$LOG108,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%d*",
	      &status->fxtyp, &status->unkwn0, &status->unkwn1,
	      &status->mfowm, &status->unkwn2, &status->sntvl,
	      &status->gpsrx, &status->nfile, &status->nfix);
  if (sn != 9) {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Get current UTC date/time via file descriptor fd.
 *****************************************************************************/
int get_current_utc(int fd, date_time_t *dtp) {
  char buf[256] = "";
  char date[8] = "";
  char *lp = NULL;
  short int rn, sn, n;

  rn = serial_read_string(fd, buf, 256, 0, "$GPRMC", "\r\n", 1100);
  if (rn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return -1;
  }
  if (rn == 0 && buf[0] != '\0') {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }
  if (rn > 0) {
    sn = sscanf(buf, "$GPRMC,%6s,", dtp->time);
    if (sn != 1) {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }
    lp = buf;
    for (n = 0; n < 9; n++) {
      lp = strchr(lp, ',');
      if (lp == NULL) {
	rcerrno = RCERROR_PARSE;
	rcerrln = __LINE__;
	return -1;
      }
      lp++;
    }
    sn = sscanf(lp, "%6s,", date);
    if (sn != 1) {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }
    sprintf(dtp->date, "20%.2s%.2s%.2s", date+4, date+2, date);
  } else {
    lp = get_cmd_response(fd, "$PROY003*", "$LOG003", buf, 64);
    if (lp == NULL)
      return -1;
    sn = sscanf(lp, "$LOG003,%8s,%6s*", dtp->date, dtp->time);
    if (sn != 2) {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
      }
  }

  return 1;
}


/*****************************************************************************
 Get log start and end date/time via file descriptor fd.
 *****************************************************************************/
int get_log_bndry(int fd, log_bndry_t *lgbp) {
  char rsp[64] = "";
  char *lp = NULL;
  short int sn;

  lp = get_cmd_response(fd, "$PROY006*", "$LOG006", rsp, 64);
  if (lp == NULL)
    return -1;

  sn = sscanf(lp, "$LOG006,%8s,%6s,%8s,%6s*", lgbp->first.date,
	      lgbp->first.time, lgbp->last.date, lgbp->last.time);
  if (sn != 4) {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Get logger memory information via file descriptor fd.
 *****************************************************************************/
int get_memory_info(int fd, memory_t *memp) {
  char rsp[64] = "";
  char *lp = NULL;
  short int sn;

  lp = get_cmd_response(fd, "$PROY100*", "$LOG100", rsp, 64);
  if (lp == NULL)
    return -1;

  sn = sscanf(lp, "$LOG100,%lu,%u,%u*", &memp->nbytes, &memp->sctrsz,
	      &memp->nmsctr);
  if (sn != 3) {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Get logger firmware information via file descriptor fd.
 *****************************************************************************/
int get_firmware_info(int fd, firmware_t *frmp) {
  firmware_t frm0 = {"","","",""};
  char rsp[512] = "";
  char *lp, *cp, *sp;
  short int wn, rn, ln, sn = 0;

  wn = send_cmd(fd, "$PROY005*");
  if (wn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return -1;
  }
  rn = serial_read_repeat(fd, rsp, 511, 1000);
  if (rn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return -1;
  }
  if (rn == 0) {
    rcerrno = RCERROR_NORSP;
    rcerrln = __LINE__;
    return -1;
  }
  rsp[rn] = '\0';

#ifdef DEBUG
  fprintf(stderr, "<<< %.512s\n", rsp);
#endif

  *frmp = frm0;
  lp = rsp;
  while (lp < rsp + 512 && sn < 5) {
    lp = find_sentence(lp, 512, "PSRFTXT");
    if (lp == NULL) {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }
    ln = strlen(lp);
    if ((cp = strstr(lp, "VersionR: ")) != NULL) {
      cp = cp + strlen("VersionR: ");
      sscanf(cp, "%[^*]*", frmp->vrsnr);
    } else if ((cp = strstr(lp, "] ")) != NULL) {
      if ((sp = strchr(cp, '*')) != NULL) {
	*sp = '\0';
	cp = cp + strlen("] ");
	strncpy(frmp->frmwr, cp, 63);
	*(frmp->frmwr + 64) = '\0';
      }
    } else if ((cp = strstr(lp, "[ONOFFLOG]")) != NULL) {
      /* RGM 3800 response "[ONOFFLOG]RoyalTek Ver 1.4.0.211 GSW3LP*58\r\n" */
      if ((sp = strchr(cp, '*')) != NULL) {
	*sp = '\0';
	cp += sizeof("[ONOFFLOG]")-1;
	strncpy(frmp->frmwr, cp, 63);
	*(frmp->frmwr + 64) = '\0';
      }
    } else if ((cp = strstr(lp, "Baud rate: ")) != NULL) {
      cp = cp + strlen("Baud rate: ");
      sscanf(cp, "%[^*]*", frmp->dflbd);
    } else if ((cp = strstr(lp, "Driver Revision = ")) != NULL) {
      cp = cp + strlen("Driver Revision = ");
      sscanf(cp, "%[^*]*", frmp->drvrv);
    }
    sn++;
    lp = lp + ln + 1;
  }

  return 1;
}


/*****************************************************************************
 Read file metadata for file number filen into lgfp via file descriptor fd.
 *****************************************************************************/
int get_file_info(int fd, short int filen, logfile_t *lgfp) {
  char cmd[32];
  char rsp[64] = "";
  char *lp = NULL;
  short int sn;

  sprintf(cmd, "$PROY101,%hd*", filen);
  lp = get_cmd_response(fd, cmd, "$LOG101", rsp, 64);
  if (lp == NULL)
    return -1;

  sn = sscanf(lp, "$LOG101,%[^,],%hd,%d,%d*", lgfp->date, &lgfp->fxtyp,
	      &lgfp->nfix, &lgfp->memp);
  if (sn != 4) {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Get date and time of first fix in file described by lgfp.
 *****************************************************************************/
int get_file_start_time(int fd, const logfile_t *lgfp, date_time_t *dtp) {
  void (*tmpfp)(unsigned short, unsigned short);
  gps_fix_t fx0;
  int rrn;

  tmpfp = gdpfp;
  gdpfp = 0;
  rrn = get_data(fd, lgfp->memp, lgfp->fxtyp, 1, &fx0, lgfp->nfix, 0);
  gdpfp = tmpfp;
  if (rrn < 0)
    return -1;
  if (rrn != 1) {
    rcerrno = RCERROR_PARSE;
    rcerrln = __LINE__;
    return -1;
  }

  strncpy(dtp->date, lgfp->date, 8);
  sprintf(dtp->time, "%02d%02d%02d", fx0.hour, fx0.min, fx0.sec);

  return 1;
}


/*****************************************************************************
 Get nfix fixes of logfile data starting at logger memory address
 memp, for record type fxtyp.
 *****************************************************************************/
int get_data(int fd, int memp, short int fxtyp, int nfix,
	     gps_fix_t *gfxp, int nfxt, int nfxb) {
  const long int tmt = 1000;
  char cmd[32];
  char buf[512] = "";
  uint8_t rbc, rsi = 0;
  int sln;
  int bn = 0, sn = 0, fn = 0;
  int wn, rn;

  /* Set up data retrieve command */
  sprintf(cmd, "$PROY102,%d,%hd,%hd*", memp, fxtyp, nfix);
  /* Send command */
  wn = send_cmd(fd, cmd);
  if (wn < 0) {
    rcerrno = RCERROR_SYS;
    rcerrln = __LINE__;
    return -1;
  }

  /* Continue reading until all requested fixes received */
  while (fn < nfix) {
    /* Try to get initial $LOG102 prefix and check result for errors. */
    rn = serial_read_discard(fd, buf, 512, bn, "$LOG102", tmt);
    if (rn < 0) {
      rcerrno = RCERROR_SYS;
      rcerrln = __LINE__;
      return -1;
    }
    if (rn == 0 && buf[0] == '\0') {
      rcerrno = RCERROR_NORSP;
      rcerrln = __LINE__;
      return -1;
    }
    if (rn == 0 && buf[0] != '\0') {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }
    bn = rn;

    /* If received buffer doesn't include sentence length byte, try
       to read the necessary additional bytes, and check result for
       errors. */
    if (bn < 11) {
      rn = serial_read_repeat(fd, buf + bn, 11 - bn, tmt);
      if (rn < 0) {
	rcerrno = RCERROR_SYS;
	rcerrln = __LINE__;
	return -1;
      }
      if (rn == 0) {
	rcerrno = RCERROR_PARSE;
	rcerrln = __LINE__;
	return -1;
      }
      bn += rn;
    }

    /* Signal error if response string indicates invalid request */
    if (strncmp(buf, "$LOG102,0*6B", 11) == 0) {
      rcerrno = RCERROR_INVLDCMD;
      rcerrln = __LINE__;
      return -1;
    }

    /* Check response sentence index number */
    if (gcwrnfp != NULL && rsi != buf[8]) {
      char wstr[128];

      sprintf(wstr, "received sentence %d while expecting %d", buf[8], rsi);
      gcwrnfp(wstr,__LINE__, __FILE__);
    }
    rsi++;

    /* Get byte count for remainder of sentence */
    rbc = buf[10];
    /* Compute sentence length (up to end of "\r\n") */
    sln = 11 + rbc + 5;

    /* If number of bytes read is less than sentence length, try to
       read remainder of sentence, and check result for errors. */
    if (bn < sln) {
      rn = serial_read_repeat(fd, buf + bn, sln - bn, tmt);
      if (rn < 0) {
	rcerrno = RCERROR_SYS;
	rcerrln = __LINE__;
	return -1;
      }
      if (rn == 0) {
	rcerrno = RCERROR_PARSE;
	rcerrln = __LINE__;
	return -1;
      }
      bn += rn;
    }

#ifdef DEBUG
    fprintf(stderr, "<<< %8.8s", buf);
    {
      uint8_t n;
      for (n = 8; n < sln-5; n++)
	fprintf(stderr, " %02hx", (unsigned short)*(unsigned char*)(buf+n));
    }
    if (isprint(buf[sln-5]) && isprint(buf[sln-4]) && isprint(buf[sln-3]))
      fprintf(stderr, " %3.3s", buf+sln-5);
    else
      fprintf(stderr, " %02hx %02hx %02hx",
	      (unsigned short)*(unsigned char*)(buf+sln-5),
	      (unsigned short)*(unsigned char*)(buf+sln-4),
	      (unsigned short)*(unsigned char*)(buf+sln-3));
    fprintf(stderr, " %02hx %02hx\n",
	    (unsigned short)*(unsigned char*)(buf+sln-2),
	    (unsigned short)*(unsigned char*)(buf+sln-1));
#endif

    /* Verify checksum on current sentence */
    if (!verify_array_checksum(buf, sln-2)) {
      rcerrno = RCERROR_CHECKSUM;
      rcerrln = __LINE__;
      return -1;
    }

    /* Set sn to byte number in sentence of start of fix data */
    sn = 11;
    /* Loop over all fixes in current sentence */
    while (sn < sln - 5) {
      /* Signal error if more fixes than expected received */
      if (fn >= nfix) {
	rcerrno = RCERROR_PARSE;
	rcerrln = __LINE__;
	return -1;
      }

      /* Initialise current output fix to zero */
      memset(gfxp+fn, 0, sizeof(gps_fix_t));
      /* Copy current sentence fix into current output fix */
      memcpy(gfxp+fn, buf+sn, fix_size(fxtyp));

#ifdef WORDS_BIGENDIAN
      /* If necessary, swap endianness of float values */
      bswap_32(*(__uint32_t*)(char*)&gfxp[fn].lat);
      bswap_32(*(__uint32_t*)(char*)&gfxp[fn].lng);
      if (fxtyp > 0)
	bswap_32(*(__uint32_t*)(char*)&gfxp[fn].alt);
      if (fxtyp > 1)
	bswap_32(*(__uint32_t*)(char*)&gfxp[fn].vel);
      if (fxtyp > 2)
	bswap_32(*(__uint32_t*)(char*)&gfxp[fn].dist);
      if (fxtyp > 3) {
	bswap_16(*(__uint16_t*)(char*)&gfxp[fn].hdop);
	bswap_16(*(__uint16_t*)(char*)&gfxp[fn].pdop);
	bswap_16(*(__uint16_t*)(char*)&gfxp[fn].vdop);
	bswap_32(*(__uint32_t*)(char*)&gfxp[fn].angle);
      }
#endif

      /* Do sanity check on fix values, calling warning callback
	 function pointer, if provided, when errors encountered. The
	 unkwn field of the gps_fix_t structure is used to signal
	 valid or invalid fixes. */
      gfxp[fn].unkwn = 0;
      if (gfxp[fn].hour > 23 || gfxp[fn].min > 59 || gfxp[fn].sec > 59) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("invalid time value", __LINE__, __FILE__);
      }
      if (isnan(gfxp[fn].lat) || isinf(gfxp[fn].lat)) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("latitude with inf/NaN value", __LINE__, __FILE__);
      }
      if (isnan(gfxp[fn].lng) || isinf(gfxp[fn].lng)) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("longitude with inf/NaN value", __LINE__, __FILE__);
      }
      if (isnan(gfxp[fn].alt) || isinf(gfxp[fn].alt)) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("altitude with inf/NaN value", __LINE__, __FILE__);
      }
      if (isnan(gfxp[fn].vel) || isinf(gfxp[fn].vel)) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("velocity with inf/NaN value", __LINE__, __FILE__);
      }
	/* Still need to determine actual range */
      if (gfxp[fn].lat < -M_PI || gfxp[fn].lat > 2*M_PI) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	  gcwrnfp("out of range latitude", __LINE__, __FILE__);
      }
      if (gfxp[fn].lng < -M_PI || gfxp[fn].lng > M_PI) {
	gfxp[fn].unkwn = 1;
	if (gcwrnfp != NULL)
	   gcwrnfp("out of range longitude", __LINE__, __FILE__);
      }

      /* Increment current output fix count */
      fn++;

      /* Call progress callback function pointer if provided */
      if (gdpfp != NULL)
	gdpfp(nfxt, nfxb+fn);

      /* Increment current sentence fix byte count */
      sn += fix_size(fxtyp);
    }

    /* If buffer contains bytes beyond current sentence, move them to
       the start of the buffer and set the new buffer content
       length. */
    if (bn > sln) {
      memmove(buf, buf + sln, bn - sln);
      bn -= sln;
    } else
      bn = 0;
  }

  return fn;
}


/*****************************************************************************
 Get full logfile data for file described by lgfp.
 *****************************************************************************/
int get_file_data(int fd, const logfile_t *lgfp, gps_fix_t *gfxp) {
  const int mxfxn = 108;
  int crn, rrn, trn = 0;

  /* Call progress callback function pointer if provided */
  if (gdpfp != NULL)
    gdpfp(lgfp->nfix, 0);

  while (trn < lgfp->nfix) {

    if (lgfp->nfix - trn > mxfxn)
      crn = mxfxn;
    else
      crn = lgfp->nfix - trn;

    rrn = get_data(fd, lgfp->memp + trn*fix_size(lgfp->fxtyp), lgfp->fxtyp,
		   crn, gfxp + trn, lgfp->nfix, trn);
    if (rrn < 0)
      return -1;

    if (rrn != crn) {
      rcerrno = RCERROR_PARSE;
      rcerrln = __LINE__;
      return -1;
    }

    trn += rrn;
  }

  return lgfp->nfix;
}


/*****************************************************************************
 Write the logger/NMEA output mode set command to file descriptor fd.
 *****************************************************************************/
int set_mode(int fd, short int log, short int out) {
  char cmd[32];
  char rsp[64] = "";
  char *lp = NULL;

  sprintf(cmd, "$PROY103,%hd,%hd*", (log == 0)?0:1, (out == 0)?0:1);
  lp = get_cmd_response(fd, cmd, "$LOG103", rsp, 64);
  if (lp == NULL)
    return -1;

  if (strcmp(lp, "$LOG103,1*6B\r\n") != 0) {
    rcerrno = RCERROR_UNXPRSP;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Set status parameters via file descriptor fd.
 *****************************************************************************/
int set_status(int fd, const status_t *status) {
  char cmd[32];
  char rsp[64] = "";
  char *lp = NULL;

  sprintf(cmd, "$PROY104,0,%hd,%hd,%hd*", status->sntvl, status->fxtyp,
	  status->mfowm);
  lp = get_cmd_response(fd, cmd, "$LOG104", rsp, 64);
  if (lp == NULL)
    return -1;

  if (strcmp(lp, "$LOG104,1*6C\r\n") != 0) {
    rcerrno = RCERROR_UNXPRSP;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Write the memory erase command to file descriptor fd.
 *****************************************************************************/
int set_memory_erase(int fd) {
  char rsp[64] = "";
  char *lp = NULL;

  lp = get_cmd_response(fd, "$PROY109,-1*", "$LOG109", rsp, 64);
  if (lp == NULL)
    return -1;

  if (strcmp(lp, "$LOG109,1*61\r\n") != 0) {
    rcerrno = RCERROR_UNXPRSP;
    rcerrln = __LINE__;
    return -1;
  }

  return 1;
}


/*****************************************************************************
 Print bsz bytes in buffer buf to the indentified stream.
 *****************************************************************************/
void print_bytes(FILE *stream, const char *buf, int bsz) {
  int k;

  for (k = 0; k < bsz; k++) {
    fprintf(stream, "%02x", (unsigned char)buf[k]);
    if (isprint(buf[k]))
      fprintf(stream, "[%c] ", buf[k]);
    else
      fprintf(stream, " ");
  }
}


/*****************************************************************************
 Print fix information gfx of type ftyp.
 *****************************************************************************/
void disp_fix(short int ftyp, gps_fix_t gfx) {
  printf("Time: %02d:%02d:%02d  Lat: %f  Lon: %f", gfx.hour, gfx.min, gfx.sec,
	 gfx.lat, gfx.lng);
  if (ftyp > 0) {
    printf("  Alt: %f", gfx.alt);
  }
  if (ftyp > 1) {
    printf("  Vel: %f", gfx.vel);
  }
  printf("\n");
}
