/******************************************************************************

    Copyright © 2008-2010 Brendt Wohlberg

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License at
    http://www.gnu.org/licenses/gpl-2.0.txt.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    Most recent modification: 4 February 2012

******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include "serial.h"
#include "rtkcom.h"
#include "gpsfmt.h"


typedef struct {
  unsigned char vflg;
  unsigned char yflg;
  unsigned char nflg;
  unsigned char pflg;
  unsigned char eflg;
  unsigned char uflg;
  char *devs;
  char *spds;
  char *btas;
  char *lgts;
  char *mfos;
  char *cfls;
  char *snts;
  char *dsts;
  char *flns;
  char *cmds;
  short int sint;
  short int fnmn;
  short int fnmx;
  unsigned int sspd; /* serial line speed */
  char usgs[1500];
} cmdlnopts_t;


int prgbrfp = 0;

void scan_cmdline(int argc, char* argv[], cmdlnopts_t *cmdopt);
void cmd_status(cmdlnopts_t *cmdopt);
void cmd_date(cmdlnopts_t *cmdopt);
void cmd_list(cmdlnopts_t *cmdopt);
void cmd_set(cmdlnopts_t *cmdopt);
void cmd_read(cmdlnopts_t *cmdopt);
void cmd_erase(cmdlnopts_t *cmdopt);

void get_data_progress(unsigned short nfxt, unsigned short nfxc);
void text_progress_bar(float frac, const char *prfs);
void warning(const char *wrn, int line, const char *file);
int is_directory(const char *path);
int file_backup(const char *path);
int coms_open(cmdlnopts_t *cmdopt);
void coms_close(int fd, const cmdlnopts_t *cmdopt);
void gpsmouse_disable(int fd, int md, const cmdlnopts_t *cmdopt);
void gpsmouse_enable(int fd, int md, const cmdlnopts_t *cmdopt);
void outlog_disable(int fd, const cmdlnopts_t *cmdopt);
void outlog_enable(int fd, int md, const cmdlnopts_t *cmdopt);
void status_read(int fd, status_t *status, const cmdlnopts_t *cmdopt);
void file_read(int fd, short int flnm, char *fnam, FILE *strm,
	       const geoid_height_t *gdhtp, const status_t* status,
	       const cmdlnopts_t *cmdopt);


/*****************************************************************************
 Main program.
 *****************************************************************************/
int main (int argc, char* argv[]) {
  const char* usage0 =
   "usage: rtkgps [-h] [-v] [-d <dev> [-r <rate>] | -b <addr>]\n"
   "              ([-e] status | date | list | [-y] erase |\n"
   "              [-c <flg>] [-l <lgtp>] [-m <mfo>] [-s <int>] set |\n"
   "              [-n] [-p] [-o <dest> [-u]] [-f <nstr>] read)\n\n"
   "       -h        display usage\n"
   "       -v        verbose mode\n"
   "       -d <dev>  specify serial device\n"
   "       -r <rate> specify baud rate for serial device\n"
   "       -b <addr> specify bluetooth address\n"
   "       -e        display extended status information\n";
  const char* usage1 =
   "       -c <flg>  set real-time output (GPS mouse) mode "
                     "(0=disable, 1=enable)\n"
   "       -l <lgtp> set log record type (tl, tla, or tlav)\n"
   "       -m <mfo>  set memory overwrite behaviour (o=overwrite, s=stop)\n"
   "       -s <int>  set sampling interval in seconds\n"
   "       -n        output data in simple native text form\n"
   "       -p        display text progress bar\n"
   "       -o <dest> specify destination file or directory\n"
   "       -u        skip downloading date for existing files\n";
  const char* usage2 =
   "       -f <nstr> string specifying index number(s) of log file(s) \n"
   "                 to retrieve as a single file number, or range of \n"
   "                 file numbers in the format -n, n-, or n-m\n"
   "       -y        don't ask for confirmation\n";

  /* most Royalteks operate on 57600 baud, use that as the default */
  cmdlnopts_t cmdopt = {0,0,0,0,0,0,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
			NULL,NULL,-1,-1,-1,57600,""};

  /* Initialise usage string */
  strcpy(cmdopt.usgs, usage0);
  strcat(cmdopt.usgs, usage1);
  strcat(cmdopt.usgs, usage2);
  /* Scan command line options */
  scan_cmdline(argc, argv, &cmdopt);

  /* Set up warning callback function */
  gcwrnfp = warning;

  /* Perform requested task */
  if (strcmp(cmdopt.cmds,"status") == 0) {
    cmd_status(&cmdopt);
  } else if (strcmp(cmdopt.cmds,"date") == 0) {
    cmd_date(&cmdopt);
  } else if (strcmp(cmdopt.cmds,"list") == 0) {
    cmd_list(&cmdopt);
  } else if (strcmp(cmdopt.cmds,"set") == 0) {
    cmd_set(&cmdopt);
  } else if (strcmp(cmdopt.cmds,"read") == 0) {
    cmd_read(&cmdopt);
  } else if (strcmp(cmdopt.cmds,"erase") == 0) {
    cmd_erase(&cmdopt);
  }

  exit(0);
}


/*****************************************************************************
 Scan command line arguments and check for valid choices.
 *****************************************************************************/
