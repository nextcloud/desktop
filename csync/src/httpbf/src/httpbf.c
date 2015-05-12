/*
 * httpbf - send big files via http
 *
 * Copyright (c) 2012 Klaas Freitag <freitag@owncloud.com>
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <inttypes.h>

#include "httpbf.h"

#include <neon/ne_session.h>
#include <neon/ne_request.h>
#include <neon/ne_basic.h>

// #ifdef NDEBUG
// #define DEBUG_HBF(...)
// #else
#define DEBUG_HBF(...) { if(transfer->log_cb) { \
        char buf[1024];                         \
        snprintf(buf, 1024, __VA_ARGS__);       \
        transfer->log_cb(__func__, buf, transfer->user_data);    \
  }  }

// #endif

#define DEFAULT_BLOCK_SIZE (10*1024*1024)

/* Platform specific defines go here. */
#ifdef _WIN32
#define _hbf_fstat _fstat64
typedef struct stat64 hbf_stat_t;
#else
#define _hbf_fstat fstat
typedef struct stat hbf_stat_t;
#endif

static int transfer_id( hbf_stat_t *sb ) {
    struct timeval tp;
    int res;
    int r;

    if( gettimeofday(&tp, 0) < 0 ) {
        return 0;
    }

    /* build a Unique ID:
     * take the current epoch and shift 8 bits up to keep the least bits.
     * than add the milliseconds, again shift by 8
     * and finally add the least 8 bit of the inode of the file.
     */
    res = tp.tv_sec; /* epoche value in seconds */
    res = res << 8;
    r = (sb->st_ino & 0xFF);
    res += r; /* least six bit of inode */
    res = res << sizeof(tp.tv_usec);
    res += tp.tv_usec; /* milliseconds */

    return res;
}

hbf_transfer_t *hbf_init_transfer( const char *dest_uri ) {
    hbf_transfer_t * transfer = NULL;

    transfer = malloc( sizeof(hbf_transfer_t) );
    memset(transfer, 0, sizeof(hbf_transfer_t));

    /* store the target uri */
    transfer->url = strdup(dest_uri);
    transfer->status_code = 200;
    transfer->error_string = NULL;
    transfer->start_id = 0;
    transfer->block_size = DEFAULT_BLOCK_SIZE;
    transfer->threshold = transfer->block_size;
    transfer->modtime_accepted = 0;
    transfer->oc_header_modtime = 0;

    return transfer;
}

/* Create the splitlist of a given file descriptor */
Hbf_State hbf_splitlist(hbf_transfer_t *transfer, int fd ) {
  hbf_stat_t sb;
  int64_t num_blocks;
  int64_t blk_size;
  int64_t remainder = 0;

  if( ! transfer ) {
      return HBF_PARAM_FAIL;
  }

  if( fd <= 0 ) {
    DEBUG_HBF("File descriptor is invalid.");
    return HBF_PARAM_FAIL;
  }
  
  if( _hbf_fstat(fd, &sb) < 0 ) {
    DEBUG_HBF("Failed to stat the file descriptor: errno = %d", errno);
    return HBF_FILESTAT_FAIL;
  }
  
  /* Store the file characteristics. */
  transfer->fd        = fd;
  transfer->stat_size = sb.st_size;
  transfer->modtime   = sb.st_mtime;
  transfer->previous_etag = NULL;
#ifndef NDEBUG
  transfer->calc_size = 0;
#endif

  DEBUG_HBF("block_size: %" PRId64 " threshold: %" PRId64 " st_size: %" PRId64, transfer->block_size, transfer->threshold, sb.st_size );


  /* calc the number of blocks to split in */
  blk_size = transfer->block_size;
  if (sb.st_size < transfer->threshold) {
      blk_size = transfer->threshold;
  }

  num_blocks = sb.st_size / blk_size;

  /* there migth be a remainder. */
  remainder = sb.st_size - num_blocks * blk_size;

  /* if there is a remainder, add one block */
  if( remainder > 0 ) {
      num_blocks++;
  }

  /* The file has size 0. There still needs to be at least one block. */
  if( sb.st_size == 0 ) {
    num_blocks = 1;
    blk_size   = 0;
  }

  DEBUG_HBF("num_blocks: %" PRId64 " rmainder: %" PRId64 " blk_size: %" PRId64, num_blocks, remainder, blk_size );


  if( num_blocks ) {
      int cnt;
      int64_t overall = 0;
      /* create a datastructure for the transfer data */
      transfer->block_arr = calloc(num_blocks, sizeof(hbf_block_t*));
      transfer->block_cnt = num_blocks;
      transfer->transfer_id = transfer_id(&sb);
      transfer->start_id = 0;

      for( cnt=0; cnt < num_blocks; cnt++ ) {
          /* allocate a block struct and fill */
          hbf_block_t *block = malloc( sizeof(hbf_block_t) );
          memset(block, 0, sizeof(hbf_block_t));

          block->seq_number = cnt;
          if( cnt > 0 ) {
              block->start = cnt * blk_size;
          }
          block->size  = blk_size;
          block->state = HBF_NOT_TRANSFERED;

          /* consider the remainder if we're already at the end */
          if( cnt == num_blocks-1 && remainder > 0 ) {
              block->size = remainder;
          }
          overall += block->size;
          /* store the block data into the result array in the transfer */
          *((transfer->block_arr)+cnt) = block;

          DEBUG_HBF("created block %d   (start: %" PRId64 "  size: %" PRId64 ")", cnt, block->start, block->size );
      }

#ifndef NDEBUG
  transfer->calc_size = overall;
#endif
  }
  return HBF_SUCCESS;
}

