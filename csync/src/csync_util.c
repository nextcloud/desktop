/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 * Copyright (c) 2012-2013 by Klaas Freitag <freitag@owncloud.com>
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

#include "config_csync.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "c_jhash.h"
#include "csync_util.h"
#include "vio/csync_vio.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.util"
#include "csync_log.h"
#include "csync_statedb.h"

typedef struct {
  const char *instr_str;
  enum csync_instructions_e instr_code;
} _instr_code_struct;

static const _instr_code_struct _instr[] =
{
  { "INSTRUCTION_NONE", CSYNC_INSTRUCTION_NONE },
  { "INSTRUCTION_EVAL", CSYNC_INSTRUCTION_EVAL },
  { "INSTRUCTION_REMOVE", CSYNC_INSTRUCTION_REMOVE },
  { "INSTRUCTION_RENAME", CSYNC_INSTRUCTION_RENAME },
  { "INSTRUCTION_EVAL_RENAME", CSYNC_INSTRUCTION_EVAL_RENAME },
  { "INSTRUCTION_NEW", CSYNC_INSTRUCTION_NEW },
  { "INSTRUCTION_CONFLICT", CSYNC_INSTRUCTION_CONFLICT },
  { "INSTRUCTION_IGNORE", CSYNC_INSTRUCTION_IGNORE },
  { "INSTRUCTION_SYNC", CSYNC_INSTRUCTION_SYNC },
  { "INSTRUCTION_STAT_ERR", CSYNC_INSTRUCTION_STAT_ERROR },
  { "INSTRUCTION_ERROR", CSYNC_INSTRUCTION_ERROR },
  { NULL, CSYNC_INSTRUCTION_ERROR }
};

struct csync_memstat_s {
  int size;
  int resident;
  int shared;
  int trs;
  int drs;
  int lrs;
  int dt;
};

const char *csync_instruction_str(enum csync_instructions_e instr)
{
  int idx = 0;

  while (_instr[idx].instr_str != NULL) {
    if (_instr[idx].instr_code == instr) {
      return _instr[idx].instr_str;
    }
    idx++;
  }

  return "ERROR!";
}


void csync_memstat_check(void) {
  int s = 0;
  struct csync_memstat_s m;
  FILE* fp;

  /* get process memory stats */
  fp = fopen("/proc/self/statm","r");
  if (fp == NULL) {
    return;
  }
  s = fscanf(fp, "%d%d%d%d%d%d%d", &m.size, &m.resident, &m.shared, &m.trs,
      &m.drs, &m.lrs, &m.dt);
  fclose(fp);
  if (s == EOF) {
    return;
  }

  CSYNC_LOG(CSYNC_LOG_PRIORITY_INFO, "Memory: %dK total size, %dK resident, %dK shared",
                 m.size * 4, m.resident * 4, m.shared * 4);
}

void csync_win32_set_file_hidden( const char *file, bool h ) {
#ifdef _WIN32
  const mbchar_t *fileName;
  DWORD dwAttrs;
  if( !file ) return;

  fileName = c_utf8_to_locale( file );
  dwAttrs = GetFileAttributesW(fileName);

  if (dwAttrs != INVALID_FILE_ATTRIBUTES) {
      if (h && !(dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
          SetFileAttributesW(fileName, dwAttrs | FILE_ATTRIBUTE_HIDDEN );
      } else if (!h && (dwAttrs & FILE_ATTRIBUTE_HIDDEN)) {
          SetFileAttributesW(fileName, dwAttrs & ~FILE_ATTRIBUTE_HIDDEN );
      }
  }

  c_free_locale_string(fileName);
#else
    (void) h;
    (void) file;
#endif
}

bool (*csync_file_locked_or_open_ext) (const char*) = 0; // filled in by library user
void set_csync_file_locked_or_open_ext(bool (*f) (const char*));
void set_csync_file_locked_or_open_ext(bool (*f) (const char*)) {
    csync_file_locked_or_open_ext = f;
}

bool csync_file_locked_or_open( const char *dir, const char *fname) {
    char *tmp_uri = NULL;
    bool ret;
    if (!csync_file_locked_or_open_ext) {
        return false;
    }
    if (asprintf(&tmp_uri, "%s/%s", dir, fname) < 0) {
        return -1;
    }
    CSYNC_LOG(CSYNC_LOG_PRIORITY_DEBUG, "csync_file_locked_or_open %s", tmp_uri);
    ret = csync_file_locked_or_open_ext(tmp_uri);
    SAFE_FREE(tmp_uri);
    return ret;
}
