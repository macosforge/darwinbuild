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
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/paths.h>
#include <errno.h>

#define DARWINTRACE_LOG_FULL_PATH 1
#define DARWINTRACE_DEBUG_OUTPUT 0
#define DARWINTRACE_START_FD 81
#define DARWINTRACE_BUFFER_SIZE	1024

#if DARWINTRACE_DEBUG_OUTPUT
#define dprintf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...)
#endif

#define DARWINTRACE_INTERPOSE(_replacement,_replacee) \
__attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

static int darwintrace_fd = -2;
static char darwintrace_progname[DARWINTRACE_BUFFER_SIZE];
static pid_t darwintrace_pid = -1;

/**
 * Redirect file access 
 */
static char *darwintrace_redirect = NULL; 
static char *darwintrace_buildroot = NULL;
static const char *darwintrace_exceptions[] = {
  "/Developer/Library/Private",
  "/Developer/Library/Frameworks",
  "/Developer/usr/bin/../../Library/Private",
  "/Developer/usr/bin/../../Library/Frameworks",
  "/Developer/Library/Xcode",
  "/Developer/Platforms/",
  "/Developer/usr/bin/xcode",
  "/System/Library/Frameworks/Carbon",
  "/Volumes/BuildRoot_",
  "/usr/bin/xcrun",
  "/usr/bin/xcode",
  "/usr/local/share/darwin",
  "/usr/share/xcode",
  "/var/folders/",
  "/var/tmp/",
  "/.vol/",
  "/tmp/",
  "/dev/",
};

/* check if str starts with one of the exceptions */
static inline bool darwintrace_except(const char *str) {
  size_t c = sizeof(darwintrace_exceptions)/sizeof(*darwintrace_exceptions); 
  size_t i;
  for (i = 0; i < c; i++) {				
    if (strncmp(darwintrace_exceptions[i], str, strlen(darwintrace_exceptions[i])) == 0) { 
      return true;
    }
  }
  return false;
}

/* apply redirection heuristic to path */
static inline char* darwintrace_redirect_path(const char* path) {
  if (!darwintrace_redirect) return (char*)path;

  char *redirpath;
  redirpath = (char *)path;
  if (path[0] == '/'
      && !darwintrace_except(path)
      && strncmp(darwintrace_buildroot, path, strlen(darwintrace_buildroot))!=0
      && strncmp(darwintrace_redirect, path, strlen(darwintrace_redirect))!=0 ) {
    asprintf(&redirpath, "%s%s%s", darwintrace_redirect, (*path == '/' ? "" : "/"), path);
    dprintf("darwintrace: redirect %s -> %s\n", path, redirpath);
  }

  return redirpath;
}

/* free path if not the same as test */
static inline void darwintrace_free_path(char* path, const char* test) {
  if (path != test) free(path);
}

static inline void darwintrace_setup() {
	if (darwintrace_fd == -2) {
	  char* path = getenv("DARWINTRACE_LOG");
	  if (path != NULL) {
      int olderrno = errno;
      int fd = open(path,
                    O_CREAT | O_WRONLY | O_APPEND,
                    DEFFILEMODE);
      int newfd;
      for(newfd = DARWINTRACE_START_FD; newfd < DARWINTRACE_START_FD + 21; newfd++) {
        if(-1 == write(newfd, "", 0) && errno == EBADF) {
          if(-1 != dup2(fd, newfd))
            darwintrace_fd = newfd;
          close(fd);
          fcntl(darwintrace_fd, F_SETFD, 1); /* close-on-exec */
          break;
        }
      }
      errno = olderrno;
	  }

	  /* read env vars needed for redirection */
	  darwintrace_redirect = getenv("DARWINTRACE_REDIRECT");
	  darwintrace_buildroot = getenv("DARWIN_BUILDROOT");
	}

	if (darwintrace_pid == -1) {
		darwintrace_pid = getpid();
		char** progname = _NSGetProgname();
		if (progname && *progname) {
		  if (strlcpy(darwintrace_progname, *progname, sizeof(darwintrace_progname)) >= sizeof(darwintrace_progname)) {
		    dprintf("darwintrace: progname too long to copy: %s\n", *progname);
		  }
		}
	}
}

/* darwintrace_setup must have been called already */
static inline void darwintrace_logpath(int fd, const char *procname, char *tag, const char *path) {
  char darwintrace_buf[DARWINTRACE_BUFFER_SIZE];
  int size = snprintf(darwintrace_buf, sizeof(darwintrace_buf),
                      "%s[%d]\t%s\t%s\n",
                      procname ? procname : darwintrace_progname, darwintrace_pid,
                      tag, path);
  write(fd, darwintrace_buf, size);
  fsync(fd);
}

/* remap resource fork access to the data fork.
 * do a partial realpath(3) to fix "foo//bar" to "foo/bar"
 */
