/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

/* #define _GNU_SOURCE */
#include <stdio.h>
#include <ctype.h>

#include "c_lib.h"
#include "c_private.h"
#include "csync_private.h"
#include "csync_config.h"
#include "csync_misc.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.config"
#include "csync_log.h"

enum csync_config_opcode_e {
    COC_UNSUPPORTED = -1,
    COC_MAX_TIMEDIFF,
    COC_MAX_DEPTH,
    COC_WITH_CONFLICT_COPY,
    COC_TIMEOUT
};

struct csync_config_keyword_table_s {
    const char *name;
    enum csync_config_opcode_e opcode;
};

static struct csync_config_keyword_table_s csync_config_keyword_table[] = {
    { "max_depth", COC_MAX_DEPTH },
    { "max_time_difference", COC_MAX_TIMEDIFF },
    { "with_confilct_copies", COC_WITH_CONFLICT_COPY },
    { "timeout", COC_TIMEOUT },
    { NULL, COC_UNSUPPORTED }
};

static enum csync_config_opcode_e csync_config_get_opcode(char *keyword) {
    int i;

    for (i = 0; csync_config_keyword_table[i].name != NULL; i++) {
        if (strcasecmp(keyword, csync_config_keyword_table[i].name) == 0) {
            return csync_config_keyword_table[i].opcode;
        }
    }

    return COC_UNSUPPORTED;
}

static int _csync_config_copy_default (const char *config) {
    int rc = 0;

#ifdef _WIN32
    /* For win32, try to copy the conf file from the directory from where the app was started. */
    mbchar_t tcharbuf[MAX_PATH+1];
    char *buf;
    int  len = 0;


    /* Get the path from where the application was started */
    len = GetModuleFileNameW(NULL, tcharbuf, MAX_PATH);
    if(len== 0) {
        rc = -1;
    } else {
        char *last_bslash;

        buf = c_utf8_from_locale(tcharbuf);
        /* cut the trailing filename off */
        if ((last_bslash = strrchr(buf, '\\')) != NULL) {
          *last_bslash='\0';
        }

        strncat(buf, "\\" CSYNC_CONF_FILE, MAX_PATH);
        if(c_copy(buf, config, 0644) < 0) {
            CSYNC_LOG(CSYNC_LOG_PRIORITY_ERROR, "Could not copy /%s to %s", buf, config );
            rc = -1;
        }
        c_free_locale_string(buf);
    }
#else
    CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Copy %s/config/%s to %s", SYSCONFDIR,
        CSYNC_CONF_FILE, config);
# ifdef WITH_UNIT_TESTING
    rc = c_copy(BINARYDIR "/config/" CSYNC_CONF_FILE, config, 0644);
# else
    rc = 0;
# endif
    if (c_copy(SYSCONFDIR "/ocsync/" CSYNC_CONF_FILE, config, 0644) < 0) {
      if (c_copy(BINARYDIR "/config/" CSYNC_CONF_FILE, config, 0644) < 0) {
        rc = -1;
      }
    }
#endif
    return rc;
}

static char *csync_config_get_cmd(char **str) {
    register char *c;
    char *r;

    /* Ignore leading spaces */
    for (c = *str; *c; c++) {
        if (! isblank(*c)) {
            break;
        }
    }

    if (*c == '\"') {
        for (r = ++c; *c; c++) {
            if (*c == '\"') {
                *c = '\0';
                goto out;
            }
        }
    }

    for (r = c; *c; c++) {
        if (*c == '\n') {
            *c = '\0';
            goto out;
        }
    }

out:
    *str = c + 1;

    return r;
}

static char *csync_config_get_token(char **str) {
    register char *c;
    char *r;

    c = csync_config_get_cmd(str);

    for (r = c; *c; c++) {
        if (isblank(*c)) {
            *c = '\0';
            goto out;
        }
    }

out:
    *str = c + 1;

    return r;
}

static int csync_config_get_int(char **str, int notfound) {
    char *p, *endp;
    int i;

    p = csync_config_get_token(str);
    if (p && *p) {
        i = strtol(p, &endp, 10);
        if (p == endp) {
            return notfound;
        }
        return i;
    }

    return notfound;
}

static const char *csync_config_get_str_tok(char **str, const char *def) {
    char *p;
    p = csync_config_get_token(str);
    if (p && *p) {
        return p;
    }

    return def;
}

