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

#include <ctype.h>

#include "c_lib.h"
#include "c_private.h"
#include "csync_private.h"
#include "csync_config.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.config"
#include "csync_log.h"

enum csync_config_opcode_e {
    COC_UNSUPPORTED = -1,
    COC_MAX_TIMEDIFF,
    COC_MAX_DEPTH,
    COC_WITH_CONFLICT_COPY
};

struct csync_config_keyword_table_s {
    const char *name;
    enum csync_config_opcode_e opcode;
};

static struct csync_config_keyword_table_s csync_config_keyword_table[] = {
    { "max_depth", COC_MAX_DEPTH },
    { "max_time_difference", COC_MAX_TIMEDIFF },
    { "with_confilct_copies", COC_WITH_CONFLICT_COPY },
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
    int re = 0;
    int rc;
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

# ifdef WITH_UNIT_TESTING
    rc = c_copy(BINARYDIR "/config/" CSYNC_CONF_FILE, config, 0644);
# else
    rc = 0;
# endif

    if (rc < 0) {
        rc = c_copy(SYSCONFDIR "/csync/" CSYNC_CONF_FILE, config, 0644);
    }
    if (rc < 0) {
        re = -1;
    }
#endif
    return re;
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
    if (!c_isfile(config)) {
        if (_csync_config_copy_default(config) < 0) {
            return -1;
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
