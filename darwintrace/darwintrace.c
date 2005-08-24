/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#include <crt_externs.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/paths.h>
#include <errno.h>

#define DARWINTRACE_SHOW_PROCESS 0
#define DARWINTRACE_LOG_FULL_PATH 1
#define DARWINTRACE_DEBUG_OUTPUT 0

#define START_FD 81
static int __darwintrace_fd = -2;
#define BUFFER_SIZE	1024
#if DARWINTRACE_SHOW_PROCESS
static char __darwintrace_progname[BUFFER_SIZE];
static pid_t __darwintrace_pid = -1;
#endif

#if DARWINTRACE_DEBUG_OUTPUT
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...)
#endif

static inline void __darwintrace_setup() {
	if (__darwintrace_fd == -2) {
	  char* path = getenv("DARWINTRACE_LOG");
	  if (path != NULL) {
		int olderrno = errno;
	    	int fd = open(path,
			      O_CREAT | O_WRONLY | O_APPEND,
			      DEFFILEMODE);
		int newfd;
		for(newfd = START_FD; newfd < START_FD + 21; newfd++) {
		  if(-1 == write(newfd, "", 0) && errno == EBADF) {
		    if(-1 != dup2(fd, newfd))
		      __darwintrace_fd = newfd;
		    close(fd);
		    fcntl(__darwintrace_fd, F_SETFD, 1); /* close-on-exec */
		    break;
		  }
		}
		errno = olderrno;
	  }
	}
#if DARWINTRACE_SHOW_PROCESS
	if (__darwintrace_pid == -1) {
		__darwintrace_pid = getpid();
		char** progname = _NSGetProgname();
		if (progname && *progname) {
			strcpy(__darwintrace_progname, *progname);
		}
	}
#endif
}

/* __darwintrace_setup must have been called already */
static inline void __darwintrace_logpath(int fd, const char *procname, char *tag, const char *path) {
#pragma unused(procname)
  char __darwintrace_buf[BUFFER_SIZE];
  int size;

  size = snprintf(__darwintrace_buf, sizeof(__darwintrace_buf),
#if DARWINTRACE_SHOW_PROCESS
		  "%s[%d]\t"
#endif
		  "%s\t%s\n",
#if DARWINTRACE_SHOW_PROCESS
		  procname ? procname : __darwintrace_progname, __darwintrace_pid,
#endif
		  tag, path );
  
  write(fd, __darwintrace_buf, size);
  fsync(fd);
}

/* remap resource fork access to the data fork.
 * do a partial realpath(3) to fix "foo//bar" to "foo/bar"
 */
static inline void __darwintrace_cleanup_path(char *path) {
  size_t pathlen, rsrclen;
  size_t i, shiftamount;
  enum { SAWSLASH, NOTHING } state = NOTHING;

  /* if this is a foo/..namedfork/rsrc, strip it off */
  pathlen = strlen(path);
  rsrclen = strlen(_PATH_RSRCFORKSPEC);
  if(pathlen > rsrclen
     && 0 == strcmp(path + pathlen - rsrclen,
		    _PATH_RSRCFORKSPEC)) {
    path[pathlen - rsrclen] = '\0';
    pathlen -= rsrclen;
  }

  /* for each position in string (including
     terminal \0), check if we're in a run of
     multiple slashes, and only emit the
     first one
  */
  for(i=0, shiftamount=0; i <= pathlen; i++) {
    if(state == SAWSLASH) {
      if(path[i] == '/') {
	/* consume it */
	shiftamount++;
      } else {
	state = NOTHING;
	path[i - shiftamount] = path[i];
      }
    } else {
      if(path[i] == '/') {
	state = SAWSLASH;
      }
      path[i - shiftamount] = path[i];
    }
  }

  dprintf("darwintrace: cleanup resulted in %s\n", path);
}

/* Log calls to open(2) into the file specified by DARWINTRACE_LOG.
   Only logs if the DARWINTRACE_LOG environment variable is set.
   Only logs files where the open succeeds.
   Only logs files opened for read access, without the O_CREAT flag set.
   The assumption is that any file that can be created isn't necessary
   to build the project.
*/

