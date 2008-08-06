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

/* parse_uri */
static const char *uri_prefix = "sftp:";
static int parse_uri(const char *uri, char **user, char **passwd,
    char **hostname, char **port, char **path) {
  const char *p = NULL;
  const char *r = NULL;
  const char *q = NULL;
  char *s = NULL;
  size_t len = 0;

  /* Ensure these returns are at least valid pointers. */
  *hostname = c_strdup("");
  *port = c_strdup("");
  *path = c_strdup("");
  *user = c_strdup("");
  *passwd = c_strdup("");

  if (*hostname == NULL || *port == NULL || *path == NULL ||
      *user == NULL || *passwd == NULL) {
    return -1;
  }

  s = c_strdup(uri);
  if (s == NULL) {
    return -1;
  }

  len = strlen(uri_prefix);
  /* check if we have the right prefix */
  if (strncmp(s, uri_prefix, len) || (s[len] != '/' && s[len] != '\0')) {
    goto err;
  }

  p = s + len;

  if (strncmp(p, "//", 2) == 2) {
    DEBUG_SFTP(("Invalid path (does not begin with sftp://\n"));
    goto err;
  }

  /* Skip the double slash */
  p += 2;

  if (*p == '\0') {
    goto decoding;
  }

  /* check that '@' occurs before '/', if '/' exists at all */
  q = strchr(p, '@');
  r = strchr(p, '/');
  if (q && (!r || q < r)) {
    char *userinfo = NULL;
    const char *u;
    userinfo = c_strndup(p, q - p);
    if (userinfo == NULL) {
      goto err;
    }
    u = strchr(userinfo, ':');
    if (u) {
      SAFE_FREE(*user);
      *user = c_strndup(userinfo, u - userinfo);
      if (*user == NULL) {
        SAFE_FREE(userinfo);
        goto err;
      }
      SAFE_FREE(*passwd);
      *passwd = c_strdup(u + 1);
      if (*passwd == NULL) {
        SAFE_FREE(userinfo);
        goto err;
      }
    } else {
      SAFE_FREE(*user);
      *user = c_strdup(userinfo);
      if (*user == NULL) {
        SAFE_FREE(userinfo);
        goto err;
      }
    }

    len = strlen(userinfo) + 1;
    SAFE_FREE(userinfo);
    p += len;
  }
  DEBUG_SFTP(("user: %s\n", *user));
  DEBUG_SFTP(("passwd: %s\n", *passwd));

  if (*p == '\0') {
    goto decoding;
  }

  /* check if we have a path */
  r = strchr(p, '/');
  if (r) {
    char *h = NULL;
    h = c_strndup(p, r - p);
    if (h == NULL) {
      goto err;
    }
    /* look for port */
    q = strchr(h, ':');
    if (q) {
      SAFE_FREE(*port);
      *port = c_strdup(q + 1);
      if (*port == NULL) {
        SAFE_FREE(h);
        goto err;
      }
      SAFE_FREE(*hostname);
      *hostname = c_strndup(h, q - h);
      if (*hostname == NULL) {
        SAFE_FREE(h);
        goto err;
      }
    } else {
      SAFE_FREE(*hostname);
      *hostname = c_strdup(h);
      if (*hostname == NULL) {
        SAFE_FREE(h);
        goto err;
      }
    }
    len = strlen(h) + 1;
    SAFE_FREE(h);
  } else {
    SAFE_FREE(*hostname);
    *hostname = c_strdup(p);
    if (*hostname == NULL) {
      goto err;
    }
    len = strlen(p);
  }
  DEBUG_SFTP(("hostname: %s\n", *hostname));
  DEBUG_SFTP(("port: %s\n", *port));

  p += len;
  if (*p == '\0') {
    goto decoding;
  }

  SAFE_FREE(*path);
  *path = c_strdup(p);
  if (*path == NULL) {
    goto err;
  }

  if (**path) {
    char *d = NULL;
    if (asprintf(&d, "/%s", *path) < 0) {
      return -1;
    }
    SAFE_FREE(*path);
    *path = d;
    DEBUG_SFTP(("path: %s\n", *path));
  }

  goto decoding;
err:
  SAFE_FREE(s);
  return -1;
decoding:
  return 0;
}

