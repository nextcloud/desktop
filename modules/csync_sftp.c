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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libssh/sftp.h>
#include <libssh/callbacks.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_SFTP(x)
#else
#define DEBUG_SFTP(x) printf x
#endif

ssh_callbacks _ssh_callbacks;
ssh_session _ssh_session;
sftp_session _sftp_session;

csync_auth_callback _authcb;
void *_userdata;
int _connected;

static int _ssh_auth_callback(const char *prompt, char *buf, size_t len,
    int echo, int verify, void *userdata) {
  if (_authcb != NULL) {
    return (*_authcb) (prompt, buf, len, echo, verify, userdata);
  }

  return -1;
}

static int auth_kbdint(ssh_session session, const char *user,
    const char *passwd) {
  const char *name = NULL;
  const char *instruction = NULL;
  const char *prompt = NULL;
  char buffer[256] = {0};
  int err = SSH_AUTH_ERROR;
  int rc;

  err = ssh_userauth_kbdint(session, user, NULL);
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
        rc = ssh_userauth_kbdint_setanswer(session, i, buffer);
        if (rc < 0) {
          return SSH_AUTH_ERROR;
        }
        ZERO_STRUCT(buffer);
      } else {
        if (passwd != NULL) {
          rc = ssh_userauth_kbdint_setanswer(session, i, passwd);
          if (rc < 0) {
            return SSH_AUTH_ERROR;
          }
        } else {
          (*_authcb) ("Password:", buffer, sizeof(buffer), 0, 0, NULL);
          rc = ssh_userauth_kbdint_setanswer(session, i, buffer);
          if (rc < 0) {
            return SSH_AUTH_ERROR;
          }
          ZERO_STRUCT(buffer);
        }
      }
    }
    err = ssh_userauth_kbdint(session, user, NULL);
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
    case SSH_FX_FAILURE:
      rc = ENOTEMPTY;
      break;
    default:
      rc = EIO;
      break;
  }

  return rc;
}

