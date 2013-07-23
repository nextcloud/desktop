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

#include <errno.h>
#include <stdio.h>
#include <libsmbclient.h>

#include "c_lib.h"
#include "c_private.h"

#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_SMB(x)
#else
#define DEBUG_SMB(x) printf x
#endif

SMBCCTX *smb_context = NULL;
csync_auth_callback _authcb = NULL;
void *_userdata;

/*
 * Authentication callback for libsmbclient
 */
static void get_auth_data_with_context_fn(SMBCCTX *smb_ctx,
    const char *srv,
    const char *shr,
    char *wg, int wglen,
    char *un, int unlen,
    char *pw, int pwlen)
{
  static int try_krb5 = 1;
  char *h;

  (void) smb_ctx;
  (void) shr;
  (void) wg;
  (void) wglen;

  DEBUG_SMB(("csync_smb - user=%s, workgroup=%s, server=%s, share=%s\n",
        un, wg, srv, shr));

  /* Don't authenticate for workgroup listing */
  if (srv == NULL || srv[0] == '\0') {
    DEBUG_SMB(("csync_smb - emtpy server name"));
    return;
  }

  /* Try kerberos authentication if available */
  if (try_krb5 && getenv("KRB5CCNAME")) {
    try_krb5 = 0;

    return;
  }

  /* check for an existing user */
  h = smbc_getUser(smb_ctx);
  if (h != NULL) {
    /* The username is known from the url. */
    DEBUG_SMB(("csync_smb - have username from url: %s\n", h));
    if (snprintf(un, unlen, "%s", h) < 0) {
      /* Even if that fials, go on. */
    }
  } else {
    if (_authcb != NULL) {
      DEBUG_SMB(("csync_smb - execute authentication callback\n"));
      (*_authcb) ("Username:", un, unlen, 1, 0, smbc_getOptionUserData(smb_ctx));
    }
  }

  /* Always ask for the password */
  if (_authcb != NULL) {
    /* Call the passwort prompt */
    (*_authcb) ("Password:", pw, pwlen, 0, 0, smbc_getOptionUserData(smb_ctx));
  }

  DEBUG_SMB(("csync_smb - user=%s, workgroup=%s, server=%s, share=%s\n",
        un, wg, srv, shr));

  try_krb5 = 1;

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

  if (fhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  handle = (smb_fhandle_t *) fhandle;

  rc = smbc_close(handle->fd);

  SAFE_FREE(handle);

  return rc;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  smb_fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return (ssize_t) -1;
  }

  handle = (smb_fhandle_t *) fhandle;

  return smbc_read(handle->fd, buf, count);
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  smb_fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return (ssize_t) -1;
  }

  handle = (smb_fhandle_t *) fhandle;

  return smbc_write(handle->fd, (char *) buf, count);
}

static off_t _lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  smb_fhandle_t *handle = NULL;

  if (fhandle == NULL) {
    errno = EBADF;
    return (off_t) -1;
  }

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
  if (handle->dh < 0) {
    SAFE_FREE(handle);
    return NULL;
  }
  handle->path = c_strdup(name);

  return (csync_vio_method_handle_t *) handle;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
  smb_dhandle_t *handle = NULL;
  int rc = -1;

  if (dhandle == NULL) {
    errno = EBADF;
    return -1;
  }

  handle = (smb_dhandle_t *) dhandle;

  rc = smbc_closedir(handle->dh);

  SAFE_FREE(handle->path);
  SAFE_FREE(handle);

  return rc;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  struct smbc_dirent *dirent = NULL;
  smb_dhandle_t *handle = NULL;
  csync_vio_file_stat_t *file_stat = NULL;

  handle = (smb_dhandle_t *) dhandle;

  errno = 0;
  dirent = smbc_readdir(handle->dh);
  if (dirent == NULL) {
    return NULL;
  }

  file_stat = c_malloc(sizeof(csync_vio_file_stat_t));
  if (file_stat == NULL) {
    return NULL;
  }

  file_stat->name = c_strndup(dirent->name, dirent->namelen);
  file_stat->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch (dirent->smbc_type) {
    case SMBC_FILE_SHARE:
      file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    case SMBC_WORKGROUP:
    case SMBC_SERVER:
    case SMBC_COMMS_SHARE:
    case SMBC_IPC_SHARE:
      break;
    case SMBC_DIR:
    case SMBC_FILE:
      file_stat->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      if (dirent->smbc_type == SMBC_DIR) {
        file_stat->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      } else {
        file_stat->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      }
      break;
    default:
      break;
  }

  return file_stat;
}

static int _mkdir(const char *uri, mode_t mode) {
  return smbc_mkdir(uri, mode);
}

