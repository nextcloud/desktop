/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "csync_auth.h"

/** Zero a structure */
#define ZERO_STRUCT(x) memset((char *)&(x), 0, sizeof(x))

int csync_auth(const char *prompt, char *buf, size_t len, int echo, int verify,
    void *userdata) {
  struct termios attr;
  struct termios old_attr;
  int ok = 0;
  int fd = -1;
  char *ptr = NULL;
  char tmp[len];

  /* unused variables */
  (void) userdata;

  ZERO_STRUCT(attr);
  ZERO_STRUCT(old_attr);
  ZERO_STRUCT(tmp);

  /* get local terminal attributes */
  if (tcgetattr(STDIN_FILENO, &attr) < 0) {
    perror("tcgetattr");
    return -1;
  }

  /* save terminal attributes */
  memcpy(&old_attr, &attr, sizeof(attr));
  if((fd = fcntl(0, F_GETFL, 0)) < 0) {
    perror("fcntl");
    return -1;
  }

  /* disable echo */
  if (!echo) {
    attr.c_lflag &= ~(ECHO);
  }

  /* write attributes to terminal */
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr) < 0) {
    perror("tcsetattr");
    return -1;
  }

  /* disable nonblocking I/O */
  if (fd & O_NDELAY) {
    fcntl(0, F_SETFL, fd & ~O_NDELAY);
  }

  /* read the password */
  while (!ok) {
    if (*buf) {
      fprintf(stdout, "%s [%s] ", prompt, buf);
    } else {
      fprintf(stdout, "%s ", prompt);
    }
    fflush(stdout);
    while (! fgets(tmp, len, stdin));

    if ((ptr = strchr(tmp, '\n'))) {
      *ptr = '\0';
    }
    fprintf(stdout, "\n");

    if (*tmp) {
      strncpy(buf, tmp, len);
    }


    if (verify) {
      char key_string[len];

      fprintf(stdout, "\nVerifying, please re-enter. %s", prompt);
      fflush(stdout);
      if (! fgets(key_string, sizeof(key_string), stdin)) {
        clearerr(stdin);
        continue;
      }
      if ((ptr = strchr(key_string, '\n'))) {
        *ptr = '\0';
      }
      fprintf(stdout, "\n");
      if (strcmp(buf, key_string)) {
        printf("\n\07\07Mismatch - try again\n");
        fflush(stdout);
        continue;
      }
    }
    ok = 1;
  }

  /* reset terminal */
  if (tcsetattr(STDIN_FILENO, TCSANOW, &old_attr)) {
    ok = 0;
  }

  /* close fd */
  if (fd & O_NDELAY) {
    fcntl(0, F_SETFL, fd);
  }

  /* force termination */
  buf[len - 1] = '\0';
  ZERO_STRUCT(tmp);

  /* return nonzero if not okay */
  return !ok;
}

/* vim: set ts=8 sw=2 et cindent: */
