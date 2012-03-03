/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef VERSION_H
#define VERSION_H

#define MIRALL_STRINGIFY(s) MIRALL_TOSTRING(s)
#define MIRALL_TOSTRING(s) #s

/* MIRALL version macros */
#define MIRALL_VERSION_INT_(a, b, c) ((a) << 16 | (b) << 8 | (c))
#define MIRALL_VERSION_DOT(a, b, c) a ##.## b ##.## c
#define MIRALL_VERSION_(a, b, c) MIRALL_VERSION_DOT(a, b, c)

/* MIRALL version */
#define MIRALL_VERSION_MAJOR  0
#define MIRALL_VERSION_MINOR  2
#define MIRALL_VERSION_MICRO  2


#define MIRALL_VERSION_INT MIRALL_VERSION_INT_(MIRALL_VERSION_MAJOR, \
                                           MIRALL_VERSION_MINOR, \
                                           MIRALL_VERSION_MICRO)
#define MIRALL_VERSION     MIRALL_VERSION_(MIRALL_VERSION_MAJOR, \
                                           MIRALL_VERSION_MINOR, \
                                           MIRALL_VERSION_MICRO)


#endif // VERSION_H