static int _sftp_connect(const char *uri) {
  char *scheme = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  unsigned int port = 0;
  char *path = NULL;
  unsigned char *hash = NULL;
  int hlen;
  int rc = -1;
  int state = SSH_SERVER_ERROR;
  int timeout = 10;
  int method;
  char *verbosity;

  if (_connected) {
    return 0;
  }

  rc = c_parse_uri(uri, &scheme, &user, &passwd, &host, &port, &path);
  if (rc < 0) {
    goto out;
  }

  DEBUG_SFTP(("csync_sftp - conntecting to: %s\n", host));

  /* create the session */
  _ssh_session = ssh_new();
  if (_ssh_session == NULL) {
    fprintf(stderr, "csync_sftp - error creating new connection: %s\n",
        strerror(errno));
    rc = -1;
    goto out;
  }

  rc = ssh_options_set(_ssh_session, SSH_OPTIONS_TIMEOUT, &timeout);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error setting options: %s\n",
        strerror(errno));
    goto out;
  }

  rc = ssh_options_set(_ssh_session, SSH_OPTIONS_COMPRESSION_C_S, "none");
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error setting options: %s\n",
        strerror(errno));
    goto out;
  }

  rc = ssh_options_set(_ssh_session, SSH_OPTIONS_COMPRESSION_S_C, "none");
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error setting options: %s\n",
        strerror(errno));
    goto out;
  }

  ssh_options_set(_ssh_session, SSH_OPTIONS_HOST, host);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error setting options: %s\n",
        strerror(errno));
    goto out;
  }

  if (port) {
    ssh_options_set(_ssh_session, SSH_OPTIONS_PORT, &port);
    if (rc < 0) {
      fprintf(stderr, "csync_sftp - error setting options: %s\n",
          strerror(errno));
      goto out;
    }
    DEBUG_SFTP(("csync_sftp - port set to: %d\n", port));
  }

  if (user && *user) {
    ssh_options_set(_ssh_session, SSH_OPTIONS_USER, user);
    if (rc < 0) {
      fprintf(stderr, "csync_sftp - error setting options: %s\n",
          strerror(errno));
      goto out;
    }
    DEBUG_SFTP(("csync_sftp - username set to: %s\n", user));
  }

  verbosity = getenv("CSYNC_SFTP_LOG_VERBOSITY");
  if (verbosity) {
    rc = ssh_options_set(_ssh_session, SSH_OPTIONS_LOG_VERBOSITY_STR, verbosity);
    if (rc < 0) {
      goto out;
    }
  }

  /* read ~/.ssh/config */
  rc = ssh_options_parse_config(_ssh_session, NULL);
  if (rc < 0) {
    goto out;
  }

  _ssh_callbacks = (ssh_callbacks) c_malloc(sizeof(struct ssh_callbacks_struct));
  if (_ssh_callbacks == NULL) {
    rc = -1;
    goto out;
  }
  ZERO_STRUCTP(_ssh_callbacks);

  _ssh_callbacks->userdata = _userdata;
  _ssh_callbacks->auth_function = _ssh_auth_callback;

  ssh_callbacks_init(_ssh_callbacks);

  ssh_set_callbacks(_ssh_session, _ssh_callbacks);

  rc = ssh_connect(_ssh_session);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error connecting to the server: %s\n", ssh_get_error(_ssh_session));
    ssh_disconnect(_ssh_session);
    _ssh_session = NULL;
    ssh_finalize();
    goto out;
  }

  hlen = ssh_get_pubkey_hash(_ssh_session, &hash);
  if (hlen < 0) {
    fprintf(stderr, "csync_sftp - error connecting to the server: %s\n",
        ssh_get_error(_ssh_session));
    ssh_disconnect(_ssh_session);
    _ssh_session = NULL;
    ssh_finalize();
    goto out;
  }

  /* check the server public key hash */
  state = ssh_is_server_known(_ssh_session);
  switch (state) {
    case SSH_SERVER_KNOWN_OK:
      break;
    case SSH_SERVER_KNOWN_CHANGED:
      fprintf(stderr, "csync_sftp - The host key for this server was "
            "not found, but another type of key exists.\n"
            "An attacker might change the default server key to confuse your "
            "client into thinking the key does not exist.\n"
            "Please contact your system administrator.\n"
            "%s\n", ssh_get_error(_ssh_session));
      ssh_print_hexa("csync_sftp - public key hash", hash, hlen);

      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
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
      fprintf(stderr, "The host key for the server %s has changed.\n"
          "This could either mean that DNS SPOOFING is happening or the IP "
          "address for the host and its host key have changed at the same time.\n"
          "The fingerprint for the key sent by the remote host is:\n", host);
          ssh_print_hexa("", hash, hlen);
          fprintf(stderr, "Please contact your system administrator.\n"
          "%s\n", ssh_get_error(_ssh_session));

      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_NOT_KNOWN:
      if (_authcb) {
        char *hexa;
        char *prompt;
        char buf[4] = {0};

        hexa = ssh_get_hexa(hash, hlen);
        if (hexa == NULL) {
          ssh_disconnect(_ssh_session);
          _ssh_session = NULL;
          ssh_finalize();
          rc = -1;
          goto out;
        }

        if (asprintf(&prompt,
              "The authenticity of host '%s' can't be established.\n"
              "RSA key fingerprint is %s.\n"
              "Are you sure you want to continue connecting (yes/no)?",
              host, hexa) < 0 ) {
          free(hexa);
          ssh_disconnect(_ssh_session);
          _ssh_session = NULL;
          ssh_finalize();
          rc = -1;
          goto out;
        }

        free(hexa);

        if ((*_authcb)(prompt, buf, sizeof(buf), 1, 0, _userdata) < 0) {
          free(prompt);
          ssh_disconnect(_ssh_session);
          _ssh_session = NULL;
          ssh_finalize();
          rc = -1;
          goto out;
        }

        free(prompt);

        if (strncasecmp(buf, "yes", 3) != 0) {
          ssh_disconnect(_ssh_session);
          _ssh_session = NULL;
          ssh_finalize();
          rc = -1;
          goto out;
        }

        if (ssh_write_knownhost(_ssh_session) < 0) {
          ssh_disconnect(_ssh_session);
          _ssh_session = NULL;
          ssh_finalize();
          rc = -1;
          goto out;
        }
      } else {
        fprintf(stderr,"csync_sftp - the server is unknown. Connect manually to "
            "the host to retrieve the public key hash, then try again.\n");
      }
      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_ERROR:
      fprintf(stderr, "%s\n", ssh_get_error(_ssh_session));

      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    default:
      break;
  }

  /* Try to authenticate */
  rc = ssh_userauth_none(_ssh_session, NULL);
  if (rc == SSH_AUTH_ERROR) {
      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
  }

