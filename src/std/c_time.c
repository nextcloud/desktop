/*
 * c_time - time functions
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
 *
 * vim: ts=2 sw=2 et cindent
 */

#include "c_time.h"

struct timespec c_tspecdiff(struct timespec time1, struct timespec time0) {
  struct timespec ret;
  int xsec = 0;
  int sign = 1;

  if (time0.tv_nsec > time1.tv_nsec) {
    xsec = (int) ((time0.tv_nsec - time1.tv_nsec) / (1E9 + 1));
    time0.tv_nsec -= (long int) (1E9 * xsec);
    time0.tv_sec += xsec;
  }

  if ((time1.tv_nsec - time0.tv_nsec) > 1E9) {
    xsec = (int) ((time1.tv_nsec - time0.tv_nsec) / 1E9);
    time0.tv_nsec += (long int) (1E9 * xsec);
    time0.tv_sec -= xsec;
  }

  ret.tv_sec = time1.tv_sec - time0.tv_sec;
  ret.tv_nsec = time1.tv_nsec - time0.tv_nsec;

  if (time1.tv_sec < time0.tv_sec) {
    sign = -1;
  }

  ret.tv_sec = ret.tv_sec * sign;

  return ret;
}

double c_secdiff(struct timespec clock1, struct timespec clock0) {
  double ret;
  struct timespec diff;

  diff = c_tspecdiff(clock1, clock0);

  ret = diff.tv_sec;
  ret += (double) diff.tv_nsec / (double) 1E9;

  return ret;
}

