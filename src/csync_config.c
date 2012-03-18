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

#include "config.h"

#include <iniparser.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_config.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.config"
#include "csync_log.h"

static int _csync_config_copy_default (const char *config) {
    int re = 0;
#ifdef _WIN32
    /* For win32, try to copy the conf file from the directory from where the app was started. */
    char buf[MAX_PATH+1];
    int  len = 0;

    /* Get the path from where the application was started */
    len = GetModuleFileName(NULL, buf, MAX_PATH);
    if(len== 0) {
        re = -1;
    } else {
        /* the path has still owncloud.exe or mirall.exe at the end.
         * find it and copy the name of the conf file instead.       */
        if( c_streq( buf+strlen(buf)-strlen("owncloud.exe"), "owncloud.exe")) {
            strcpy(buf+strlen(buf)-strlen("owncloud.exe"), CSYNC_CONF_FILE );
        }
        if( c_streq( buf+strlen(buf)-strlen("mirall.exe"), "mirall.exe")) {
            strcpy(buf+strlen(buf)-strlen("mirall.exe"), CSYNC_CONF_FILE );
        }

        if(c_copy(buf, config, 0644) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Could not copy /%s to %s", buf, config );
            re = -1;
        }
    }
#else
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Copy %s/config/%s to %s", SYSCONFDIR,
        CSYNC_CONF_FILE, config);
    if (c_copy(SYSCONFDIR "/csync/" CSYNC_CONF_FILE, config, 0644) < 0) {
      if (c_copy(BINARYDIR "/config/" CSYNC_CONF_FILE, config, 0644) < 0) {
        re = -1;
      }
    }
#endif
    return re;
}

int csync_config_load(CSYNC *ctx, const char *config) {
  dictionary *dict;

  /* copy default config, if no config exists */
  if (! c_isfile(config)) {
    if (_csync_config_copy_default(config) < 0) {
      return -1;
    }
  }

  dict = iniparser_load(config);
  if (dict == NULL) {
    return -1;
  }

  ctx->options.max_depth = iniparser_getint(dict, "global:max_depth", MAX_DEPTH);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Config: max_depth = %d",
      ctx->options.max_depth);

  ctx->options.max_time_difference = iniparser_getint(dict,
      "global:max_time_difference", MAX_TIME_DIFFERENCE);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Config: max_time_difference = %d",
      ctx->options.max_time_difference);

  ctx->options.sync_symbolic_links = iniparser_getboolean(dict,
      "global:sync_symbolic_links", 0);
  CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Config: sync_symbolic_links = %d",
      ctx->options.sync_symbolic_links);

  iniparser_freedict(dict);

  return 0;
}

/* vim: set ts=8 sw=2 et cindent: */
