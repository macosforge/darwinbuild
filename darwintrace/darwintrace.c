#include <crt_externs.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>

int __darwintrace_fd = -2;
#define BUFFER_SIZE	1024
char __darwintrace_buf[BUFFER_SIZE];
#if DARWINTRACE_SHOW_PROCESS
char __darwintrace_progname[BUFFER_SIZE];
pid_t __darwintrace_pid = -1;
#endif

inline void __darwintrace_setup() {
	if (__darwintrace_fd == -2) {
	  char* path = getenv("DARWINTRACE_LOG");
	  if (path != NULL) {
		__darwintrace_fd = open(path,
		O_CREAT | O_WRONLY | O_APPEND,
		0666);
		fcntl(__darwintrace_fd, F_SETFD, 1); // close-on-exec
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

// Log calls to open(2) into the file specified by DARWINTRACE_LOG.
// Only logs if the DARWINTRACE_LOG environment variable is set.
// Only logs files where the open succeeds.
// Only logs files opened for read access, without the O_CREAT flag set.
// The assumption is that any file that can be created isn't necessary
// to build the project.

int open(const char* path, int flags, ...) {
// SYS_open 5
#define open(x,y,z) syscall(5, (x), (y), (z))
	va_list args;
	va_start(args, flags);
	mode_t mode = va_arg(args, int);
	va_end(args);
	int result = open(path, flags, mode);
	if (result >= 0 && (flags & (O_CREAT | O_WRONLY /*O_RDWR*/)) == 0 ) {
		__darwintrace_setup();
		if (__darwintrace_fd >= 0) {
		  int size;
		  if(strncmp(path, "/.vol/", 6) == 0) {
		    char realpath[MAXPATHLEN];
		    if(0 == fcntl(result, F_GETPATH, realpath)) {
#if DARWINTRACE_SHOW_PROCESS
		      size = snprintf(__darwintrace_buf, BUFFER_SIZE, "%s[%d]\topen\t%s\n", __darwintrace_progname, __darwintrace_pid, realpath );
		      // printf("resolved %s to %s\n", path, realpath);
#else
		      size = snprintf(__darwintrace_buf, BUFFER_SIZE, "open\t%s\n", realpath );
#endif
		    }
		    // if we can't resolve it, ignore the volfs path
		  } else {
#if DARWINTRACE_SHOW_PROCESS
		    size = snprintf(__darwintrace_buf, BUFFER_SIZE, "%s[%d]\topen\t%s\n", __darwintrace_progname, __darwintrace_pid, path );
#else
			size = snprintf(__darwintrace_buf, BUFFER_SIZE, "open\t%s\n", path );
#endif
		  }
		  write(__darwintrace_fd, __darwintrace_buf, size);
		  fsync(__darwintrace_fd);
		}
	}
	return result;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
// SYS_execve 59
#define execve(x,y,z) syscall(59, (x), (y), (z))
	__darwintrace_setup();
	if (__darwintrace_fd >= 0) {
	  struct stat sb;
	  if (stat(path, &sb) == 0) {
#if DARWINTRACE_SHOW_PROCESS
		int size = snprintf(__darwintrace_buf, BUFFER_SIZE, "%s[%d]\texecve\t%s\n", __darwintrace_progname, __darwintrace_pid, path );
#else
		int size = snprintf(__darwintrace_buf, BUFFER_SIZE, "execve\t%s\n", path );
#endif
		write(__darwintrace_fd, __darwintrace_buf, size);
		fsync(__darwintrace_fd);
	  }
	}
	int result = execve(path, argv, envp);
	return result;
}
