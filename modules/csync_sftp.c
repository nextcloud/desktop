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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libssh/sftp.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_SFTP(x)
#else
#define DEBUG_SFTP(x) printf x
#endif

int connected;
SSH_SESSION *ssh_session;
SFTP_SESSION *sftp_session;

csync_auth_callback _authcb;
void *_userdata;

static int _ssh_auth_callback(const char *prompt, char *buf, size_t len,
    int echo, int verify, void *userdata) {
  if (_authcb != NULL) {
    return (*_authcb) (prompt, buf, len, echo, verify, userdata);
  }

  return -1;
}

static int auth_kbdint(SSH_SESSION *session){
  char *name = NULL;
  char *instruction = NULL;
  char *prompt = NULL;
  char buffer[256] = {0};
  int err = SSH_AUTH_ERROR;

  err = ssh_userauth_kbdint(session, NULL, NULL);
  while (err == SSH_AUTH_INFO) {
    int n = 0;
    int i = 0;

    name = ssh_userauth_kbdint_getname(session);
    instruction = ssh_userauth_kbdint_getinstruction(session);
    n = ssh_userauth_kbdint_getnprompts(session);

    if (strlen(name) > 0) {
      printf("%s\n", name);
    }

    if (strlen(instruction) > 0) {
      printf("%s\n", instruction);
    }

    for (i = 0; i < n; ++i) {
      char echo;

      prompt = ssh_userauth_kbdint_getprompt(session, i, &echo);
      if (echo) {
        (*_authcb) (prompt, buffer, sizeof(buffer), 1, 0, NULL);
        ssh_userauth_kbdint_setanswer(session, i, buffer);
        ZERO_STRUCT(buffer);
      } else {
        (*_authcb) ("Password:", buffer, sizeof(buffer), 0, 0, NULL);
        ssh_userauth_kbdint_setanswer(session, i, buffer);
        ZERO_STRUCT(buffer);
      }
    }
    err = ssh_userauth_kbdint(session, NULL, NULL);
  }

  return err;
}

static int _sftp_portable_to_errno(int sftp_errno) {
  int rc = 0;

  switch(sftp_errno) {
    case SSH_FX_OK:
      break;
    case SSH_FX_NO_SUCH_FILE:
    case SSH_FX_NO_SUCH_PATH:
      rc = ENOENT;
      break;
    case SSH_FX_PERMISSION_DENIED:
      rc = EACCES;
      break;
    case SSH_FX_FILE_ALREADY_EXISTS:
      rc = EEXIST;
      break;
    case SSH_FX_INVALID_HANDLE:
      rc = EBADF;
      break;
    case SSH_FX_OP_UNSUPPORTED:
    case SSH_FX_BAD_MESSAGE:
      rc = EINVAL;
      break;
    default:
      rc = EIO;
      break;
  }

  return rc;
}