static inline void darwintrace_cleanup_path(char *path) {
  size_t pathlen, rsrclen;
  size_t i, shiftamount;
  enum { SAWSLASH, NOTHING } state = NOTHING;

  /* if this is a foo/..namedfork/rsrc, strip it off */
  pathlen = strlen(path);
  rsrclen = strlen(_PATH_RSRCFORKSPEC);
  if(pathlen > rsrclen
     && 0 == strncmp(path + pathlen - rsrclen,
                     _PATH_RSRCFORKSPEC, rsrclen)) {
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

/* 
   Only logs files where the open succeeds.
   Only logs files opened for read access, without the O_CREAT flag set.
   The assumption is that any file that can be created isn't necessary
   to build the project.
*/
int darwintrace_open(const char* path, int flags, ...) {
	mode_t mode;
	int result;
	va_list args;

	char* redirpath = darwintrace_redirect_path(path);

	va_start(args, flags);
	mode = va_arg(args, int);
	va_end(args);
	result = open(redirpath, flags, mode);
	if (result >= 0 && (flags & (O_CREAT | O_WRONLY)) == 0 ) {
	  darwintrace_setup();
	  if (darwintrace_fd >= 0) {
	    char realpath[MAXPATHLEN];
#if DARWINTRACE_LOG_FULL_PATH
	    int usegetpath = 1;
#else	  
	    int usegetpath = 0;
#endif

	    dprintf("darwintrace: original open path is %s\n", redirpath);

	    /* for volfs paths, we need to do a GETPATH anyway */
	    if(!usegetpath && strncmp(redirpath, "/.vol/", 6) == 0) {
	      usegetpath = 1;
	    }
	    
	    if(usegetpath) {
        if(0 == fcntl(result, F_GETPATH, realpath)) {
          dprintf("darwintrace: resolved %s to %s\n", redirpath, realpath);
	      } else {
          /* use original path */
          dprintf("darwintrace: failed to resolve %s\n", redirpath);
          if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
            dprintf("darwintrace: in open: original path too long to copy: %s\n", redirpath);
          }
        }
      } else {
	      if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
          dprintf("darwintrace: in open (without getpath): path too long to copy: %s\n", redirpath);
        }
      }

	    darwintrace_cleanup_path(realpath);
	    darwintrace_logpath(darwintrace_fd, NULL, "open", realpath);
	  }
	}

	darwintrace_free_path(redirpath, path);
	return result;
}
DARWINTRACE_INTERPOSE(darwintrace_open, open)


/* 
   Only logs files where the readlink succeeds.
*/
ssize_t darwintrace_readlink(const char * path, char * buf, size_t bufsiz) {
	ssize_t result;

	char* redirpath = darwintrace_redirect_path(path);
	result = readlink(redirpath, buf, bufsiz);
	if (result >= 0) {
	  darwintrace_setup();
	  if (darwintrace_fd >= 0) {
	    char realpath[MAXPATHLEN];

	    dprintf("darwintrace: original readlink path is %s\n", redirpath);

	    if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
	      dprintf("darwintrace: in readlink: path too long to copy: %s\n", redirpath);
	    }
	    
	    darwintrace_cleanup_path(realpath);
	    darwintrace_logpath(darwintrace_fd, NULL, "readlink", realpath);
	  }
	}
  
	darwintrace_free_path(redirpath, path);
	return result;
}
DARWINTRACE_INTERPOSE(darwintrace_readlink, readlink)


int darwintrace_execve(const char* path, char* const argv[], char* const envp[]) {
	int result;
	
	char* redirpath = darwintrace_redirect_path(path);
	darwintrace_setup();
	if (darwintrace_fd >= 0) {
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

	  dprintf("darwintrace: original execve path is %s\n", redirpath);

	  /* for symlinks, we wan't to capture
	   * both the original path and the modified one,
	   * since for /usr/bin/gcc -> gcc-4.0,
	   * both "gcc_select" and "gcc" are contributors
	   */
	  if (lstat(redirpath, &sb) == 0) {
	    if(redirpath[0] != '/') {
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
	      if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
          dprintf("darwintrace: in execve: path too long to copy: %s\n", redirpath);
	      }

	      darwintrace_cleanup_path(realpath);
	      darwintrace_logpath(darwintrace_fd, NULL, "execve", realpath);
	    }
		
	    fd = open(redirpath, O_RDONLY, 0);
	    if (fd != -1) {

	      char buffer[MAXPATHLEN];
	      ssize_t bytes_read;

	      /* once we have an open fd, if a full path was requested, do it */
	      if(printreal) {

          if(usegetpath) {
            if(0 == fcntl(fd, F_GETPATH, realpath)) {
              dprintf("darwintrace: resolved execve path %s to %s\n", redirpath, realpath);
            } else {
              dprintf("darwintrace: failed to resolve %s\n", redirpath);
              if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
                dprintf("darwintrace: in execve: original path too long to copy: %s\n", redirpath);
              }
            }
          } else {
            if (strlcpy(realpath, redirpath, sizeof(realpath)) >= sizeof(realpath)) {
              dprintf("darwintrace: in execve (without getpath): path too long to copy: %s\n", redirpath);
            }
          }
          
          darwintrace_cleanup_path(realpath);
          darwintrace_logpath(darwintrace_fd, NULL, "execve", realpath);
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

            /* look for slash to get the basename */
            procname = strrchr(argv[0], '/');
            if (procname == NULL) {
              /* no slash found, so assume whole string is basename */
              procname = argv[0];
            } else {
              /* advance pointer to just after slash */
              procname++;
            }

            darwintrace_cleanup_path(interp);
            darwintrace_logpath(darwintrace_fd, procname, "execve", interp);
          }
	      }
        
	      close(fd);
	    }
	  }
	}
  
	result = execve(redirpath, argv, envp);
	darwintrace_free_path(redirpath, path);
	return result;
}
DARWINTRACE_INTERPOSE(darwintrace_execve, execve)


/* 
   if darwintrace has been initialized, trap
   attempts to close our file descriptor
*/
int darwintrace_close(int fd) {
  if(darwintrace_fd != -2 && fd == darwintrace_fd) {
    errno = EBADF;
    return -1;
  }

  return close(fd);
}
DARWINTRACE_INTERPOSE(darwintrace_close, close)