#if 0
  /* authenticate with the server */
  if (passwd && *passwd) {
    DEBUG_SFTP(("csync_sftp - authenticating with user/password\n"));
    /*
     * This is tunneled cleartext password authentication and possibly needs
     * to be allowed by the ssh server. Set 'PasswordAuthentication yes'
     */
    auth = ssh_userauth_password(_ssh_session, user, passwd);
  } else {
    DEBUG_SFTP(("csync_sftp - authenticating with pubkey\n"));
    auth = ssh_userauth_autopubkey(_ssh_session, NULL);
  }

  if (auth == SSH_AUTH_ERROR) {
    fprintf(stderr, "csync_sftp - authenticating with pubkey: %s\n",
        ssh_get_error(_ssh_session));
    ssh_disconnect(_ssh_session);
    _ssh_session = NULL;
    ssh_finalize();
    rc = -1;
    goto out;
  }

  if (auth != SSH_AUTH_SUCCESS) {
    if (_authcb != NULL) {
      auth = auth_kbdint(_ssh_session);
      if (auth == SSH_AUTH_ERROR) {
        fprintf(stderr,"csync_sftp - authentication failed: %s\n",
            ssh_get_error(_ssh_session));
        ssh_disconnect(_ssh_session);
        _ssh_session = NULL;
        ssh_finalize();
        rc = -1;
        goto out;
      }
    } else {
      ssh_disconnect(_ssh_session);
      _ssh_session = NULL;
      ssh_finalize();
      rc = -1;
      goto out;
    }
  }


#endif
  method = ssh_auth_list(_ssh_session);

  while (rc != SSH_AUTH_SUCCESS) {
    /* Try to authenticate with public key first */
    if (method & SSH_AUTH_METHOD_PUBLICKEY) {
      rc = ssh_userauth_autopubkey(_ssh_session, NULL);
      if (rc == SSH_AUTH_ERROR) {
        ssh_disconnect(_ssh_session);
        _ssh_session = NULL;
        ssh_finalize();
        rc = -1;
        goto out;
      } else if (rc == SSH_AUTH_SUCCESS) {
        break;
      }
    }

    /* Try to authenticate with keyboard interactive */
    if (method & SSH_AUTH_METHOD_INTERACTIVE) {
      rc = auth_kbdint(_ssh_session, user, passwd);
      if (rc == SSH_AUTH_ERROR) {
        ssh_disconnect(_ssh_session);
        _ssh_session = NULL;
        ssh_finalize();
        rc = -1;
        goto out;
      } else if (rc == SSH_AUTH_SUCCESS) {
        break;
      }
    }

    /* Try to authenticate with password */
    if ((method & SSH_AUTH_METHOD_PASSWORD) && passwd && *passwd) {
      rc = ssh_userauth_password(_ssh_session, user, passwd);
      if (rc == SSH_AUTH_ERROR) {
        ssh_disconnect(_ssh_session);
        _ssh_session = NULL;
        ssh_finalize();
        rc = -1;
        goto out;
      } else if (rc == SSH_AUTH_SUCCESS) {
        break;
      }
    }
  }

  DEBUG_SFTP(("csync_sftp - creating sftp channel...\n"));
  /* start the sftp session */
  _sftp_session = sftp_new(_ssh_session);
  if (_sftp_session == NULL) {
    fprintf(stderr, "csync_sftp - sftp error initialising channel: %s\n", ssh_get_error(_ssh_session));
    rc = -1;
    goto out;
  }

  rc = sftp_init(_sftp_session);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - error initialising sftp: %s\n", ssh_get_error(_ssh_session));
    goto out;
  }

  DEBUG_SFTP(("csync_sftp - connection established...\n"));
  _connected = 1;
  rc = 0;
out:
  SAFE_FREE(scheme);
  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(path);
  SAFE_FREE(hash);

  return rc;
}

/*
 * file functions
 */

static csync_vio_method_handle_t *_sftp_open(const char *uri, int flags, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(_sftp_session, path, flags, mode);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static csync_vio_method_handle_t *_sftp_creat(const char *uri, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(_sftp_session, path, O_CREAT|O_WRONLY|O_TRUNC, mode);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static int _sftp_close(csync_vio_method_handle_t *fhandle) {
  int rc = -1;

  rc = sftp_close(fhandle);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  return rc;
}

static ssize_t _sftp_read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  int rc = -1;

  rc = sftp_read(fhandle, buf, count);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  return rc;
}

static ssize_t _sftp_write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  int rc = -1;

  rc = sftp_write(fhandle, (void *) buf, count);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  return rc;
}

