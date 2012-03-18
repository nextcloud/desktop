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
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Copy %s/config/%s to %s", SYSCONFDIR,
        CSYNC_CONF_FILE, config);
    if (c_copy(SYSCONFDIR "/csync/" CSYNC_CONF_FILE, config, 0644) < 0) {
      if (c_copy(BINARYDIR "/config/" CSYNC_CONF_FILE, config, 0644) < 0) {
        return -1;
      }
    }
    return 0;
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