void hbf_free_transfer( hbf_transfer_t *transfer ) {
    int cnt;

    if( !transfer ) return;

    for( cnt = 0; cnt < transfer->block_cnt; cnt++ ) {
        hbf_block_t *block = transfer->block_arr[cnt];
        if( !block ) continue;
        if( block->http_error_msg ) free( block->http_error_msg );
        if( block->etag ) free( block->etag );
    }
    free( transfer->block_arr );
    free( transfer->url );
    free( transfer->file_id );
    if( transfer->error_string) free( (void*) transfer->error_string );

    free( transfer );
}

static char* get_transfer_url( hbf_transfer_t *transfer, int indx ) {
    char *res = NULL;

    hbf_block_t *block = NULL;

    if( ! transfer ) return NULL;
    if( indx >= transfer->block_cnt ) return NULL;

    block = transfer->block_arr[indx];
    if( ! block ) return NULL;

    if( transfer->block_cnt == 1 ) {
      /* Just one chunk. We send as an ordinary request without chunking. */
      res = strdup( transfer->url );
    } else {
      char trans_id_str[32];
      char trans_block_str[32];
      char indx_str[32];
      int len = 1; /* trailing zero. */
      int tlen = 0;

      tlen = sprintf( trans_id_str,    "%u", transfer->transfer_id );
      if( tlen < 0 ) {
        return NULL;
      }
      len += tlen;

      tlen = sprintf( trans_block_str, "%u", transfer->block_cnt );
      if( tlen < 0 ) {
        return NULL;
      }
      len += tlen;

      tlen = sprintf( indx_str,        "%u", indx );
      if( tlen < 0 ) {
        return NULL;
      }
      len += tlen;

      len += strlen(transfer->url);
      len += strlen("-chunking---");

      res = malloc(len);

      /* Note: must be %u for unsigned because one does not want '--' */
      if( sprintf(res, "%s-chunking-%u-%u-%u", transfer->url, transfer->transfer_id,
                   transfer->block_cnt, indx ) < 0 ) {
        return NULL;
      }
    }
    return res;
}

/*
 * perform one transfer of one block.
 * returns HBF_TRANSFER_SUCCESS if the transfer of this block was a success
 * returns HBF_SUCCESS if the server aknoweldge that he received all the blocks
 */
