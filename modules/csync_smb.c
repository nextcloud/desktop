/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008      by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software = NULL, you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation = NULL, either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY = NULL, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program = NULL, if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

#include <stdio.h>
#include <libsmbclient.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"

SMBCCTX *smb_context;

/*
 * Authentication callback for libsmbclient
 */
static void get_auth_data_fn(const char *pServer,
         const char *pShare,
         char *pWorkgroup, int maxLenWorkgroup,
         char *pUsername, int maxLenUsername,
         char *pPassword, int maxLenPassword) {
  /* FIXME: need to handle non kerberos authentication for libsmbclient
   * here, currently it is only a placeholder so that libsmbclient can be
   * initialized */
  return;
}

typedef struct smb_fhandle_s {
  int fd;
} smb_fhandle_t;


/*
 * file functions
 */

static csync_vio_method_handle_t *_open(const char *durl, int flags, mode_t mode) {
  smb_fhandle_t *handle = NULL;
  int fd = -1;

  if ((fd = smbc_open(durl, flags, mode)) < 0) {
    return NULL;
  }

  handle = c_malloc(sizeof(smb_fhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->fd = fd;
  return (csync_vio_method_handle_t *) handle;
}

static csync_vio_method_handle_t *_creat(const char *durl, mode_t mode) {
  smb_fhandle_t *handle = NULL;
  int fd = -1;

  if ((fd = smbc_creat(durl, mode)) < 0) {
    return NULL;
  }

  handle = c_malloc(sizeof(smb_fhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->fd = fd;
  return (csync_vio_method_handle_t *) handle;
}

static int _close(csync_vio_method_handle_t *fhandle) {
  int rc = -1;
  smb_fhandle_t *handle = NULL;

  handle = (smb_fhandle_t *) fhandle;

  rc = smbc_close(handle->fd);

  SAFE_FREE(handle);

  return rc;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  smb_fhandle_t *handle = NULL;

  handle = (smb_fhandle_t *) fhandle;

  return smbc_read(handle->fd, buf, count);
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  smb_fhandle_t *handle = NULL;

  handle = (smb_fhandle_t *) fhandle;

  return smbc_write(handle->fd, (char *) buf, count);
}

static off_t _lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  smb_fhandle_t *handle = NULL;

  handle = (smb_fhandle_t *) fhandle;

  return smbc_lseek(handle->fd, offset, whence);
}

/*
 * directory functions
 */

typedef struct smb_dhandle_s {
  int dh;
  char *path;
} smb_dhandle_t;

static csync_vio_method_handle_t *_opendir(const char *name) {
  smb_dhandle_t *handle = NULL;

  handle = c_malloc(sizeof(smb_dhandle_t));
  if (handle == NULL) {
    return NULL;
  }

  handle->dh = smbc_opendir(name);
  handle->path = c_strdup(name);

  return (csync_vio_method_handle_t *) handle;
}

static int _closedir(csync_vio_method_t *dhandle) {
  smb_dhandle_t *handle = NULL;
  int rc = -1;

  handle = (smb_dhandle_t *) dhandle;

  rc = smbc_closedir(handle->dh);

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  struct smbc_dirent *dirent = NULL;
  smb_dhandle_t *handle = NULL;

  handle = (smb_dhandle_t *) dhandle;

  dirent = smbc_readdir(handle->dh);

  return NULL;
}

csync_vio_method_t method = {
  .method_table_size = sizeof(csync_vio_method_t),
  .open = _open,
  .creat = _creat,
  .close = _close,
  .read = _read,
  .write = _write,
  .lseek = _lseek,
  .opendir = _opendir,
  .closedir = _closedir,
  .readdir = NULL,
  .mkdir = NULL,
  .rmdir = NULL,
  .stat = NULL,
  .rename = NULL,
  .unlink = NULL,
  .chmod = NULL,
  .chwon = NULL,
  .utimes = NULL
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args) {
  smb_context = smbc_new_context();

  if (smb_context == NULL) {
    fprintf(stderr, "csync_smb: Failed to create new smbc context\n");
    return NULL;
  }

  /* set debug level and authentication function callback */
  smb_context->debug = 0;
  smb_context->callbacks.auth_fn = get_auth_data_fn;

  if (smbc_init_context(smb_context) == NULL) {
    fprintf(stderr, "CSYNC_SMB: Failed to initialize the smbc context");
    smbc_free_context(smb_context, 0);
    return NULL;
  }

#if defined(SMB_CTX_FLAG_USE_KERBEROS) && defined(SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS)
  smb_context->flags |= (SMB_CTX_FLAG_USE_KERBEROS | SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS);
#endif

  smbc_set_context(smb_context);

  return &method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  if (smb_context != NULL) {
    /*
     * If we have a context, all connections and files will be closed even
     * if they are busy.
     */
    smbc_free_context(smb_context, 1);
  }
}