static int _sftp_connect(const char *uri) {
  SSH_OPTIONS *options = NULL;
  char *scheme = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  unsigned int port = 0;
  char *path = NULL;
  unsigned char hash[MD5_DIGEST_LEN];
  int rc = -1;
  int auth = SSH_AUTH_ERROR;
  int state = SSH_SERVER_ERROR;

  if (connected) {
    return 0;
  }

  rc = c_parse_uri(uri, &scheme, &user, &passwd, &host, &port, &path);
  if (rc < 0) {
    goto out;
  }

  DEBUG_SFTP(("csync_sftp - conntecting to: %s\n", host));

  options = ssh_options_new();
  if (options == NULL) {
    fprintf(stderr, "csync_sftp - error creating options: %s\n", strerror(errno));
    rc = -1;
    goto out;
  }

  /* set the option to connect to the server */
  ssh_options_allow_ssh1(options, 0);
  ssh_options_allow_ssh2(options, 1);

  /* set a 10 seconds timeout */
  ssh_options_set_timeout(options, 10, 0);

  /* don't use compression */
  ssh_options_set_wanted_algos(options, SSH_COMP_C_S, "none");
  ssh_options_set_wanted_algos(options, SSH_COMP_S_C, "none");

#if 0
  ssh_set_verbosity(1);
#endif

  ssh_options_set_host(options, host);
  if (port) {
    ssh_options_set_port(options, port);
    DEBUG_SFTP(("csync_sftp - port set to: %d\n", port));
  }

  if (user && *user) {
    ssh_options_set_username(options, user);
    DEBUG_SFTP(("csync_sftp - username set to: %s\n", user));
  }

  if (_authcb) {
    ssh_options_set_auth_callback(options, _ssh_auth_callback, _userdata);
  }

  /* connect to the server */
  ssh_session = ssh_new();
  if (ssh_session == NULL) {
    fprintf(stderr, "csync_sftp - error creating new connection: %s\n", strerror(errno));
    rc = -1;
    goto out;
  }

  ssh_set_options(ssh_session, options);
  rc = ssh_connect(ssh_session);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error connecting to the server: %s\n", ssh_get_error(ssh_session));
    ssh_disconnect(ssh_session);
    ssh_session = NULL;
    ssh_finalize();
    goto out;
  }

  /* check the server public key hash */
  state = ssh_is_server_known(ssh_session);
  switch (state) {
    case SSH_SERVER_KNOWN_OK:
      break;
    case SSH_SERVER_KNOWN_CHANGED:
      fprintf(stderr, "csync_sftp - host key for server changed : server's one is now:\n");
      ssh_get_pubkey_hash(ssh_session, hash);
      ssh_print_hexa("csync_sftp - public key hash", hash, MD5_DIGEST_LEN);
      fprintf(stderr,"csync_sftp - for security reason, connection will be stopped\n");
      ssh_disconnect(ssh_session);
      ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_FOUND_OTHER:
      fprintf(stderr, "csync_sftp - the host key for this server was not "
          "found but an other type of key exists.\n");
      fprintf(stderr, "csync_sftp - an attacker might change the default "
          "server key to confuse your client into thinking the key does not "
          "exist\n");
      ssh_disconnect(ssh_session);
      ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_NOT_KNOWN:
      fprintf(stderr,"csync_sftp - the server is unknown. Connect manually to "
          "the host first to retrieve the public key hash\n");
      ssh_disconnect(ssh_session);
      ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_ERROR:
      fprintf(stderr, "%s\n", ssh_get_error(ssh_session));
      ssh_disconnect(ssh_session);
      ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    default:
      break;
  }

  /* TODO: try ssh_userauth_none to get auth methods */

  /* authenticate with the server */
  if (passwd && *passwd) {
    DEBUG_SFTP(("csync_sftp - authenticating with user/password\n"));
    /*
     * This is tunneled cleartext password authentication and possibly needs
     * to be allowed by the ssh server. Set 'PasswordAuthentication yes'
     */
    auth = ssh_userauth_password(ssh_session, user, passwd);
  } else {
    DEBUG_SFTP(("csync_sftp - authenticating with pubkey\n"));
    auth = ssh_userauth_autopubkey(ssh_session, NULL);
  }

  if (auth == SSH_AUTH_ERROR) {
    fprintf(stderr, "csync_sftp - authenticating with pubkey: %s\n",
        ssh_get_error(ssh_session));
    ssh_disconnect(ssh_session);
    ssh_session = NULL;
    ssh_finalize();
    rc = -1;
    goto out;
  }

  if (auth != SSH_AUTH_SUCCESS) {
    if (_authcb != NULL) {
      auth = auth_kbdint(ssh_session);
      if (auth == SSH_AUTH_ERROR) {
        fprintf(stderr,"csync_sftp - authentication failed: %s\n",
            ssh_get_error(ssh_session));
        ssh_disconnect(ssh_session);
        ssh_session = NULL;
        ssh_finalize();
        rc = -1;
        goto out;
      }
    } else {
      ssh_disconnect(ssh_session);
      ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
    }
  }

  DEBUG_SFTP(("csync_sftp - creating sftp channel...\n"));
  /* start the sftp session */
  sftp_session = sftp_new(ssh_session);
  if (sftp_session == NULL) {
    fprintf(stderr, "csync_sftp - sftp error initialising channel: %s\n", ssh_get_error(ssh_session));
    rc = -1;
    goto out;
  }

  rc = sftp_init(sftp_session);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error initialising sftp: %s\n", ssh_get_error(ssh_session));
    goto out;
  }

  DEBUG_SFTP(("csync_sftp - connection established...\n"));
  connected = 1;
  rc = 0;
out:
  SAFE_FREE(scheme);
  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(path);

  return rc;
}

/*
 * file functions
 */