static int _hbf_dav_request(hbf_transfer_t *transfer, ne_request *req, int fd, hbf_block_t *blk ) {
    Hbf_State state = HBF_TRANSFER_SUCCESS;
    int res;
    const ne_status *req_status = NULL;
    const char *etag = NULL;

    (void) transfer;

    if( ! (blk && req) ) return HBF_PARAM_FAIL;

    ne_set_request_body_fd(req, fd, blk->start, blk->size);
    DEBUG_HBF("Block: %d , Start: %" PRId64 " and Size: %" PRId64 "", blk->seq_number, blk->start, blk->size );
    res = ne_request_dispatch(req);

    req_status = ne_get_status( req );

    switch(res) {
    case NE_OK:
        blk->state = HBF_TRANSFER_FAILED;
        state = HBF_FAIL;
        etag = 0;
        if( req_status->klass == 2 ) {
            state = HBF_TRANSFER_SUCCESS;
            blk->state = HBF_TRANSFER_SUCCESS;
            etag = ne_get_response_header(req, "ETag");
            if (etag && etag[0]) {
                /* When there is an etag, it means the transfer was complete */
                state = HBF_SUCCESS;

                if( etag[0] == '"' && etag[ strlen(etag)-1] == '"') {
                     int len = strlen( etag )-2;
                     blk->etag = malloc( len+1 );
                     strncpy( blk->etag, etag+1, len );
                     blk->etag[len] = '\0';
                } else {
                    blk->etag = strdup( etag );
                }
            } else {
                /* DEBUG_HBF("OOOOOOOO No etag returned!"); */
            }

            /* check if the server was able to set the mtime already. */
            etag = ne_get_response_header(req, "X-OC-MTime");
            if( etag && strcmp(etag, "accepted") == 0 ) {
                /* the server acknowledged that the mtime was set. */
                transfer->modtime_accepted = 1;
            }

            etag = ne_get_response_header(req, "OC-FileID");
            if( etag ) {
                transfer->file_id = strdup( etag );
            }
        }
        break;
    case NE_AUTH:
            state = HBF_AUTH_FAIL;
            blk->state = HBF_TRANSFER_FAILED;
            break;
        case NE_PROXYAUTH:
            state = HBF_PROXY_AUTH_FAIL;
            blk->state = HBF_TRANSFER_FAILED;
        break;
        case NE_CONNECT:
            state = HBF_CONNECT_FAIL;
            blk->state = HBF_TRANSFER_FAILED;
        break;
        case NE_TIMEOUT:
            state = HBF_TIMEOUT_FAIL;
            blk->state = HBF_TRANSFER_FAILED;
            break;
        case NE_ERROR:
            state = HBF_FAIL;
            blk->state = HBF_TRANSFER_FAILED;
            break;
    }

    blk->http_result_code = req_status->code;
    if( req_status->reason_phrase ) {
        blk->http_error_msg = strdup(req_status->reason_phrase);
    }

    return state;
}

Hbf_State hbf_validate_source_file( hbf_transfer_t *transfer ) {
  Hbf_State state = HBF_SUCCESS;
  hbf_stat_t sb;

  if( transfer == NULL ) {
    state = HBF_PARAM_FAIL;
  }

  if( state == HBF_SUCCESS ) {
    if( transfer->fd <= 0 ) {
      state = HBF_PARAM_FAIL;
    }
  }

  if( state == HBF_SUCCESS ) {
    int rc = _hbf_fstat( transfer->fd, &sb );
    if( rc != 0 ) {
      state = HBF_STAT_FAIL;
    }
  }

  if( state == HBF_SUCCESS ) {
    if( sb.st_mtime != transfer->modtime || sb.st_size != transfer->stat_size ) {
      state = HBF_SOURCE_FILE_CHANGE;
    }
  }
  return state;
}

/* Get the HTTP error code for the last request  */
static int _hbf_http_error_code(ne_session *session) {
    const char *msg = ne_get_error( session );
    char *msg2;
    int err;
    err = strtol(msg, &msg2, 10);
    if (msg == msg2) {
        err = 0;
    }
    return err;
}

static Hbf_State _hbf_transfer_no_chunk(ne_session *session, hbf_transfer_t *transfer, const char *verb) {
    int res;
    const ne_status* req_status;

    ne_request *req = ne_request_create(session, verb ? verb : "PUT", transfer->url);
    if (!req)
        return HBF_MEMORY_FAIL;

    ne_add_request_header( req, "Content-Type", "application/octet-stream");

    ne_set_request_body_fd(req, transfer->fd, 0, transfer->stat_size);
    DEBUG_HBF("HBF: chunking not supported for %s", transfer->url);
    res = ne_request_dispatch(req);
    req_status = ne_get_status( req );

    if (res == NE_OK && req_status->klass == 2) {
        ne_request_destroy(req);
        return HBF_SUCCESS;
    }

    if( transfer->error_string ) free( transfer->error_string );
    transfer->error_string = strdup( ne_get_error(session) );
    transfer->status_code = req_status->code;
    ne_request_destroy(req);
    return HBF_FAIL;
}

