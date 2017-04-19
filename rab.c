/* 
 * Reimplemented Andrew Benchmark (RAB)
 * 
 * This benchmark is not the Modified Andrew Benchmark (MAB), but
 * purports to exercise the same parts of the kernel. Scalability is
 * the goal.  Measuring transactions is cool too.
 * 
 * Operating Systems & Architecture Group
 * University of Texas at Austin - Department of Computer Sciences
 * Copyright 2007-2009. All Rights Reserved.
 * See LICENSE file for license terms.
 * 
 * Primarily written by Alex Benn and Don Porter.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>


#ifdef TX
#include "tx.h"
#else
#define xbegin(a, b) 
#define xend() 
#endif

#define MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

struct timeval s;
int n_source_files;

void start_phase() {
  printf("starting phase...\n");
  gettimeofday(&s, NULL);
#ifdef BIG_TX
  xbegin(TX_LIVE_DANGEROUSLY, NULL);
#endif
}

double end_phase() {
  time_t ds;
  time_t du;
  struct timeval f;
  struct rusage ru;
#ifdef PRE_XEND_TIME
  gettimeofday(&f, NULL);
#endif
#ifdef BIG_TX
  xend();
#endif
#ifndef PRE_XEND_TIME
  gettimeofday(&f, NULL);
#endif
  ds = f.tv_sec - s.tv_sec;
  du = f.tv_usec - s.tv_usec;
  printf("ending phase...\n");
  return (ds) + ((double)du/1000000.0);
}

#if defined TX && !defined BIG_TX
#define lil_xbegin()   xbegin(TX_LIVE_DANGEROUSLY, NULL)
#define lil_xend()     xend()
#else
#define lil_xbegin() 
#define lil_xend() 
#endif

#define PATH_LEN 64
void make_files(int copy_filesize) {
  int i, j;
  char path[PATH_LEN];
  char rand_buf[copy_filesize];
  snprintf(path, PATH_LEN, "srcdir");
  mkdir(path, S_IRWXU);
  
  for (i=0; i<n_source_files; i++) {
    int fd, err;
    ssize_t n_written, n_left;
    for (j=0; j<copy_filesize; j++) {
      long int rnd = random();
      rand_buf[j] = rnd%('~' - ' ') + ' ';
    }
    snprintf(path, PATH_LEN, "srcdir/f%d", i);
    fd = open(path, O_CREAT | O_WRONLY, MODE);
    if (fd < 0) {
      perror("create files\n");
      exit(-100);
    }
    n_left = copy_filesize;
    do {
      n_written = write(fd, rand_buf, n_left);
      if (n_written < 0) {
	perror("write init files\n");
	exit(-100);
      }
    } while (n_left -= n_written);

    do {
      err = close(fd);
      if (err < 0 && errno != -EINTR) {
	perror("close init files\n");
	exit(-100);
      }
    } while (err < 0 && errno == -EINTR);
  }
}

/* each thread has its own directory called d0, d1, etc. */
void prepare(int n_threads, int copy_filesize) {
  int i;
  char path[PATH_LEN];
  make_files(copy_filesize);
  for (i = 0; i<n_threads; i++) {
    snprintf(path, PATH_LEN, "d%d", i);
    mkdir(path, S_IRWXU);
  }
}

void cleanup() {
  system("rm -rf d* srcdir");
}

#define P_PATH_LEN 256
/* phase 1: do a bunch of mkdirs */
void phase_1(int n_iters, int id) {
  char path[P_PATH_LEN];
  int i;
  for (i=0; i<n_iters; i++) {
    snprintf(path, P_PATH_LEN, "d%d/sd%d", id, i);
    mkdir(path, S_IRWXU);
  }
}

#define P2_SOURCE_DIR "srcdir"