static off_t _sftp_lseek(csync_vio_method_handle_t *fhandle, off_t offset, int whence) {
  /* FIXME: really implement seek for lseek? */
  (void) whence;
  sftp_seek(fhandle, offset);
  return 0;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *_sftp_opendir(const char *uri) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_opendir(_sftp_session, path);
  if (mh == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return mh;
}

static int _sftp_closedir(csync_vio_method_handle_t *dhandle) {
  int rc = -1;

  rc = sftp_closedir(dhandle);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  return rc;
}

static csync_vio_file_stat_t *_sftp_readdir(csync_vio_method_handle_t *dhandle) {
  sftp_attributes dirent = NULL;
  csync_vio_file_stat_t *fs = NULL;

  /* TODO: consider adding the _sftp_connect function */
  dirent = sftp_readdir(_sftp_session, dhandle);
  if (dirent == NULL) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
    return NULL;
  }

  fs = c_malloc(sizeof(csync_vio_file_stat_t));
  if (fs == NULL) {
    sftp_attributes_free(dirent);
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

  sftp_attributes_free(dirent);
  return fs;
}

static int _sftp_mkdir(const char *uri, mode_t mode) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_mkdir(_sftp_session, path, mode);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _sftp_rmdir(const char *uri) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_rmdir(_sftp_session, path);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _sftp_stat(const char *uri, csync_vio_file_stat_t *buf) {
  sftp_attributes attrs;
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  attrs = sftp_lstat(_sftp_session, path);
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
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }
  SAFE_FREE(path);
  sftp_attributes_free(attrs);

  return rc;
}

static int _sftp_rename(const char *olduri, const char *newuri) {
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
  sftp_unlink(_sftp_session, newpath);
  rc = sftp_rename(_sftp_session, oldpath, newpath);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

out:
  SAFE_FREE(oldpath);
  SAFE_FREE(newpath);

  return rc;
}

static int _sftp_unlink(const char *uri) {
  char *path = NULL;
  int rc = -1;

  if (_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = sftp_unlink(_sftp_session, path);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _sftp_chmod(const char *uri, mode_t mode) {
  struct sftp_attributes_struct attrs;
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

  rc = sftp_setstat(_sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _sftp_chown(const char *uri, uid_t owner, gid_t group) {
  struct sftp_attributes_struct attrs;
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

  rc = sftp_setstat(_sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static int _sftp_utimes(const char *uri, const struct timeval *times) {
  struct sftp_attributes_struct attrs;
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

  rc = sftp_setstat(_sftp_session, path, &attrs);
  if (rc < 0) {
    errno = _sftp_portable_to_errno(sftp_get_error(_sftp_session));
  }

  SAFE_FREE(path);
  return rc;
}

static struct csync_vio_capabilities_s _sftp_capabilities = {
    .atomar_copy_support = false
};

static struct csync_vio_capabilities_s *_sftp_get_capabilities(void)
{
    return &_sftp_capabilities;
}

csync_vio_method_t _method = {
  .method_table_size = sizeof(csync_vio_method_t),
  .get_capabilities = _sftp_get_capabilities,
  .open = _sftp_open,
  .creat = _sftp_creat,
  .close = _sftp_close,
  .read = _sftp_read,
  .write = _sftp_write,
  .lseek = _sftp_lseek,
  .opendir = _sftp_opendir,
  .closedir = _sftp_closedir,
  .readdir = _sftp_readdir,
  .mkdir = _sftp_mkdir,
  .rmdir = _sftp_rmdir,
  .stat = _sftp_stat,
  .rename = _sftp_rename,
  .unlink = _sftp_unlink,
  .chmod = _sftp_chmod,
  .chown = _sftp_chown,
  .utimes = _sftp_utimes
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

  if (_sftp_session) {
    sftp_free(_sftp_session);
  }
  if (_ssh_session) {
    ssh_disconnect(_ssh_session);
  }
  if (_ssh_callbacks) {
    free(_ssh_callbacks);
  }

  ssh_finalize();
}

/* vim: set ts=8 sw=2 et cindent: */