Hbf_State hbf_transfer( ne_session *session, hbf_transfer_t *transfer, const char *verb ) {
    Hbf_State state = HBF_TRANSFER_SUCCESS;
    int cnt;

    if( ! session ) {
        state = HBF_SESSION_FAIL;
    }
    if( ! transfer ) {
        state = HBF_SPLITLIST_FAIL;
    }
    if( ! verb ) {
        state = HBF_PARAM_FAIL;
    }

    if(state == HBF_TRANSFER_SUCCESS) {
        DEBUG_HBF("%s request to %s", verb, transfer->url);
    }

    for( cnt=0; state == HBF_TRANSFER_SUCCESS && cnt < transfer->block_cnt; cnt++ ) {
        /* cnt goes from O to block_cnt,  but block_id starts at start_id and wrap around
         * That way if we have not finished uploaded when we reach block_cnt, we re-upload
         * the beginning of the file that the server did not have in cache anymore.
         */
        int block_id = (cnt + transfer->start_id) % transfer->block_cnt;
        hbf_block_t *block = transfer->block_arr[block_id];
        char *transfer_url = NULL;

        if( ! block ) state = HBF_PARAM_FAIL;

        if( transfer->abort_cb ) {
            int do_abort = (transfer->abort_cb)(transfer->user_data);
            if( do_abort ) {
              state = HBF_USER_ABORTED;
              transfer->start_id = block_id  % transfer->block_cnt;
            }
        }

        if( state == HBF_TRANSFER_SUCCESS ) {
            transfer_url = get_transfer_url( transfer, block_id );
            if( ! transfer_url ) {
                state = HBF_PARAM_FAIL;
            }
        }

        if( state == HBF_TRANSFER_SUCCESS ) {
          if( transfer->block_cnt > 1 && cnt > 0 ) {
            /* The block count is > 1, check size and mtime before transmitting. */
            state = hbf_validate_source_file(transfer);
            if( state == HBF_SOURCE_FILE_CHANGE ) {
              /* The source file has changed meanwhile */
            }
          }
        }

        if( state == HBF_TRANSFER_SUCCESS || state == HBF_SUCCESS ) {
            ne_request *req = ne_request_create(session, verb, transfer_url);

            if( req ) {
                char buf[21];

                snprintf(buf, sizeof(buf), "%"PRId64, transfer->stat_size);
                ne_add_request_header(req, "OC-Total-Length", buf);
                if( transfer->oc_header_modtime > 0 ) {
                    snprintf(buf, sizeof(buf), "%"PRId64, transfer->oc_header_modtime);
                    ne_add_request_header(req, "X-OC-Mtime", buf);
                }

                if( transfer->previous_etag ) {
                  ne_add_request_header(req, "If-Match", transfer->previous_etag);
                }

                if( transfer->block_cnt > 1 ) {
                  ne_add_request_header(req, "OC-Chunked", "1");
                  snprintf(buf, sizeof(buf), "%"PRId64, transfer->threshold);
                  ne_add_request_header(req, "OC-Chunk-Size", buf);
                }
                ne_add_request_header( req, "Content-Type", "application/octet-stream");

                state = _hbf_dav_request(transfer,  req, transfer->fd, block );

                if( state != HBF_TRANSFER_SUCCESS && state != HBF_SUCCESS) {
                  if( transfer->error_string ) free( transfer->error_string );
                  transfer->error_string = strdup( ne_get_error(session) );
                  transfer->start_id = block_id  % transfer->block_cnt;
                  /* Set the code of the last transmission. */
                  state = HBF_FAIL;
                  transfer->status_code = transfer->block_arr[block_id]->http_result_code;
                }
                ne_request_destroy(req);

                if (transfer->block_cnt > 1 && state == HBF_SUCCESS && cnt == 0) {
                    /* Success on the first chunk is suspicious.
                       It could happen that the server did not support chunking */
                    int rc = ne_delete(session, transfer_url);
                    if (rc == NE_OK && _hbf_http_error_code(session) == 204) {
                        /* If delete suceeded, it means some proxy strips the OC_CHUNKING header
                           start again without chunking: */
                       free( transfer_url );
                       return _hbf_transfer_no_chunk(session, transfer, verb);
                    }
                }

                if (state == HBF_TRANSFER_SUCCESS && transfer->chunk_finished_cb) {
                    transfer->chunk_finished_cb(transfer, block_id, transfer->user_data);
                }

            } else {
                state = HBF_MEMORY_FAIL;
            }
        }
        free( transfer_url );
    }

    /* do the source file validation finally (again). */
    if( state == HBF_TRANSFER_SUCCESS ) {
        /* This means that no etag was returned on one of the chunks to indicate
         * that the upload was finished. */
        state = HBF_TRANSFER_NOT_ACKED;
    }

    return state;
}