/* const char *copy_targets[] = { */
/*     "arbitrator.c", "bres.s", "cmr.c", "cvtalto.c", "cvtfont.c",  */
/*     "cvtv1v2.c", "display.h", "DrawString.c", "filelist", "font.c",  */
/*     "font.h", "fontmanip.c", "fontmanip.h", "fontnaming.c", */
/*     "font.v0.h", "font.v1.h", "framebuf.h", "gacha12.c", "graphicops.c",  */
/*     "gxfind.c", "keymap.h", "logo.c", "makefile", "MakeLoadable.c",  */
/*     "menu.c", "menu.h", "mkfont.c", "mksail7.c", "mouse.c", "mousedd.c",  */
/*     "profile.c", "ProgramName.c", "putenv.c", "RasterFile.h", */
/*     "rasterop.c", "sail7.c", "sun1bw.c", "sun1color.c", "sunbitmap.c",  */
/*     "suncolor.c", "test.c", "timetest.c", "towindow.c", "UserDrawS.c",  */
/*     "usergraphics.c", "usergraphics.h", "util.h", "vec.c", "window.c",  */
/*     "window.h", "windowops.c", "wm.c", */
/*     "include/assert.h", "include/colorbuf.h", "include/ctype.h",  */
/*     "include/errno.h", "include/fcntl.h", "include/netdb.h",  */
/*     "include/sgtty.h", "include/signal.h", "include/stdio.h", "include/time.h", */
/*     "include/netinet/in.h", "include/sys/dir.h", "include/sys/ioctl.h",  */
/*     "include/sys/socket.h", "include/sys/stat.h", "include/sys/ttychars.h",  */
/*     "include/sys/ttydev.h", "include/sys/types.h", "include/sys/wait.h" */
/*   }; */
/* const int n_copy_targets = 71; */
#define BUF_SIZE 4*1048576

/* phase 2: copy a bunch of files from srcdir */

void phase_2(int n_iters, int id) {
  char srcpath[P_PATH_LEN];
  char dstpath[P_PATH_LEN];
  char buf[BUF_SIZE];
  int dstfds[n_iters];
  int i, j, err, bytes_left, nchars;
  int fdin;

  for(j=0; j<n_source_files; j++) {
    snprintf(srcpath, P_PATH_LEN, "%s/f%d", P2_SOURCE_DIR, j);
    fdin = open(srcpath, O_RDONLY, MODE);
    if (fdin < 0){
      fprintf(stderr, "err opening %s\n",srcpath);
      exit(-100);
    }
    for (i=0; i<n_iters; i++) {
      snprintf(dstpath, P_PATH_LEN, "d%d/sd%d/c%d", id, i, j);
      dstfds[i] = open(dstpath, O_CREAT | O_WRONLY, MODE);
      if (dstfds[i] < 0) {
	fprintf(stderr, "err opening %s\n",dstpath);
	exit(-100);
      }
    }

    while(nchars = read(fdin, buf, BUF_SIZE)) {
      for (i=0; i<n_iters; i++) {
	bytes_left = nchars;
	do {
	  char *buf_left = buf;
	  err = write(dstfds[i], buf_left, bytes_left);
	  if (err < 0) {
	    fprintf(stderr, "err writing %s", srcpath);
	    exit(-100);
	  }
	  bytes_left -= err;
	  buf_left += err;
	} while (bytes_left > 0);
      }
    }

    for (i=0; i<n_iters; i++) {
      err = close(dstfds[i]);
      if (err < 0) {
	perror("err doing close");
	exit(-100);
      }
    }
    err = close(fdin);
    if (err < 0) {
      perror("err doing close");
      exit(-100);
    }
  }
}

/* phase 3: du on the resulting file tree
 */
void phase_3(int n_iters, int id) {
  char dustr[P_PATH_LEN];
  snprintf(dustr, P_PATH_LEN, "du d%d > /dev/null", id);
  system(dustr);
}

/* phase 4: grep and sum on the resulting file tree
 */