void scan_cmdline(int argc, char* argv[], cmdlnopts_t *cmdopt) {
  int n;

  /* Scan command line options */
  opterr = 0;
  while ((n = getopt (argc, argv, "hved:r:b:l:m:c:s:npo:uf:y")) != -1)
    switch (n) {
    case 'h': fprintf(stderr, "%s", cmdopt->usgs);
      exit(0);
    case '?': fprintf(stderr, "rtkgps: Unknown command line flag\n");
      fprintf(stderr, "%s", cmdopt->usgs);
      exit(1);
    case 'v': cmdopt->vflg = 1;
      break;
    case 'e': cmdopt->eflg = 1;
      break;
    case 'd': cmdopt->devs = optarg;
      break;
    case 'r': cmdopt->spds = optarg;
      break;
    case 'b': cmdopt->btas = optarg;
      break;
    case 'l': cmdopt->lgts = optarg;
      break;
    case 'm': cmdopt->mfos = optarg;
      break;
    case 'c': cmdopt->cfls = optarg;
      break;
    case 's': cmdopt->snts = optarg;
      break;
    case 'n': cmdopt->nflg = 1;
     break;
    case 'p': cmdopt->pflg = 1;
     break;
    case 'o': cmdopt->dsts = optarg;
      break;
    case 'u': cmdopt->uflg = 1;
      break;
    case 'f': cmdopt->flns = optarg;
      break;
    case 'y': cmdopt->yflg = 1;
      break;
    default:
      exit(1);
    }

  /* Check command line options */
  if (cmdopt->devs != NULL && cmdopt->btas != NULL) {
    fprintf(stderr, "%s", cmdopt->usgs);
    exit(1);
  }

#if !ENABLE_LINUX_BT-0
  if (cmdopt->btas != NULL) {
    fprintf(stderr, "rtkgps: Bluetooth support disabled\n");
    exit(1);
  }
  /* bluetooth not enabled and Serial device name not provided */
  if (cmdopt->devs == NULL) {
    fprintf(stderr, "%s", cmdopt->usgs);
    exit(1);
  }
#endif /* ENABLE_LINUX_BT */

  /* baud rate is illegal for bluetooth device */
  if (cmdopt->spds != NULL && cmdopt->btas != NULL) {
    fprintf(stderr, "%s", cmdopt->usgs);
    exit(1);
  }
  if (cmdopt->spds != NULL) {
    int baudi;
    if(sscanf(cmdopt->spds, "%d", &baudi) != 1 || baudi < 0) {
      fprintf(stderr, "rtkgps: Flag -r may only take unsigned "
	      "integer values\n");
      exit(1);
    }
    if( !dev_speed_valid(baudi)){
      fprintf(stderr, "rtkgps: Unsupported baud rate: %d\n", baudi);
      exit(1);
    }
    cmdopt->sspd = baudi;
  }

  if (optind < argc)
    cmdopt->cmds = argv[optind++];
  else {
    fprintf(stderr, "%s", cmdopt->usgs);
    exit(1);
  }
  if ((strcmp(cmdopt->cmds,"status") != 0 &&
       strcmp(cmdopt->cmds,"date") != 0 &&
       strcmp(cmdopt->cmds,"list") != 0 &&
       strcmp(cmdopt->cmds,"read") != 0 &&
       strcmp(cmdopt->cmds,"set") != 0 &&
       strcmp(cmdopt->cmds,"erase") != 0) ||
      (strcmp(cmdopt->cmds,"status") != 0 && cmdopt->eflg) ||
      (strcmp(cmdopt->cmds,"set") != 0 && cmdopt->cfls) ||
      (strcmp(cmdopt->cmds,"set") != 0 && cmdopt->lgts) ||
      (strcmp(cmdopt->cmds,"set") != 0 && cmdopt->mfos) ||
      (strcmp(cmdopt->cmds,"set") != 0 && cmdopt->snts) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->dsts != NULL) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->nflg) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->pflg) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->uflg) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->dsts) ||
      (strcmp(cmdopt->cmds,"read") != 0 && cmdopt->flns) ||
      (strcmp(cmdopt->cmds,"erase") != 0 && cmdopt->yflg)) {
    fprintf(stderr, "%s", cmdopt->usgs);
    exit(1);
  }
  if (strcmp(cmdopt->cmds,"set") == 0) {
    if (cmdopt->lgts == NULL && cmdopt->mfos == NULL &&
	cmdopt->cfls == NULL && cmdopt->snts == NULL) {
      fprintf(stderr, "rtkgps: Must specify at least one parameter flag "
	      " for set command\n");
      exit(1);
    }
    if (cmdopt->lgts != NULL && strcmp(cmdopt->lgts,"tl") != 0 &&
	strcmp(cmdopt->lgts,"tla") != 0 &&
	strcmp(cmdopt->lgts,"tlav") != 0) {
      fprintf(stderr, "rtkgps: Flag -l for set command may only take values"
	      "\"tl\", \"tla\", or \"tlav\"\n");
      exit(1);
    }
    if (cmdopt->mfos != NULL && strcmp(cmdopt->mfos,"o") != 0 &&
	strcmp(cmdopt->mfos,"s") != 0) {
      fprintf(stderr, "rtkgps: Flag -m for set command may only take values"
	      "\"o\" or \"s\"\n");
      exit(1);
    }
    if (cmdopt->cfls != NULL && strcmp(cmdopt->cfls,"0") != 0 &&
	strcmp(cmdopt->cfls,"1") != 0) {
      fprintf(stderr, "rtkgps: Flag -c for set command may only take values"
	      "\"0\" or \"1\"\n");
      exit(1);
    }
    if (cmdopt->snts != NULL &&
	(sscanf(cmdopt->snts, "%hd", &cmdopt->sint) != 1 ||
	 cmdopt->sint < 1 || cmdopt->sint > 60)) {
      fprintf(stderr, "rtkgps: Flag -c for set command may only take "
	      "integer values between 1 and 60\n");
      exit(1);
    }
  }
  if (cmdopt->flns != NULL) {
    unsigned char strvld = 0;
    if (strchr(cmdopt->flns, '-')) {
      if (cmdopt->flns[0] == '-') {
	if (sscanf(cmdopt->flns, "-%hd", &cmdopt->fnmx) == 1)
	  strvld = 1;
      } else if (cmdopt->flns[strlen(cmdopt->flns)-1] == '-') {
	if (sscanf(cmdopt->flns, "%hd-", &cmdopt->fnmn) == 1)
	  strvld = 1;
      } else {
	if (sscanf(cmdopt->flns, "%hd-%hd", &cmdopt->fnmn, &cmdopt->fnmx) == 2)
	  strvld = 1;
      }
    } else {
      if (sscanf(cmdopt->flns, "%hd", &cmdopt->fnmn) == 1) {
	cmdopt->fnmx = cmdopt->fnmn;
	strvld = 1;
      }
    }
    if (cmdopt->fnmn != -1 && cmdopt->fnmn < 0)
      strvld = 0;
    if (cmdopt->fnmn != -1 && cmdopt->fnmx != -1 && 
	cmdopt->fnmn > cmdopt->fnmx)
      strvld = 0;
    if (!strvld) {
      fprintf(stderr, "rtkgps: Flag -f for read command has invalid "
	      "file number specification string\n");
      exit(1);
    }
  }
  if (strcmp(cmdopt->cmds,"erase") == 0 &&
      cmdopt->devs == NULL && cmdopt->btas == NULL) {
    fprintf(stderr, "rtkgps: Must specify explicit device or address for "
	    " erase command\n");
    exit(1);
  }
}