static int _rmdir(const char *uri) {
  return smbc_rmdir(uri);
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
  csync_stat_t sb;

  if (smbc_stat(uri, &sb) < 0) {
    return -1;
  }

  buf->name = c_basename(uri);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
    return -1;
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch(sb.st_mode & S_IFMT) {
    case S_IFBLK:
      buf->type = CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE;
      break;
    case S_IFCHR:
      buf->type = CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE;
      break;
    case S_IFDIR:
      buf->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    case S_IFIFO:
      buf->type = CSYNC_VIO_FILE_TYPE_FIFO;
      break;
    case S_IFLNK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    case S_IFREG:
      buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case S_IFSOCK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    default:
      buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
      break;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

  buf->mode = sb.st_mode;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

  if (buf->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
    /* FIXME: handle symlink */
    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
  } else {
    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;

  buf->device = sb.st_dev;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_DEVICE;

  buf->inode = sb.st_ino;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_INODE;

  buf->nlink = sb.st_nlink;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_LINK_COUNT;

  buf->uid = sb.st_uid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

  buf->gid = sb.st_gid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

  buf->size = sb.st_size;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

  buf->blksize = sb.st_blksize;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_SIZE;

  buf->blkcount = sb.st_blocks;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_BLOCK_COUNT;

  buf->atime = sb.st_atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = sb.st_mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  buf->ctime = sb.st_ctime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;

  return 0;
}

static int _rename(const char *olduri, const char *newuri) {
  return smbc_rename(olduri, newuri);
}

static int _unlink(const char *uri) {
  return smbc_unlink(uri);
}

static int _chmod(const char *uri, mode_t mode) {
  return smbc_chmod(uri, mode);
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
  (void) uri;
  (void) owner;
  (void) group;

  /* FIXME: implement smbc_setxattr() */

  return 0;
}

static int _utimes(const char *uri, const struct timeval *times) {
  return smbc_utimes(uri, (struct timeval *) times);
}

static struct csync_vio_capabilities_s _smb_capabilities = {
    .atomar_copy_support = false
};

static struct csync_vio_capabilities_s *_smb_get_capabilities(void)
{
    return &_smb_capabilities;
}

csync_vio_method_t _method = {
  .method_table_size = sizeof(csync_vio_method_t),
  .get_capabilities = _smb_get_capabilities,
  .open = _open,
  .creat = _creat,
  .close = _close,
  .read = _read,
  .write = _write,
  .lseek = _lseek,
  .opendir = _opendir,
  .closedir = _closedir,
  .readdir = _readdir,
  .mkdir = _mkdir,
  .rmdir = _rmdir,
  .stat = _stat,
  .rename = _rename,
  .unlink = _unlink,
  .chmod = _chmod,
  .chown = _chown,
  .utimes = _utimes
};

csync_vio_method_t *vio_module_init(const char *method_name, const char *args,
    csync_auth_callback cb, void *userdata) {
  smb_context = smbc_new_context();

  DEBUG_SMB(("csync_smb - method_name: %s\n", method_name));
  DEBUG_SMB(("csync_smb - args: %s\n", args));
  (void) method_name;
  (void) args;
  (void) cb;

  if (smb_context == NULL) {
    fprintf(stderr, "csync_smb - failed to create new smbc context\n");
    return NULL;
  }

  if (cb != NULL) {
    _authcb = cb;
  }

  /* set debug level and authentication function callback */
  smbc_setDebug(smb_context, 0);
  smbc_setOptionUserData(smb_context, userdata);
  smbc_setFunctionAuthDataWithContext(smb_context, get_auth_data_with_context_fn);

  /* Kerberos support */
  smbc_setOptionUseKerberos(smb_context, 1);
  smbc_setOptionFallbackAfterKerberos(smb_context, 1);

  DEBUG_SMB(("csync_smb - use kerberos = %d\n",
        smbc_getOptionUseKerberos(smb_context)));
  DEBUG_SMB(("csync_smb - use fallback after kerberos = %d\n",
        smbc_getOptionFallbackAfterKerberos(smb_context)));

  if (smbc_init_context(smb_context) == NULL) {
    fprintf(stderr, "csync_smb - failed to initialize the smbc context");
    smbc_free_context(smb_context, 0);
    smb_context = NULL;

    return NULL;
  }

  DEBUG_SMB(("csync_smb - KRB5CCNAME = %s\n", getenv("KRB5CCNAME") != NULL ?
        getenv("KRB5CCNAME") : "not set"));

  smbc_set_context(smb_context);

  return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;

  if (smb_context != NULL) {
    /*
     * If we have a context, all connections and files will be closed even
     * if they are busy.
     */
    smbc_free_context(smb_context, 1);
    smb_context = NULL;
  }
}

/* vim: set ts=8 sw=2 et cindent: */