void phase_4(int n_iters, int id) {
  char grepstr[P_PATH_LEN];
  int i;
  snprintf(grepstr, P_PATH_LEN, "grep -r xoxoxoxo d%d > /dev/null", id);
  system(grepstr);
  for (i=0; i<n_iters; i++) {
    snprintf(grepstr, P_PATH_LEN, "cd d%d; echo \"sum sd%d/* &> /dev/null\" | bash",id, i);
    system(grepstr);
  }
}

#define N_PROCESSORS 4 /* bad bad bad bad bad hack */

/* phase: infrastructure for spawning multiple parallel threads
 * executing a function
 * phase_func's arguments are the number of iterations and a unique
 * id used to identify which subdirectory to perform ops in
 */
double phase(int n_iters, int n_threads, void (*phase_func)(int, int)) {
  int i;
  double duration = 0.0;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  start_phase();
  for (i=0; i<n_threads; i++) {
    int pid = fork();
    if (pid == 0) {
      CPU_SET(i%N_PROCESSORS, &mask);
      sched_setaffinity(0, sizeof(cpu_set_t), &mask);
      lil_xbegin();
      phase_func(n_iters, i);
      lil_xend();
      exit(0);
    } else if (pid < 0) {
      perror("fork");
      exit(-100);
    }
  }

  for (i=0; i < n_threads; i++) {
    int stat, retval;
    do {
      retval = waitpid(-1, &stat, __WALL);
      if (retval == -1) {
	perror("waitpid");
	exit(-100);
      }
    } while (!WIFEXITED(stat) && !WIFSIGNALED(stat));
  }

  duration = end_phase();
  return duration;
}

int flush_caches()
{
  int fd, r, i;

  sync();
  sync();
  sync();
  fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
  if(!fd){
    printf("Failed to open drop_caches: %d\n", errno);
    exit(1);
  }
  r = write(fd, "3", 1);
  if(r != 1){
    printf("Failed to write drop_caches: %d\n", errno);
    exit(1);
  }

  close(fd);
}

int main (int argc, char * argv[])
{
  int n_iters;
  int n_threads = 1;
  int n_bytes_to_copy = 1024;
  double duration;
  if (argc < 2) {
    printf("Usage: %s <num iterations> [<num threads, default 1>, [<n files to copy, default 100>, [<n bytes per file, default 1k>]]]\n", argv[0]);
    exit(-100);
  }

  n_source_files = 100;

  n_iters = atoi(argv[1]);
  if (argc >= 3) {
    n_threads = atoi(argv[2]);
    if (argc >= 4) {
      n_source_files = atoi(argv[3]);
      if (argc >= 5) {
	n_bytes_to_copy = atoi(argv[4]);
      }
    }
  }

  flush_caches();

  printf("n subdirs=%d, n input files=%d, size of input file=%d bytes, \n"
	 "n copies=%d, n threads=%d\n",
	 n_iters*40, n_source_files, n_bytes_to_copy, n_iters, n_threads);

  prepare(n_threads, n_bytes_to_copy); /* 100 files, 1k long */

  /* PHASE 1 */
  printf("Phase 1: mkdir\n");
  duration = phase(n_iters * 40, n_threads, &phase_1); 
  /* 40 is to make phase 1 comparable in size to other phases */
  printf("duration: %g\n", duration);

  /* PHASE 2 */
  printf("Phase 2: copy\n");
  duration = phase(n_iters, n_threads, &phase_2);
  printf("duration: %g\n", duration);

  /* PHASE 3 */
  printf("Phase 3: du\n");
  duration = phase(n_iters, n_threads, &phase_3);
  printf("duration: %g\n", duration);

  /* PHASE 4 */

  /* This phase would do something linear in the size of the files.
   * Discussed possibilities include grep and sum.
   */
  
  printf("Phase 4: grep and sum\n");
  duration = phase(n_iters, n_threads, &phase_4);
  printf("duration: %g\n", duration);
  
  cleanup();
  return 0;
}
