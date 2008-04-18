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
 *
 * vim: ts=2 sw=2 et cindent
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <dlfcn.h> /* dlopen(), dlclose(), dlsym() ... */

#include "csync_private.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.vio.main"
#include "csync_log.h"

int csync_vio_init(CSYNC *ctx, const char *module, const char *args) {
  char *path = NULL;
  char *err = NULL;
  csync_vio_method_t *m;
  csync_vio_method_init_fn init_fn;

#if DEVELOPER
  if (asprintf(&path, "%s/csync_%s.so", SYSCONFDIR "/modules", module) < 0) {
#else
  if (asprintf(&path, "%s/csync_%s.so", PLUGINDIR, module) < 0) {
#endif
    return -1;
  }
  printf("path: %s\n", path);

  ctx->module.handle = dlopen(path, RTLD_LAZY);
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading %s plugin failed - %s",
             module, err);
  }

  SAFE_FREE(path);

  init_fn = dlsym(ctx->module.handle, "vio_module_init");
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading function failed - %s", err);
    return -1;
  }

  ctx->module.finish_fn = dlsym(ctx->module.handle, "vio_module_shutdown");
  if ((err = dlerror()) != NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "loading function failed - %s", err);
    return -1;
  }

  /* get the method struct */
  m = (*init_fn)(module, args);
  if (m == NULL) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s return a NULL method", module);
    return -1;
  }

  /* Some basic checks */
  if (m->method_table_size == 0) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s method table size is 0", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, open)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no open fn", module);
    return -1;
  }

  if (! VIO_METHOD_HAS_FUNC(m, open)) {
    CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "module %s has no stat fn", module);
    return -1;
  }

  ctx->module.method = m;

  return 0;
}

void csync_vio_shutdown(CSYNC *ctx) {
  if (ctx->module.handle != NULL) {
    dlclose(ctx->module.handle);
    ctx->module.handle = NULL;

    ctx->module.method = NULL;
    ctx->module.finish_fn = NULL;
  }
}