static int _sftp_connect(const char *uri) {
  SSH_OPTIONS *options = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  char *port = NULL;
  char *path = NULL;
  unsigned char hash[MD5_DIGEST_LEN];
  int rc = -1;
  int auth = SSH_AUTH_ERROR;
  int state = SSH_SERVER_ERROR;

  if (connected) {
    return 0;
  }

  rc = parse_uri(uri, &user, &passwd, &host, &port, &path);
  if (rc < 0) {
    goto out;
  }

  options = ssh_options_new();
  if (options == NULL) {
    rc = -1;
    goto out;
  }

  /* set the option to connect to the server */
  ssh_options_set_host(options, host);
  if (*port) {
    ssh_options_set_port(options, atoi(port));
  }

  if (*user) {
    ssh_options_set_username(options, user);
  }

  /* connect to the server */
  ssh_session = ssh_new();
  ssh_set_options(ssh_session, options);
  rc = ssh_connect(ssh_session);
  if (rc < 0) {
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
      fprintf(stderr, "Host key for server changed : server's one is now :\n");
      ssh_get_pubkey_hash(ssh_session, hash);
      ssh_print_hexa("Public key hash", hash, MD5_DIGEST_LEN);
      fprintf(stderr,"For security reason, connection will be stopped\n");
      ssh_disconnect(ssh_session);
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_FOUND_OTHER:
      fprintf(stderr, "The host key for this server was not found but an other "
          "type of key exists.\n");
      fprintf(stderr, "An attacker might change the default server key to "
          "confuse your client into thinking the key does not exist\n");
      ssh_disconnect(ssh_session);
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_NOT_KNOWN:
      fprintf(stderr,"The server is unknown. Connect manually to the host "
          "first to retrieve the public key hash\n");
      ssh_disconnect(ssh_session);
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    case SSH_SERVER_ERROR:
      fprintf(stderr, "%s", ssh_get_error(ssh_session));
      ssh_disconnect(ssh_session);
      ssh_finalize();
      rc = -1;
      goto out;
      break;
    default:
      break;
  }

  /* authenticate with the server */
  if (*passwd) {
    auth = ssh_userauth_password(ssh_session, user, passwd);
    /*
    char username[256] = {0};
    char password[256] = {0};
    */
  } else {
    auth = ssh_userauth_autopubkey(ssh_session);
    if (auth == SSH_AUTH_ERROR) {
      fprintf(stderr, "Authenticating with pubkey: %s\n", ssh_get_error(ssh_session));
      ssh_finalize();
      rc = -1;
      goto out;
    }
  }

  /* FIXME: call callback */

  /* start the sftp session */
  sftp_session = sftp_new(ssh_session);
  if (sftp_session == NULL) {
    rc = -1;
    goto out;
  }

  rc = sftp_init(sftp_session);
  if (rc < 0) {
    return rc;
  }

  connected = 1;
  rc = 0;
out:
  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(port);
  SAFE_FREE(path);

  return rc;
}

csync_vio_method_handle_t *mh = NULL;
csync_vio_file_stat_t fs;

/*
 * file functions
 */

static csync_vio_method_handle_t *_open(const char *durl, int flags, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  char *port = NULL;
  char *path = NULL;

  /* FIXME: implement mode with SFTP_ATTRIBUTES */
  (void) mode;

  if (_sftp_connect(durl) < 0) {
    return NULL;
  }

  if (parse_uri(durl, &user, &passwd, &host, &port, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(sftp_session, path, flags, NULL);

  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(port);
  SAFE_FREE(path);

  return mh;
}

static csync_vio_method_handle_t *_creat(const char *durl, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  char *port = NULL;
  char *path = NULL;

  (void) mode;

  if (_sftp_connect(durl) < 0) {
    return NULL;
  }

  if (parse_uri(durl, &user, &passwd, &host, &port, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_open(sftp_session, path, O_CREAT|O_WRONLY|O_TRUNC, NULL);

  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(port);
  SAFE_FREE(path);

  return mh;
}

static int _close(csync_vio_method_handle_t *fhandle) {
  return sftp_file_close(fhandle);
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  return sftp_read(fhandle, buf, count);
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  return sftp_write(fhandle, buf, count);
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

static csync_vio_method_handle_t *_opendir(const char *durl) {
  csync_vio_method_handle_t *mh = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  char *port = NULL;
  char *path = NULL;

  if (_sftp_connect(durl) < 0) {
    return NULL;
  }

  if (parse_uri(durl, &user, &passwd, &host, &port, &path) < 0) {
    return NULL;
  }

  mh = (csync_vio_method_handle_t *) sftp_opendir(sftp_session, path);

  SAFE_FREE(user);
  SAFE_FREE(passwd);
  SAFE_FREE(host);
  SAFE_FREE(port);
  SAFE_FREE(path);

  return mh;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
  return sftp_dir_close(dhandle);
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  SFTP_ATTRIBUTES *dirent = NULL;
  csync_vio_file_stat_t *fs = NULL;

  /* TODO: consider adding the _sftp_connect function */
  dirent = sftp_readdir(sftp_session, dhandle);
  if (dirent == NULL) {
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
  (void) uri;
  (void) mode;

  return 0;
}

static int _rmdir(const char *uri) {
  (void) uri;

  return 0;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
  time_t now;

  buf->name = c_basename(uri);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  time(&now);
  buf->mtime = now;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  return 0;
}

static int _rename(const char *olduri, const char *newuri) {
  (void) olduri;
  (void) newuri;

  return 0;
}

static int _unlink(const char *uri) {
  (void) uri;

  return 0;
}

static int _chmod(const char *uri, mode_t mode) {
  (void) uri;
  (void) mode;

  return 0;
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
  (void) uri;
  (void) owner;
  (void) group;

  return 0;
}

static int _utimes(const char *uri, const struct timeval *times) {
  (void) uri;
  (void) times;

  return 0;
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

csync_vio_method_t *vio_module_init(const char *method_name, const char *args, csync_auth_callback cb) {
  DEBUG_SFTP(("csync_sftp - method_name: %s\n", method_name));
  DEBUG_SFTP(("csync_sftp - args: %s\n", args));

  (void) method_name;
  (void) args;

  return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;
}