static csync_vio_method_handle_t *_open(const char *uri, int flags, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(sftp_session, path, flags, mode);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static csync_vio_method_handle_t *_creat(const char *uri, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(sftp_session, path, O_CREAT|O_WRONLY|O_TRUNC, mode);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static int _close(csync_vio_method_handle_t *fhandle) {
  int rc = -1;

  rc = sftp_close(fhandle);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  return rc;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  int rc = -1;

  rc = sftp_read(fhandle, buf, count);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  return rc;
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  int rc = -1;

  rc = sftp_write(fhandle, (void *) buf, count);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  return rc;
}

static off_t _lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  /* FIXME: really implement seek for lseek? */
  (void) whence;
  sftp_seek(fhandle, offset);
  return 0;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *_opendir(const char *uri) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_opendir(sftp_session, path);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
  int rc = -1;

  rc = sftp_closedir(dhandle);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  return rc;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  SFTP_ATTRIBUTES *dirent = NULL;
  csync_vio_file_stat_t *fs = NULL;

  /* TODO: consider adding the _sftp_connect function */
  dirent = sftp_readdir(sftp_session, dhandle);
  if (dirent == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
    return NULL;
  }

  fs = c_malloc(sizeof(csync_vio_file_stat_t));
  if (fs == NULL) {
    return NULL;
  }

  fs->name = c_strdup(dirent->name);
  fs->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch (dirent->type) {
    case SSH_FILEXFER_TYPE_REGULAR:
      fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      fs->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case SSH_FILEXFER_TYPE_DIRECTORY:
      fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      fs->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    case SSH_FILEXFER_TYPE_SYMLINK:
    case SSH_FILEXFER_TYPE_SPECIAL:
    case SSH_FILEXFER_TYPE_UNKNOWN:
      break;
  }

  return fs;
}

static int _mkdir(const char *uri, mode_t mode) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_mkdir(sftp_session, path, mode);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _rmdir(const char *uri) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_rmdir(sftp_session, path);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
  SFTP_ATTRIBUTES *attrs;
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  attrs = sftp_lstat(sftp_session, path);
  if (attrs == NULL) {
    rc = -1;
    goto out;
  }

  buf->name = c_basename(path);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
    goto out;
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch (attrs->type) {
    case SSH_FILEXFER_TYPE_REGULAR:
      buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case SSH_FILEXFER_TYPE_DIRECTORY:
      buf->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    case SSH_FILEXFER_TYPE_SYMLINK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    case SSH_FILEXFER_TYPE_SPECIAL:
    case SSH_FILEXFER_TYPE_UNKNOWN:
      buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
      break;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

  buf->mode = attrs->permissions;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

  if (buf->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
    /* FIXME: handle symlink */
    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
  } else {
    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;

  buf->uid = attrs->uid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

  buf->uid = attrs->gid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

  buf->size = attrs->size;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

  buf->atime = attrs->atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = attrs->mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  buf->ctime = attrs->createtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_CTIME;

  rc = 0;
out:
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }
  SAFE_FREE(path);

  return rc;
}

static int _rename(const char *olduri, const char *newuri) {
  char *oldpath = NULL;
  char *newpath = NULL;
  int rc = -1;

  if (_sftp_connect(olduri) < 0) {
    return -1;
  }

  if (c_parse_uri(olduri, NULL, NULL, NULL, NULL, NULL, &oldpath) < 0) {
    rc = -1;
    goto out;
  }

  if (c_parse_uri(newuri, NULL, NULL, NULL, NULL, NULL, &newpath) < 0) {
    rc = -1;
    goto out;
  }

  /* FIXME: workaround cause, sftp_rename can't overwrite */
  sftp_unlink(sftp_session, newpath);
  rc = sftp_rename(sftp_session, oldpath, newpath);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

out:
  SAFE_FREE(oldpath);
  SAFE_FREE(newpath);

  return rc;
}

static int _unlink(const char *uri) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_unlink(sftp_session, path);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _chmod(const char *uri, mode_t mode) {
  SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);
  attrs.permissions = mode;
  attrs.flags |= SSH_FILEXFER_ATTR_PERMISSIONS;

  rc = sftp_setstat(sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
  SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);
  attrs.uid = owner;
  attrs.gid = group;
  attrs.flags |= SSH_FILEXFER_ATTR_OWNERGROUP;

  rc = sftp_setstat(sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _utimes(const char *uri, const struct timeval *times) {
  SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);
  attrs.atime = times[0].tv_sec;
  attrs.atime_nseconds = times[0].tv_usec;

  attrs.mtime = times[1].tv_sec;
  attrs.mtime_nseconds = times[1].tv_usec;
  attrs.flags |= SSH_FILEXFER_ATTR_ACCESSTIME | SSH_FILEXFER_ATTR_MODIFYTIME;

  rc = sftp_setstat(sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

csync_vio_method_t _method = {
  .method_table_size = sizeof(csync_vio_method_t),
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
  DEBUG_SFTP(("csync_sftp - method_name: %s\n", method_name));
  DEBUG_SFTP(("csync_sftp - args: %s\n", args));

  (void) method_name;
  (void) args;

  _authcb = cb;
  _userdata = userdata;

  return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;

  if (sftp_session) {
    sftp_free(sftp_session);
  }
  if (ssh_session) {
    ssh_disconnect(ssh_session);
  }

  ssh_finalize();
}

