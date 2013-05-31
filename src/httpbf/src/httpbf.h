/**
 * http big file functions
 *
 * Copyright (c) 2012 by Klaas Freitag <freitag@owncloud.com>
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
 **/

#ifndef _HBF_SEND_H
#define _HBF_SEND_H

#include "config.h"
#ifdef NEON_WITH_LFS /* Switch on LFS in libneon. Never remove the NE_LFS! */
#define NE_LFS
#endif

#include <neon/ne_session.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hbf_state_e {
    HBF_SUCCESS,
    HBF_NOT_TRANSFERED,           /* never tried to transfer     */
    HBF_TRANSFER,                 /* transfer currently running  */
    HBF_TRANSFER_FAILED,          /* transfer tried but failed   */
    HBF_TRANSFER_SUCCESS,         /* block transfer succeeded.   */
    HBF_SPLITLIST_FAIL,           /* the file could not be split */
    HBF_SESSION_FAIL,
    HBF_FILESTAT_FAIL,
    HBF_PARAM_FAIL,
    HBF_AUTH_FAIL,
    HBF_PROXY_AUTH_FAIL,
    HBF_CONNECT_FAIL,
    HBF_TIMEOUT_FAIL,
    HBF_MEMORY_FAIL,
    HBF_STAT_FAIL,
    HBF_SOURCE_FILE_CHANGE,
    HBF_USER_ABORTED,
    HBF_FAIL
};

typedef enum hbf_state_e Hbf_State;

typedef struct hbf_block_s hbf_block_t;

struct hbf_block_s {
    int seq_number;

    off_t start;
    off_t size;

    Hbf_State state;
    int http_result_code;
    char *http_error_msg;
    char *etag;

    int tries;
};

/* Callback for to check on abort */
typedef int (*hbf_abort_callback) ();

typedef struct hbf_transfer_s hbf_transfer_t;

struct hbf_transfer_s {
    hbf_block_t **block_arr;
    int block_cnt;
    int fd;
    int transfer_id;
    char *url;
    int start_id;

    int status_code;
    char *error_string;

    off_t stat_size;
    time_t modtime;
    off_t block_size;

    hbf_abort_callback abort_cb;
#ifndef NDEBUG
    off_t calc_size;
#endif
};

hbf_transfer_t *hbf_init_transfer( const char *dest_uri );

Hbf_State hbf_transfer( ne_session *session, hbf_transfer_t *transfer, const char *verb );

Hbf_State hbf_splitlist( hbf_transfer_t *transfer, int fd );

void hbf_free_transfer( hbf_transfer_t *transfer );

const char *hbf_error_string( Hbf_State state );

void hbf_set_abort_callback( hbf_transfer_t *transfer, hbf_abort_callback cb);

/* returns an http (error) code of the transmission. If the transmission
 * succeeded, the code is 200. If it failed, its the error code of the
 * first part transmission that failed.
 */
int hbf_fail_http_code( hbf_transfer_t *transfer );

#ifdef __cplusplus
}
#endif


#endif