/*****************************************************************************
 Perform rtkgps status command.
 *****************************************************************************/
void cmd_status(cmdlnopts_t *cmdopt) {
  int fd;
  unsigned int mu = 0;
  status_t status;
  logfile_t lgfl;
  log_bndry_t lgbd;
  memory_t mem;
  firmware_t frm;

  fd = coms_open(cmdopt);

  if (cmdopt->vflg)
    printf("Requesting logger status information\n");

  status_read(fd, &status, cmdopt);

  if (cmdopt->eflg) {
    if (cmdopt->vflg)
      printf("Requesting extended logger information\n");

    gpsmouse_disable(fd, status.gpsms, cmdopt);

    if (get_log_bndry(fd, &lgbd) < 0) {
      fprintf(stderr,"rtkgps: Failed to read log start/end details [%s]\n",
	      gcstrerror(rcerrno));
      gpsmouse_enable(fd, status.gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
    if (get_memory_info(fd, &mem) < 0) {
      fprintf(stderr,"rtkgps: Failed to read logger memory details [%s]\n",
	      gcstrerror(rcerrno));
      gpsmouse_enable(fd, status.gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
    if (get_firmware_info(fd, &frm) < 0) {
      fprintf(stderr,"rtkgps: Failed to read logger firmware details [%s]\n",
	      gcstrerror(rcerrno));
      gpsmouse_enable(fd, status.gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
     /* Get info for first logfile */
    if (get_file_info(fd, 0, &lgfl) < 0) {
      fprintf(stderr,"rtkgps: Error reading information for file %d [%s]\n",
	      0, gcstrerror(rcerrno));
      gpsmouse_enable(fd, status.gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    } 
    if (lgfl.memp == 0) { /* Memory has not wrapped around in overwrite mode */
      /* Get info for last logfile */
      if (get_file_info(fd, status.nfile-1, &lgfl) < 0) {
	fprintf(stderr,"rtkgps: Error reading information for file %d [%s]\n",
		status.nfile-1, gcstrerror(rcerrno));
	gpsmouse_enable(fd, status.gpsms, cmdopt);
	coms_close(fd, cmdopt);
	exit(5);
      }
      /* Memory used computed from last logfile pointer plus number of 
	 fixes in active logfile */
      mu = lgfl.memp + status.nfix*fix_size(status.fxtyp);
    } else { /* Memory has wrapped around in overwrite mode */
      int n;
      /* Need to compute memory used by adding up number of fixes in each 
	 log file */
      mu = lgfl.nfix*fix_size(lgfl.fxtyp);
      for (n = 1; n < status.nfile; n++) {
	if (get_file_info(fd, n, &lgfl) < 0) {
	  fprintf(stderr,"rtkgps: Error reading information for file "
                 "%d [%s]\n", n, gcstrerror(rcerrno));
	  gpsmouse_enable(fd, status.gpsms, cmdopt);
	  coms_close(fd, cmdopt);
	  exit(5);
	}
	mu += lgfl.nfix*fix_size(lgfl.fxtyp);
      }
    }

    gpsmouse_enable(fd, status.gpsms, cmdopt);
  }

  printf("GPS Fix:            %s\nGPS mouse mode:     %s\n"
	 "Record type:        %s\nMemory full:        %s\n"
	 "Sampling interval:  %hds\nNumber of files:    %hd\n"
	 "Fixes in last file: %d\n", gpsrx_string(status.gpsrx),
	 gpsms_string(status.gpsms), fxtyp_string(status.fxtyp),
	 mfowm_string(status.mfowm), status.sntvl, status.nfile,
	 status.nfix
	 );

  if (cmdopt->eflg) {
    printf("First log fix:      %.4s-%.2s-%.2s %.2s:%.2s:%.2s\n"
	   "Last log fix:       %.4s-%.2s-%.2s %.2s:%.2s:%.2s\n",
	   lgbd.first.date,lgbd.first.date+4,lgbd.first.date+6,
	   lgbd.first.time, lgbd.first.time+2, lgbd.first.time+4,
	   lgbd.last.date,lgbd.last.date+4,lgbd.last.date+6,
	   lgbd.last.time, lgbd.last.time+2, lgbd.last.time+4);
    printf("Device memory:      %7.2fkb\n"
	   "Memory used:        %7.2fkb (%.2f%%)\n",
	   mem.nbytes / 1024.0, mu / 1024.4, ((float)mu / mem.nbytes)*100.0);
    printf("Version:            %s\n"
	   "Firmware:           %s\n", frm.vrsnr, frm.frmwr);
  }

  coms_close(fd, cmdopt);
}


/*****************************************************************************
 Perform rtkgps date command.
 *****************************************************************************/
void cmd_date(cmdlnopts_t *cmdopt) {
  int fd;
  date_time_t dttm;

  fd = coms_open(cmdopt);

  if (cmdopt->vflg)
    printf("Determining current date/time information\n");

  if (get_current_utc(fd, &dttm) < 0) {
    fprintf(stderr,"rtkgps: Failed to determine current date/time [%s]\n",
	    gcstrerror(rcerrno));
    coms_close(fd, cmdopt);
    exit(5);
  }

  printf("%.4s-%.2s-%.2s %.2s:%.2s:%.2s\n", dttm.date,dttm.date+4,
	 dttm.date+6,dttm.time, dttm.time+2, dttm.time+4);

  coms_close(fd, cmdopt);
}


/*****************************************************************************
 Perform rtkgps list command.
 *****************************************************************************/
void cmd_list(cmdlnopts_t *cmdopt) {
  int fd;
  status_t status;
  logfile_t *lgflp;
  /*unsigned int mem = 0;*/
  int n;

  fd = coms_open(cmdopt);

  if (cmdopt->vflg)
    printf("Requesting logger status information\n");

  status_read(fd, &status, cmdopt);

  gpsmouse_disable(fd, status.gpsms, cmdopt);

  if ((lgflp = malloc(status.nfile*sizeof(logfile_t))) == NULL) {
    fprintf(stderr,"rtkgps: Error allocating memory\n");
    gpsmouse_enable(fd, status.gpsms, cmdopt);
    coms_close(fd, cmdopt);
    exit(2);
  }

  for (n = 0; n < status.nfile; n++) {
    if (cmdopt->vflg) {
      printf("Requesting metadata for file %4d\n", n);
    }
    if (get_file_info(fd, n, lgflp+n) < 0) {
      fprintf(stderr,"rtkgps: Error reading information for file %d [%s]\n",
	      n, gcstrerror(rcerrno));
      free(lgflp);
      gpsmouse_enable(fd, status.gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
  }

  printf("File num   Date      Fix type  Num fix  Mem ptr\n");
  for (n = 0; n < status.nfile; n++)
    printf("%8d   %8s  %8hd  %7d  %7d\n", n, lgflp[n].date, lgflp[n].fxtyp,
	   lgflp[n].nfix, lgflp[n].memp);

  free(lgflp);
  gpsmouse_enable(fd, status.gpsms, cmdopt);
  coms_close(fd, cmdopt);
}


/*****************************************************************************
 Perform rtkgps set command.
 *****************************************************************************/
void cmd_set(cmdlnopts_t *cmdopt) {
  int fd;
  status_t status;
  unsigned char cflg = 0, fxtp = 0, mfow = 0, gpsm;

  fd = coms_open(cmdopt);

  if (cmdopt->vflg) {
    printf("Requesting logger status information\n");
  }
  status_read(fd, &status, cmdopt);

  gpsm = status.gpsms;
  if (cmdopt->cfls != NULL) {
    if (strcmp(cmdopt->cfls,"0") == 0)
      gpsm = 0;
    else if (strcmp(cmdopt->cfls,"1") == 0)
      gpsm = 1;
  }
  if (cmdopt->lgts != NULL) {
    if (strcmp(cmdopt->lgts,"tl") == 0)
      fxtp = 0;
    else if (strcmp(cmdopt->lgts,"tla") == 0)
      fxtp = 1;
    else if (strcmp(cmdopt->lgts,"tlav") == 0)
      fxtp = 2;
    if (fxtp != status.fxtyp) {
      status.fxtyp = fxtp;
      cflg = 1;
    }
  }
  if (cmdopt->mfos != NULL) {
    if (strcmp(cmdopt->mfos,"o") == 0)
      mfow = 0;
    else if (strcmp(cmdopt->mfos,"s") == 0)
      mfow = 1;
    if (mfow != status.mfowm) {
      status.mfowm = mfow;
      cflg = 1;
    }
  }
  if (cmdopt->snts != NULL && status.sntvl != cmdopt->sint) {
    status.sntvl = cmdopt->sint;
    cflg = 1;
  }

  if (cflg == 1) {
    gpsmouse_disable(fd, status.gpsms, cmdopt);
    if (cmdopt->vflg) {
      printf("Setting new logger parameters\n");
    }
    if (set_status(fd, &status) < 0) {
      fprintf(stderr,"rtkgps: Failed to set device status [%s]\n",
	      gcstrerror(rcerrno));
      gpsmouse_enable(fd, gpsm, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
    gpsmouse_enable(fd, gpsm, cmdopt);
  } else {
    if (gpsm && !status.gpsms)
      gpsmouse_enable(fd, gpsm, cmdopt);
    else if (!gpsm && status.gpsms)
      gpsmouse_disable(fd, status.gpsms, cmdopt);
  }

  coms_close(fd, cmdopt);

}


/*****************************************************************************
 Perform rtkgps read command.
 *****************************************************************************/
void cmd_read(cmdlnopts_t *cmdopt) {
  int fd;
  status_t status;
  geoid_height_t gdht = {0,0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,NULL,NULL};
  FILE *strm = NULL;
  char *fnam = NULL;
  short int n;

#ifdef GEOIDCOR
  /* Set up geoid correction data structure */
  if (geoid_calc_open(GGRDPATH, &gdht) == -1) {
    fprintf(stderr, "rtkgps: Warning: could not access geoid correction "
	    "data\n");
  }
#endif

  /* Open communication with logger */
  fd = coms_open(cmdopt);

  /* Set up progress bar if requested */
  if (cmdopt->pflg) {
#ifdef HAVE_TIOCGWINSZ
     struct winsize ws;
     if (ioctl(1, TIOCGWINSZ, (void *)&ws) == 0)
       prgbrfp = 1;
     else if (ioctl(2, TIOCGWINSZ, (void *)&ws) == 0)
       prgbrfp = 2;
#else
     prgbrfp = 1;
#endif
     if (prgbrfp)
       gdpfp = get_data_progress;
  }

  if (cmdopt->vflg)
    printf("Requesting logger status information\n");

  /* Read logger status */
  status_read(fd, &status, cmdopt);

 /* Handle unspecified ends of file number range */
  if (cmdopt->fnmn == -1)
    cmdopt->fnmn = 0;
  if (cmdopt->fnmx == -1)
    cmdopt->fnmx = status.nfile-1;

  /* Check for minimum file number out of range */
  if (cmdopt->fnmn >= status.nfile) {
    fprintf(stderr,"rtkgps: Requested file number(s) all invalid\n");
    coms_close(fd, cmdopt);
    exit(1);
  }

  /* Check for maximum file number out of range */
  if (cmdopt->fnmx >= status.nfile) {
    cmdopt->fnmx = status.nfile-1;
    fprintf(stderr, "rtkgps:  Warning: reduced maximum requested file "
	    "number to valid range\n");
  }

  outlog_disable(fd, cmdopt);

  /* The complicated handling of output files, split between this function 
     and file_read, is due to the following output policy:
     • If no output file is specified, selected logfiles are written 
       to stdout
     • If the specified output file is a regular file, all selected logfiles
       are written to that file
     • If the specified output file is a directory, each selected logfile is
       written to a unique file within that directory  
  */
  if (cmdopt->dsts == NULL)
    strm = stdout; /* If output flag not specified, output stream is stdout */
  else {
    /* If output flag is specified, determine whether it is a directory */
    if (is_directory(cmdopt->dsts)) {
      /* If specified output path is a directory, allocate memory for
	 constructing the full output filename */
      if ((fnam = malloc(strlen(cmdopt->dsts) + 32)) == NULL) {
	fprintf(stderr,"rtkgps: Error allocating memory\n");
	outlog_enable(fd, status.gpsms, cmdopt);
	coms_close(fd, cmdopt);
	exit(2);
      }
    } else {
      /* If specified output path is not a directory, open the file
	 for writing after creating a backup if the file already exists. */
      if (file_backup(cmdopt->dsts) != 0) {
	fprintf(stderr,"rtkgps: Error creating backup of file %s\n",
		cmdopt->dsts);
	outlog_enable(fd, status.gpsms, cmdopt);
	coms_close(fd, cmdopt);
	exit(3);
      }
      if ((strm = fopen(cmdopt->dsts, "w")) == NULL) {
	fprintf(stderr,"rtkgps: Error opening output file %s [%s]\n",
		cmdopt->dsts, gcstrerror(rcerrno));
	outlog_enable(fd, status.gpsms, cmdopt);
	coms_close(fd, cmdopt);
	exit(3);
      }
    }
  }

  if (strm != NULL) {
    /* Write output file header */
    if (cmdopt->nflg)
      print_hdr_native(strm);
    else
      print_hdr_nmea(strm, cmdopt->btas);
  }

  /* Reset warning function message records */
  warning(NULL, 0, NULL);

  /* Read requested range of log files */
  for (n = cmdopt->fnmn; n <= cmdopt->fnmx; n++) {
    char nstr[8];

    /* If progress bar requested and verbose output not enabled, set
       up prefix displaying file number */
    if (cmdopt->pflg && !cmdopt->vflg) {
      sprintf(nstr, "%4d ", n);
      text_progress_bar(0.0, nstr);
    }
    file_read(fd, n, fnam, strm, &gdht, &status, cmdopt);

    /* Reset warning function message records */
    warning(NULL, 0, NULL);
  }

  /* Free memory allocated for file name */
  free(fnam);

  outlog_enable(fd, status.gpsms, cmdopt);

  /* Close ommunication with logger */
  coms_close(fd, cmdopt);

#ifdef GEOIDCOR
   /* Destroy geoid correction data structure */
  geoid_calc_close(&gdht);
#endif
}


/*****************************************************************************
  Perform rtkgps erase command.
 *****************************************************************************/
void cmd_erase(cmdlnopts_t *cmdopt) {
  int fd;
  int ce;

  ce = cmdopt->yflg;
  if (!ce) {
    printf("Confirm complete memory erase [y/N]\n");
    if (tolower(getchar()) == 'y')
      ce = 1;
  }

  if (ce) {
    fd = coms_open(cmdopt);

    if (cmdopt->vflg) {
      printf("Erasing memory\n");
    }

    if (set_memory_erase(fd) < 0) {
      fprintf(stderr,"rtkgps: Memory erase command not confirmed [%s]\n",
	      gcstrerror(rcerrno));
      coms_close(fd, cmdopt);
      exit(5);
    }

    coms_close(fd, cmdopt);
  } else if (cmdopt->vflg)
    printf("Erase operation aborted\n");
}


/*****************************************************************************
 Get data progress callback function.
 *****************************************************************************/
void get_data_progress(unsigned short nfxt, unsigned short nfxc) {
  text_progress_bar((float)nfxc/(float)nfxt, NULL);
  if (nfxc == nfxt)
    fprintf(stderr,"\n");
}


/*****************************************************************************
 Display text progress bar.
 *****************************************************************************/
void text_progress_bar(float frac, const char *prfs) {
  static const char *prfx = "";
  static unsigned short nbpv = 0;
  static unsigned short ncol = 0;
  static unsigned short bmax = 0;
  static char bar[] = "================================================="
    "==================================================================="
    "===================================================================";
  static char spc[] = "                                                 "
    "                                                                   "
    "                                                                   "
    "                                                                   ";

  if (prfs == NULL && prgbrfp) {
    FILE *fp = NULL;
    unsigned short nbar;

    /* Set output file pointer based on file descriptor in prgbrfp */
    if (prgbrfp == 1)
      fp = stdout;
    else if (prgbrfp == 2)
      fp = stderr;
    else
      return;

#ifdef HAVE_TIOCGWINSZ
    {
      struct winsize ws;
      if (ioctl(prgbrfp, TIOCGWINSZ, (void *)&ws) == 0)
        ncol = ws.ws_col;
      else
        return;
    }
#else
    ncol = 40;
#endif

    if (frac < 0) {
      /* Print spaces to clear current progress bar line */
      fprintf(fp, "%.*s\r", (int)((ncol < strlen(spc))?ncol:strlen(spc)), spc);
      fflush(fp);
    } else {
      /* Compute length of current progress bar */
      bmax = ncol - strlen(prfx) - 7;
      if (bmax > strlen(bar))
	bmax = strlen(bar);
      nbar = (unsigned short)floor(frac*bmax);
      if (nbar > bmax)
	nbar = bmax;

      /* If progress bar length has changed, print new line and update
	 previous bar length */
      if (nbar != nbpv) {
	nbpv = nbar;
	fprintf(fp, "%s|%.*s%.*s| %3.0f%%\r", prfx, nbar, bar, bmax-nbar,
		spc, 100.0*frac);
	fflush(fp);
      }
    }
  } else
    prfx = prfs;
}


/*****************************************************************************
 Warning display function.
 *****************************************************************************/
#if defined(__GNUC__) && !defined(DEBUG)
void warning(const char *wrn, int line __attribute__((unused)), 
	     const char *file __attribute__((unused))) {
#else
void warning(const char *wrn, int line, const char *file) {
#endif
  static char wmsg[20][256];
  static uint8_t nmsg = 0;

  if (wrn == NULL) {
    /* Reset warning message records if function called with null string */
    nmsg = 0;
  } else {
    /* Record and print non-null string */
    uint8_t msn = 0;
    uint8_t n = 0;

#ifdef DEBUG
    /* When debugging, print every message */
    msn = 1;
#else
    /* Determine whether error message has already been encountered */
    for (n = 0; n < nmsg; n++) {
      if (strncmp(wrn, wmsg[n], 255) == 0) {
	msn = 1;
	break;
      }
    }
#endif

    if (!msn) {
      /* Current error message has not already been encountered */
#ifndef DEBUG
      /* When debugging, no need to record messages */
      if (nmsg < 20) {
	strncpy(wmsg[n], wrn, 255);
	wmsg[n][255] = '\0';
	nmsg++;
      }
#endif

      /* If progress bar file descriptor is not zero, call progress bar
	 function with request to clear current progress bar line. */
      if (prgbrfp)
	text_progress_bar(-1.0, NULL);
      /* Print warning */
      fprintf(stderr, "rtkgps: Warning: ");
      fprintf(stderr, "%s ", wrn);
#ifdef DEBUG
      fprintf(stderr, "at line %d in file %s", line, file);
#endif
      fprintf(stderr, "\n");

    }
  }
}


/*****************************************************************************
 Determine whether the file path is a dictionary.
 *****************************************************************************/
int is_directory(const char *path) {
  struct stat pstat;

  if (stat(path, &pstat) < 0)
    return 0; /* If file doesn't exist, it isn't a directory */
  return S_ISDIR(pstat.st_mode);
}


/*****************************************************************************
 Determine whether the file path is a non-zero size regular file
 *****************************************************************************/
int is_nzsregfile(const char *path) {
  struct stat pstat;

  if (stat(path, &pstat) < 0)
    return 0; /* If file doesn't exist, it isn't a regular file */
  return S_ISREG(pstat.st_mode) && (pstat.st_size > 0);
}


/*****************************************************************************
 If the specified file exists, rename it to have a .bak extension.
 *****************************************************************************/
int file_backup(const char *path) {
  char bpath[512];

  if (strlen(path) > 500)
    return -3;
  if (is_directory(path))
    return -2;
  if (access(path, F_OK) == 0) {
    sprintf(bpath, "%s.bak", path);
    return rename(path, bpath);
  } else
    return 0;
}


/*****************************************************************************
 Open communications with GPS device.
 *****************************************************************************/
int coms_open(cmdlnopts_t *cmdopt) {
  int fd = -1;

  /* Open communication with device */
  if (cmdopt->devs != NULL) {
    /* Serial device name is provided */
    fd = dev_open(cmdopt->devs);
    if (fd < 0) {
      fprintf(stderr, "rtkgps: Error opening device %s [%s]\n", cmdopt->devs,
	      strerror(errno));
      exit(4);
    }
    if (dev_config_serial(fd, cmdopt->sspd) < 0) {
      fprintf(stderr, "rtkgps: Error setting device speed to %u [%s]\n",
      		cmdopt->sspd, strerror(errno));
      exit(4);
    }

    /* Display connection message in verbose mode */
    if (cmdopt->vflg) {
      printf("Opened device %s at %u baud\n", cmdopt->devs, cmdopt->sspd);
    }
  } else {
#if ENABLE_LINUX_BT-0
    static char btscs[24];
    /* Serial device name not provide: scan or use provide bluetooth address */
    if (cmdopt->btas == NULL) {
      /* Bluetooth address not specified: do a scan for a matching device */
      bt_device_t btd[32];
      int nbtd, nbgp = -1, n;

      /* Perform scan */
      nbtd = bt_scan(btd, 32);
      if (nbtd < 0) {
	fprintf(stderr, "rtkgps: Error perfoming bluetooth scan [%s]\n",
		strerror(errno));
	exit(4);
      }
      /* Report error if no devices found */
      if (nbtd == 0) {
	fprintf(stderr, "rtkgps: No bluetooth devices found during scan\n");
	exit(4);
      }
      /* Display scan result in verbose mode */
      if (cmdopt->vflg) {
	printf("Bluetooth scan:\n");
	bt_scan_print(stdout,  btd, nbtd);
      }
      /* Look for single bluetooth device named BlueGPS */
      for (n = 0; n < nbtd; n++) {
	if (strncmp(btd[n].name, "BlueGPS ", 8) == 0) {
	  if (nbgp == -1)
	    nbgp = n;
	  else {
	    /* Report error if multiple matching devices found */
	    fprintf(stderr, "rtkgps: Multiple BlueGPS devices found during"
		    " scan\n");
	    exit(4);
	  }
	}
      }
      if (nbgp == -1) {
	/* Report error if no matching devices found */
	fprintf(stderr, "rtkgps: No BlueGPS devices found during scan\n");
	exit(4);
      }
      /* Set selected device address from matching device address */
      ba2str(&btd[nbgp].bdaddr, btscs);

      cmdopt->btas = btscs;
      /* Display matching device details in verbose mode */
      if (cmdopt->vflg) {
	printf("Found appropriate device: ");
	bt_scan_print(stdout,  &btd[nbgp], 1);
      }
      /* This delay avoids "Operation already in progress" errors on
	 trying to open the bluetooth connection */
      sleep(1);
    }
    /* Open a connection to the selected bluetooth device */
    fd = bt_open(cmdopt->btas, 1);
    if (fd < 0) {
      fprintf(stderr, "rtkgps: Error connecting to %s [%s]\n", cmdopt->btas,
	      strerror(errno));
      exit(4);
    }
    /* Display connection message in verbose mode */
    if (cmdopt->vflg) {
      printf("Connected to %s\n", cmdopt->btas);
    }
#endif /* ENABLE_LINUX_BT */
  }

  return fd;
}


/*****************************************************************************
 Close communications with GPS device.
 *****************************************************************************/
void coms_close(int fd, const cmdlnopts_t *cmdopt) {
  if (cmdopt->devs != NULL) {
    dev_close(fd);
    if (cmdopt->vflg)
      printf("Closed device %s\n", cmdopt->devs);
  } else {
    bt_close(fd);
    if (cmdopt->vflg)
      printf("Disconnected from %s\n", cmdopt->btas);
  }
}


/*****************************************************************************
 Disable GPS mouse mode (1Hz real-time NMEA output).
 *****************************************************************************/
void gpsmouse_disable(int fd, int md, const cmdlnopts_t *cmdopt) {
  /* Set GPS mouse mode inactive */
  if (md) {
    if (cmdopt->vflg)
      printf("Disabling GPS mouse mode\n");
    if (set_mode(fd, 1, 0) < 0) {
      fprintf(stderr,"rtkgps: Failed to set logger mode [%s]\n",
	      gcstrerror(rcerrno));
      coms_close(fd, cmdopt);
      exit(5);
    }
  }
}


/*****************************************************************************
 Enable GPS mouse mode (1Hz real-time NMEA output).
 *****************************************************************************/
void gpsmouse_enable(int fd, int md, const cmdlnopts_t *cmdopt) {
 /* Restore GPS mouse mode if appropriate */
  if (md) {
    if (cmdopt->vflg)
      printf("Enabling GPS mouse mode\n");
    if (set_mode(fd, 1, 1) < 0) {
      fprintf(stderr,"rtkgps: Failed to set logger mode [%s]\n",
	      gcstrerror(rcerrno));
      coms_close(fd, cmdopt);
      exit(5);
    }
  }
}


/*****************************************************************************
 Disable logger and GPS mouse mode (1Hz real-time NMEA output).
 *****************************************************************************/
void outlog_disable(int fd, const cmdlnopts_t *cmdopt) {
  /* Set logger and GPS mouse mode inactive */
  if (cmdopt->vflg)
    printf("Disabling logger and GPS mouse mode\n");
  if (set_mode(fd, 0, 0) < 0) {
    fprintf(stderr,"rtkgps: Failed to set logger mode [%s]\n",
	    gcstrerror(rcerrno));
    coms_close(fd, cmdopt);
    exit(5);
  }
}


/*****************************************************************************
 Enable logger and GPS mouse mode (1Hz real-time NMEA output).
 *****************************************************************************/
void outlog_enable(int fd, int md, const cmdlnopts_t *cmdopt) {
  /* Restore logger and GPS mouse mode */
  if (cmdopt->vflg) {
    if (md)
      printf("Enabling logger and GPS mouse mode\n");
    else
      printf("Enabling logger\n");
  }
  if (set_mode(fd, 1, md) < 0) {
    fprintf(stderr,"rtkgps: Failed to set logger mode [%s]\n",
	    gcstrerror(rcerrno));
    coms_close(fd, cmdopt);
    exit(5);
  }
}


/*****************************************************************************
 Read GPS device status.
 *****************************************************************************/
void status_read(int fd, status_t *status, const cmdlnopts_t *cmdopt) {
  if (get_status(fd, status) < 0) {
    fprintf(stderr,"rtkgps: Failed to get device status [%s]\n",
	    gcstrerror(rcerrno));
    coms_close(fd, cmdopt);
    exit(5);
  }
}


/*****************************************************************************
 Read a single log file.
 *****************************************************************************/
void file_read(int fd, short int flnm, char *fnam, FILE *strm,
	       const geoid_height_t *gdhtp, const status_t* status,
	       const cmdlnopts_t *cmdopt) {
  logfile_t lgfl;
#if !defined(FILENAME_DATE_PTR)
  date_time_t dt;
#endif
  gps_fix_t *gfxp = NULL;
  float *gcrp = NULL;
  char *pstr = "";
  int fn;

  if (cmdopt->vflg)
    printf("Requesting metadata for file %4d\n", flnm);

  /* Read metadata for file number flnm */
  if (get_file_info(fd, flnm, &lgfl) < 0) {
    fprintf(stderr,"rtkgps: Error reading information for file %d [%s]\n",
	    flnm, gcstrerror(rcerrno));
    free(fnam);
    outlog_enable(fd, status->gpsms, cmdopt);
    coms_close(fd, cmdopt);
    exit(5);
  }

  /* If memory is allocated for the file name, the output path is a
      destination directory. Construct a standard file path with the
      directory as a base, and open the file for writing */
  if (fnam != NULL) {

#if !defined(FILENAME_DATE_PTR)
    if (get_file_start_time(fd, &lgfl, &dt) != 1) {
      fprintf(stderr,"rtkgps: Error reading initial time for file %d [%s]\n",
	      flnm, gcstrerror(rcerrno));
      free(fnam);
      outlog_enable(fd, status->gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(5);
    }
#endif

    /* Add filename postfix to indicate file is not complete (data
       still being captured). */
    if (flnm == status->nfile-1)
      pstr = "_part";
    /* Construct file name */
#if defined(FILENAME_DATE_PTR)
    sprintf(fnam, "%s/%8.8s_%06x%s.%s", cmdopt->dsts, lgfl.date, 
	    lgfl.memp, pstr, (cmdopt->nflg)?"rngl":"nmea");
#else
    sprintf(fnam, "%s/%8.8sT%6.6sZ%s.%s", cmdopt->dsts, lgfl.date, 
	    dt.time, pstr, (cmdopt->nflg)?"rngl":"nmea");
#endif
    /* Skip existing files if requested */
    if (cmdopt->uflg && is_nzsregfile(fnam) && flnm != status->nfile-1) {
      if (cmdopt->vflg)
	printf("Skipping download for file %4d\n", flnm);
      return;
    }

    /* Create backup of output file if it already exists */
    if (file_backup(fnam) != 0) {
      fprintf(stderr,"rtkgps: Error creating backup of file %s\n", fnam);
      free(fnam);
      outlog_enable(fd, status->gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(3);
    }
    /* Attempt to open file */
    if ((strm = fopen(fnam, "w")) == NULL) {
      fprintf(stderr,"rtkgps: Error opening output file %s [%s]\n",
	      fnam, gcstrerror(rcerrno));
      free(fnam);
      outlog_enable(fd, status->gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(3);
    }
  }

  /* Allocate memory for the log file data */
  if ((gfxp = malloc(lgfl.nfix*sizeof(gps_fix_t))) == NULL) {
    fprintf(stderr,"rtkgps: Error allocating memory\n");
    free(fnam);
    if (fnam != NULL)
      fclose(strm);
    outlog_enable(fd, status->gpsms, cmdopt);
    coms_close(fd, cmdopt);
    exit(2);
  }

#ifdef GEOIDCOR
  /* If necessary, allocate memory for geoid correction values */
  if (lgfl.fxtyp > 0 && gdhtp->filep != NULL) {
    if ((gcrp = malloc(lgfl.nfix*sizeof(float))) == NULL) {
      fprintf(stderr,"rtkgps: Error allocating memory\n");
      free(fnam);
      if (fnam != NULL)
	fclose(strm);
      outlog_enable(fd, status->gpsms, cmdopt);
      coms_close(fd, cmdopt);
      exit(2);
    }
  }
#endif

  if (cmdopt->vflg)
    printf("Requesting content of file   %4d\n", flnm);

  /* Read the log file data */
  fn = get_file_data(fd, &lgfl, gfxp);
  if (fn < 0) {
    fprintf(stderr,"rtkgps: Error reading file %d [%s]\n",
	    flnm, gcstrerror(rcerrno));
    /* Remove empty file so that it isn't skipped (with -u flag) on
       read restart */
    if (fnam != NULL)
      remove(fnam);
    free(gcrp);
    free(gfxp);
    free(fnam);
    if (fnam != NULL)
      fclose(strm);
    outlog_enable(fd, status->gpsms, cmdopt);
    coms_close(fd, cmdopt);
    exit(5);
  }

#ifdef GEOIDCOR
  if (gcrp != NULL) {
    const double dgrd = 360.0/(2*M_PI);
    unsigned short n;

    if (cmdopt->vflg)
      printf("Computing geoid altitude corrections\n");
    for (n = 0; n < lgfl.nfix; n++) {
      gcrp[n] = geoid_calc_correction(gdhtp, dgrd*gfxp[n].lat,
				      dgrd*gfxp[n].lng);
    }
  }
#endif

  /* Print the log file data to the output stream */
  if (cmdopt->nflg) {
    if (fnam != NULL)
      print_hdr_native(strm);
    print_log_native(strm, &lgfl, gfxp, gcrp);
  }
  else {
    if (fnam != NULL)
      print_hdr_nmea(strm, cmdopt->btas);
    print_log_nmea(strm, &lgfl, gfxp, gcrp);
  }

  /* Free memory for geoid correction values */
  free(gcrp);
  /* Free memory for the log file data */
  free(gfxp);

  /* If memory is allocated for the file name, the output path is a
     directory and the output stream was opened in this function, so
     the stream should be closed here */
  if (fnam != NULL)
    fclose(strm);
}