int hbf_fail_http_code( hbf_transfer_t *transfer )
{
  int cnt;

  if( ! transfer ) return 0;

  for( cnt = 0; cnt < transfer->block_cnt; cnt++ ) {
    int block_id = (cnt + transfer->start_id) % transfer->block_cnt;
    hbf_block_t *block = transfer->block_arr[block_id];

    if( block->state != HBF_NOT_TRANSFERED && block->state != HBF_TRANSFER_SUCCESS ) {
      return block->http_result_code;
    }
  }
  return 200;
}

const char *hbf_transfer_etag( hbf_transfer_t *transfer )
{
    int cnt;
    const char *etag = NULL;

    if( ! transfer ) return 0;

    /* Loop over all parts and do a assertion that there is only one etag. */
    for( cnt = 0; cnt < transfer->block_cnt; cnt++ ) {
        int block_id = (cnt + transfer->start_id) % transfer->block_cnt;
        hbf_block_t *block = transfer->block_arr[block_id];
        if( block->etag ) {
            if( etag && strcmp(etag, block->etag) != 0 ) {
                /* multiple etags in the transfer, not equal. */
                DEBUG_HBF( "WARN: etags are not equal in blocks of one single transfer." );
            }
            etag = block->etag;
        }
    }
    return etag;
}

const char *hbf_transfer_file_id( hbf_transfer_t *transfer )
{
    const char *re = NULL;
    if(transfer) {
        re = transfer->file_id;
    }
    return re;
}

const char *hbf_error_string(hbf_transfer_t *transfer, Hbf_State state)
{
    const char *re;
    int cnt;
    switch( state ) {
    case HBF_SUCCESS:
        re = "Ok.";
        break;
    case HBF_NOT_TRANSFERED:   /* never tried to transfer     */
        re = "Block was not yet tried to transfer.";
        break;
    case HBF_TRANSFER:         /* transfer currently running  */
        re = "Block is currently transferred.";
        break;
    case HBF_TRANSFER_FAILED:  /* transfer tried but failed   */
        re = "Block transfer failed.";
        break;
    case HBF_TRANSFER_SUCCESS: /* transfer succeeded.         */
        re = "Block transfer successful.";
        break;
    case HBF_SPLITLIST_FAIL:   /* the file could not be split */
        re = "Splitlist could not be computed.";
        break;
    case HBF_SESSION_FAIL:
        re = "No valid session in transfer.";
        break;
    case HBF_FILESTAT_FAIL:
        re = "Source file could not be stat'ed.";
        break;
    case HBF_PARAM_FAIL:
        re = "Parameter fail.";
        break;
    case HBF_AUTH_FAIL:
        re = "Authentication fail.";
        break;
    case HBF_PROXY_AUTH_FAIL:
        re = "Proxy Authentication fail.";
        break;
    case HBF_CONNECT_FAIL:
        re = "Connection could not be established.";
        break;
    case HBF_TIMEOUT_FAIL:
        re = "Network timeout.";
        break;
    case HBF_MEMORY_FAIL:
        re = "Out of memory.";
        break;
    case HBF_STAT_FAIL:
        re = "Filesystem stat on file failed.";
        break;
    case HBF_SOURCE_FILE_CHANGE:
      re = "Source file changed too often during upload.";
      break;
    case HBF_USER_ABORTED:
        re = "Transmission aborted by user.";
        break;
    case HBF_TRANSFER_NOT_ACKED:
        re = "The server did not provide an Etag.";
        break;
    case HBF_FAIL:
    default:
        for( cnt = 0; cnt < transfer->block_cnt; cnt++ ) {
            int block_id = (cnt + transfer->start_id) % transfer->block_cnt;
            hbf_block_t *block = transfer->block_arr[block_id];

            if( block->state != HBF_NOT_TRANSFERED && block->state != HBF_TRANSFER_SUCCESS
                    && block->http_error_msg != NULL) {
                return block->http_error_msg;
            }
        }
        re = "Unknown error.";
    }
    return re;
}

void hbf_set_abort_callback( hbf_transfer_t *transfer, hbf_abort_callback cb)
{
  if( transfer ) {
    transfer->abort_cb = cb;
  }
}

void hbf_set_log_callback(hbf_transfer_t* transfer, hbf_log_callback cb)
{
    if( transfer ) {
        transfer->log_cb = cb;
    }
}