int open(const char* path, int flags, ...) {
#define open(x,y,z) syscall(SYS_open, (x), (y), (z))
	mode_t mode;
	int result;
	va_list args;

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);
	result = open(path, flags, mode);
	if (result >= 0 && (flags & (O_CREAT | O_WRONLY /*O_RDWR*/)) == 0 ) {
	  __darwintrace_setup();
	  if (__darwintrace_fd >= 0) {
	    char realpath[MAXPATHLEN];
#if DARWINTRACE_LOG_FULL_PATH
	    int usegetpath = 1;
#else	  
	    int usegetpath = 0;
#endif

	    dprintf("darwintrace: original open path is %s\n", path);

	    /* for volfs paths, we need to do a GETPATH anyway */
	    if(!usegetpath && strncmp(path, "/.vol/", 6) == 0) {
	      usegetpath = 1;
	    }
	    
	    if(usegetpath) {
	      if(0 == fcntl(result, F_GETPATH, realpath)) {
		dprintf("darwintrace: resolved %s to %s\n", path, realpath);
	      } else {
		/* use original path */
		dprintf("darwintrace: failed to resolve %s\n", path);
		strcpy(realpath, path);
	      }
	    } else {
		strcpy(realpath, path);
	    }

	    __darwintrace_cleanup_path(realpath);

	    __darwintrace_logpath(__darwintrace_fd, NULL, "open", realpath);
	  }
	}
	return result;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
#define execve(x,y,z) syscall(SYS_execve, (x), (y), (z))
	int result;
	
	__darwintrace_setup();
	if (__darwintrace_fd >= 0) {
	  struct stat sb;
	  char realpath[MAXPATHLEN];
	  int printorig = 0;
	  int printreal = 0;
	  int fd;
#if DARWINTRACE_LOG_FULL_PATH
	  int usegetpath = 1;
#else	  
	  int usegetpath = 0;
#endif

	  dprintf("darwintrace: original execve path is %s\n", path);

	  /* for symlinks, we wan't to capture
	   * both the original path and the modified one,
	   * since for /usr/bin/gcc -> gcc-4.0,
	   * both "gcc_select" and "gcc" are contributors
	   */
	  if (lstat(path, &sb) == 0) {
	    if(path[0] != '/') {
	      /* for relative paths, only print full path */
	      printreal = 1;
	      printorig = 0;
	    } else if(S_ISLNK(sb.st_mode)) {
	      /* for symlinks, print both */
	      printreal = 1;
	      printorig = 1;
	    } else {
	      /* for fully qualified paths, print real */
	      printreal = 1;
	      printorig = 0;
	    }

	    if(printorig) {
	      strcpy(realpath, path);

	      __darwintrace_cleanup_path(realpath);
	      __darwintrace_logpath(__darwintrace_fd, NULL, "execve", realpath);
	    }
		
	    fd = open(path, O_RDONLY, 0);
	    if (fd != -1) {

	      char buffer[MAXPATHLEN];
	      ssize_t bytes_read;

	      /* once we have an open fd, if a full path was requested, do it */
	      if(printreal) {
		
		if(usegetpath) {
		  if(0 == fcntl(fd, F_GETPATH, realpath)) {
		    dprintf("darwintrace: resolved execve path %s to %s\n", path, realpath);
		  } else {
		    dprintf("darwintrace: failed to resolve %s\n", path);
		    strcpy(realpath, path);
		  }
		} else {
		  strcpy(realpath, path);
		}
		__darwintrace_cleanup_path(realpath);

		__darwintrace_logpath(__darwintrace_fd, NULL, "execve", realpath);
	      }

	      bzero(buffer, sizeof(buffer));

	      bytes_read = read(fd, buffer, MAXPATHLEN);
	      if (bytes_read > 2 &&
		  buffer[0] == '#' && buffer[1] == '!') {
		char* interp = &buffer[2];
		int i;
		/* skip past leading whitespace */
		for (i = 2; i < bytes_read; ++i) {
		  if (buffer[i] != ' ' && buffer[i] != '\t') {
		    interp = &buffer[i];
		    break;
		  }
		}
		/* found interpreter (or ran out of data)
		   skip until next whitespace, then terminate the string */
		for (; i < bytes_read; ++i) {
		  if (buffer[i] == ' ' || buffer[i] == '\t' || buffer[i] == '\n') {
		    buffer[i] = 0;
		    break;
		  }
		}
		/* we have liftoff */
		if (interp && interp[0] != '\0') {
		  const char* procname = NULL;
#if DARWINTRACE_SHOW_PROCESS
		  procname = strrchr(argv[0], '/') + 1;
		  if (procname == NULL) procname = argv[0];
#endif
		  __darwintrace_cleanup_path(interp);

		  __darwintrace_logpath(__darwintrace_fd, procname, "execve", interp);
		}
	      }
	      close(fd);
	    }
	  }
	}
	result = execve(path, argv, envp);
	return result;
}
