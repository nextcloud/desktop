/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006      by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "c_lib.h"
#include "csync_lock.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.lock"
#include "csync_log.h"

static int _csync_lock_create(const char *lockfile) {
  int fd = -1;
  pid_t pid = 0;
  int rc = -1;
  char errbuf[256] = {0};
  char *ctmpfile = NULL;
  char *dir = NULL;
  char *buf = NULL;

  pid = getpid();

  dir = c_dirname(lockfile);
  if (dir == NULL) {
    rc = -1;
    goto out;
  }

  if (asprintf(&ctmpfile, "%s/tmp_lock_XXXXXX", dir) < 0) {
    rc = -1;
    goto out;
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Create temporary lock file: %s", ctmpfile);
  if ((fd = mkstemp(ctmpfile)) < 0) {
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "Unable to create temporary lock file: %s - %s",
        ctmpfile,
        errbuf);
    rc = -1;
    goto out;
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Write pid (%d) to temporary lock file: %s", pid, ctmpfile);
  pid = asprintf(&buf, "%d\n", pid);
  if (write(fd, buf, pid) == pid) {
    /* Create lock file */
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Create a hardlink from %s to %s.", ctmpfile, lockfile);
    if (link(ctmpfile, lockfile) < 0 ) {
      /* Oops, alredy locked */
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO,
          "Already locked: %s - %s",
          lockfile,
          errbuf);
      rc = -1;
      goto out;
    }
  } else {
    strerror_r(errno, errbuf, sizeof(errbuf));
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
        "Can't create %s - %s",
        ctmpfile,
        errbuf);
    rc = -1;
    goto out;
  }

  rc = 0;

out:
  close(fd);
	if (ctmpfile) {
		unlink(ctmpfile);
	}

  SAFE_FREE(buf);
  SAFE_FREE(dir);
  SAFE_FREE(ctmpfile);

  return rc;
}

static pid_t _csync_lock_read(const char *lockfile) {
  char errbuf[256] = {0};
  char buf[8] = {0};
  int  fd, pid;

  /* Read PID from existing lock */
  if ((fd = open(lockfile, O_RDONLY)) < 0) {
     return -1;
  }

  pid = read(fd, buf, sizeof(buf));
  close(fd);

  if (pid <= 0) {
     return -1;
  }

  buf[sizeof(buf) - 1] = '\0';
  pid = strtol(buf, NULL, 10);
  if (!pid || errno == ERANGE) {
     /* Broken lock file */
     strerror_r(errno, errbuf, sizeof(errbuf));
     if (unlink(lockfile) < 0) {
       CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
           "Unable to remove broken lock %s - %s",
           lockfile,
           errbuf);
     }
     return -1;
  }

  /* Check if process is still alive */
  if (kill(pid, 0) < 0 && errno == ESRCH) {
     /* Process is dead. Remove stale lock. */
     if (unlink(lockfile) < 0) {
       strerror_r(errno, errbuf, sizeof(errbuf));
       CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
           "Unable to remove stale lock %s - %s",
           lockfile,
           errbuf);
     }
     return -1;
  }

  return pid;
}

int csync_lock(const char *lockfile) {
  /* Check if lock already exists. */
  if (_csync_lock_read(lockfile) > 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Aborting, another synchronization process is running.");
    return -1;
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "Creating lock file: %s", lockfile);

  return _csync_lock_create(lockfile);
}

void csync_lock_remove(const char *lockfile) {
  char errbuf[256] = {0};
  /* You can't remove the lock if it is from another process */
  if (_csync_lock_read(lockfile) == getpid()) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "Removing lock file: %s", lockfile);
    if (unlink(lockfile) < 0) {
      strerror_r(errno, errbuf, sizeof(errbuf));
      CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR,
          "Unable to remove lock %s - %s",
          lockfile,
          errbuf);
    }
  }
}

