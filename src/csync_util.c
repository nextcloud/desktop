/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2006 by Andreas Schneider <mail@cynapses.org>
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

#include "csync_util.h"

#define CSYNC_LOG_CATEGORY_NAME "csync.util"
#include "csync_log.h"

typedef struct {
  const char *instr_str;
  enum csync_instructions_e instr_code;
} _instr_code_struct;

static const _instr_code_struct _instr[] =
{
  { "CSYNC_INSTRUCTION_NONE", CSYNC_INSTRUCTION_NONE },
  { "CSYNC_INSTRUCTION_EVAL", CSYNC_INSTRUCTION_EVAL },
  { "CSYNC_INSTRUCTION_REMOVE", CSYNC_INSTRUCTION_REMOVE },
  { "CSYNC_INSTRUCTION_RENAME", CSYNC_INSTRUCTION_RENAME },
  { "CSYNC_INSTRUCTION_NEW", CSYNC_INSTRUCTION_NEW },
  { "CSYNC_INSTRUCTION_CONFLICT", CSYNC_INSTRUCTION_CONFLICT },
  { "CSYNC_INSTRUCTION_IGNORE", CSYNC_INSTRUCTION_IGNORE },
  { "CSYNC_INSTRUCTION_SYNC", CSYNC_INSTRUCTION_SYNC },
  { "CSYNC_INSTRUCTION_STAT_ERROR", CSYNC_INSTRUCTION_STAT_ERROR },
  { "CSYNC_INSTRUCTION_ERROR", CSYNC_INSTRUCTION_ERROR },
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

