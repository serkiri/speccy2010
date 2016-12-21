/*
 * Stub version of execve.
 */

#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#undef errno
extern int errno;

int _execve( char  *name, char **argv, char **env )
{
  errno = ENOSYS;
  return -1;
}