static int csync_config_get_yesno(char **str, int notfound) {
    const char *p;

    p = csync_config_get_str_tok(str, NULL);
    if (p == NULL) {
        return notfound;
    }

    if (strncasecmp(p, "yes", 3) == 0) {
        return 1;
    } else if (strncasecmp(p, "no", 2) == 0) {
        return 0;
    }

    return notfound;
}

static int csync_config_parse_line(CSYNC *ctx,
                                   const char *line,
                                   unsigned int count)
{
    enum csync_config_opcode_e opcode;
    char *s, *x;
    char *keyword;
    size_t len;
    int i;

    x = s = c_strdup(line);
    if (s == NULL) {
        return -1;
    }

    /* Remove trailing spaces */
    for (len = strlen(s) - 1; len > 0; len--) {
        if (! isspace(s[len])) {
            break;
        }
        s[len] = '\0';
    }

    keyword = csync_config_get_token(&s);
    if (keyword == NULL || keyword[0] == '#' ||
        keyword[0] == '\0' || keyword[0] == '\n') {
        free(x);
        return 0;
    }

    opcode = csync_config_get_opcode(keyword);

    switch (opcode) {
        case COC_MAX_DEPTH:
            i = csync_config_get_int(&s, 50);
            if (i > 0) {
                ctx->options.max_depth = i;
            }
            break;
        case COC_MAX_TIMEDIFF:
            i = csync_config_get_int(&s, 10);
            if (i >= 0) {
                ctx->options.max_time_difference = i;
            }
            break;
        case COC_WITH_CONFLICT_COPY:
            i = csync_config_get_yesno(&s, -1);
            if (i > 0) {
                ctx->options.with_conflict_copys = true;
            } else {
                ctx->options.with_conflict_copys = false;
            }
            break;
        case COC_TIMEOUT:
            i = csync_config_get_int(&s, 0);
            if (i > 0) {
                ctx->options.timeout = i;
                CSYNC_LOG(CSYNC_LOG_PRIORITY_TRACE, "Config: timeout = %d",
                      ctx->options.timeout);
            }
            break;
        case COC_UNSUPPORTED:
            CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
                      "Unsupported option: %s, line: %d\n",
                      keyword, count);
            break;
    }

    free(x);
    return 0;
}

int csync_config_parse_file(CSYNC *ctx, const char *config)
{
    unsigned int count = 0;
    char line[1024] = {0};
    char *s;
    FILE *f;

    /* copy default config, if no config exists */
    if (! c_isfile(config)) {
        /* check if there is still one csync.conf left over in $HOME/.csync
        * and copy it over (migration path)
        */
        char *home = NULL;
        char *home_config = NULL;
        char *config_file = NULL;

        /* there is no statedb at the expected place. */
        home = csync_get_user_home_dir();
        if( !c_streq(home, ctx->options.config_dir) ) {
            int rc = -1;

            config_file = c_basename(config);
            if( config_file ) {
                rc = asprintf(&home_config, "%s/%s/%s", home, CSYNC_CONF_DIR, config_file);
                SAFE_FREE(config_file);
            }

            if (rc >= 0) {
                CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "config file %s not found, checking %s",
                            config, home_config);

                /* check the home file and copy to new statedb if existing. */
                if(c_isfile(home_config)) {
                    if (c_copy(home_config, config, 0644) < 0) {
                        /* copy failed, but that is not reason to die. */
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_WARN, "Could not copy config %s => %s",
                                    home_config, config);
                    } else {
                        CSYNC_LOG(CSYNC_LOG_PRIORITY_NOTICE, "Copied %s => %s",
                                    home_config, config);
                    }
                }
            }
            SAFE_FREE(home_config);
        }
        SAFE_FREE(home);
        /* Still install the default one if nothing is there. */
        if( ! c_isfile(config)) {
            if (_csync_config_copy_default(config) < 0) {
                return -1;
            }
        }
    }

    f = fopen(config, "r");
    if (f == NULL) {
        return 0;
    }

    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG,
            "Reading configuration data from %s",
            config);

    s = fgets(line, sizeof(line), f);
    while (s != NULL) {
        int rc;
        count++;

        rc = csync_config_parse_line(ctx, line, count);
        if (rc < 0) {
            fclose(f);
            return -1;
        }
        s = fgets(line, sizeof(line), f);
    }

    fclose(f);

    return 0;
}
