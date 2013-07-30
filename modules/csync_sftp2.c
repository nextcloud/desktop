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
#include <unistd.h>
#include <pwd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libssh2.h>
#include <libssh2_sftp.h>

#include "c_lib.h"
#include "vio/csync_vio_module.h"
#include "vio/csync_vio_file_stat.h"

#ifdef NDEBUG
#define DEBUG_SFTP(x)
#else
#define DEBUG_SFTP(x) printf x
#endif

int connected;

int sock = 0;
LIBSSH2_SESSION *ssh_session;
LIBSSH2_SFTP *sftp_session;

csync_auth_callback auth_cb;

static void _kbd_callback(const char *name, int name_len, 
             const char *instruction, int instruction_len, int num_prompts,
             const LIBSSH2_USERAUTH_KBDINT_PROMPT *prompts,
             LIBSSH2_USERAUTH_KBDINT_RESPONSE *responses,
             void **abstract)
{
  char buf[256] = {0};
  int i = 0;

  (void) name;
  (void) name_len;
  (void) instruction;
  (void) instruction_len;

  for (i = 0; i < num_prompts; i++) {
    if ((*auth_cb) (prompts[i].text, buf, 255, (int) prompts[i].echo, 0) == 0) {
      responses[i].text = c_strdup(buf);
      responses[i].length = strlen(buf);
    }
  }

  (void) prompts;
  (void) abstract;
}

static int _libssh2_sftp_connect(const char *uri) {
  in_addr_t hostaddr = 0;
  struct sockaddr_in sin;

  char *scheme = NULL;
  char *user = NULL;
  char *passwd = NULL;
  char *host = NULL;
  unsigned int port = 22;
  char *path = NULL;

  const char *fingerprint = NULL;
  const char *userauthlist = NULL;

  const char *keyfile1 = "~/.ssh/id_dsa.pub";
  const char *keyfile2 = "~/.ssh/id_dsa";

  int i = 0;
  int auth = 0;
  int rc = -1;

  if (connected) {
    return 0;
  }

  rc = c_parse_uri(uri, &scheme, &user, &passwd, &host, &port, &path);
  if (rc < 0) {
    goto out;
  }

  if (user == NULL || *user == '\0') {
    const char *usr = NULL;
    struct passwd *p_entry;

    usr = getenv("USER");
    if (usr != NULL) {
      user = c_strdup(usr);
    } else {
      p_entry = getpwuid(getuid());
      user = c_strdup(p_entry->pw_name);
    }

    if (user == NULL || *user == '\0') {
      rc = -1;
      goto shutdown;
    }
  }

  DEBUG_SFTP(("csync_sftp - username: %s\n", user));
  DEBUG_SFTP(("csync_sftp - conntecting to: %s\n", host));

  /*
   * The application code is responsible for creating the socket
   * and establishing the connection
   */

  /* FIXME: allow hostnames */
  hostaddr = inet_addr(host);
  sock = socket(AF_INET, SOCK_STREAM, 0);

  ZERO_STRUCT(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = hostaddr;

  rc = connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in));
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - failed to connect!\n");
    goto out;
  }

  /* Create a session instance */
  ssh_session = libssh2_session_init();
  if (ssh_session == NULL) {
    rc = -1;
    goto out;
  }

  /* Since we have set non-blocking, tell libssh2 we are blocking */
  libssh2_session_set_blocking(ssh_session, 1);

  /* ... start it up. This will trade welcome banners, exchange keys,
   * and setup crypto, compression, and MAC layers
   */
  rc = libssh2_session_startup(ssh_session, sock);
  if (rc < 0) {
    fprintf(stderr, "csync_sftp - failure establishing SSH session: %d\n", rc);
    goto shutdown;
  }

  /* Since we have not set non-blocking, tell libssh2 we are blocking */
  libssh2_session_set_blocking(ssh_session, 1);

  /* At this point we havn't yet authenticated.  The first thing to do
   * is check the hostkey's fingerprint against our known hosts your app
   * may have it hard coded, may go to a file, may present it to the
   * user, that's your call
   */
  fingerprint = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_MD5);

  /* FIXME: validate the fingerprint */
  DEBUG_SFTP(("csync_sftp - md5 fingerprint: "));
  for (i = 0; i < 16; i++) {
    DEBUG_SFTP(("%02X ", (unsigned char)fingerprint[i]));
  }
  DEBUG_SFTP(("\n"));

  fingerprint = libssh2_hostkey_hash(ssh_session, LIBSSH2_HOSTKEY_HASH_SHA1);
  DEBUG_SFTP(("csync_sftp - sha1 fingerprint: "));
  for (i = 0; i < 20; i++) {
    DEBUG_SFTP(("%02X ", (unsigned char)fingerprint[i]));
  }
  DEBUG_SFTP(("\n"));

  /* check what authentication methods are available */
  userauthlist = libssh2_userauth_list(ssh_session, user, strlen(user));

  DEBUG_SFTP(("csync_sftp - authentication methods: %s\n", userauthlist));
  if (strstr(userauthlist, "password") != NULL) {
    auth |= 1;
  }

  if (strstr(userauthlist, "keyboard-interactive") != NULL) {
    auth |= 2;
  }

  if (strstr(userauthlist, "publickey") != NULL) {
    auth |= 4;
  }

  rc = -1;
  if (passwd && *passwd) {
    if (auth & 1) {
      DEBUG_SFTP(("csync_sftp - authentication method: password\n"));
      rc = libssh2_userauth_password(ssh_session, user, passwd);
      if (rc) {
        fprintf(stderr, "csync_sftp - password authentication failed\n");
      }
    }

    if (auth & 4) {
      DEBUG_SFTP(("csync_sftp - authentication method: publickey\n"));
      rc = libssh2_userauth_publickey_fromfile(ssh_session, user, keyfile1,
          keyfile2, passwd);
      if (rc) {
        fprintf(stderr, "csync_sftp - publickey authentication failed\n");
      }
    }
  }

  if (rc < 0 && auth_cb && (auth & 2)) {
    DEBUG_SFTP(("csync_sftp - authentication method: keyboard-interactive\n"));
    rc = libssh2_userauth_keyboard_interactive(ssh_session, user, &_kbd_callback);
  }

  if (rc < 0) {
    fprintf(stderr, "csync_sftp - authentication failed\n");
    goto shutdown;
  }

  sftp_session = libssh2_sftp_init(ssh_session);
  if (sftp_session == NULL) {
    rc = -1;
    goto shutdown;
  }

  DEBUG_SFTP(("csync_sftp - connection established...\n"));

  connected = 1;
  rc = 0;

  goto out;

shutdown:
  if (sftp_session != NULL) {
    libssh2_sftp_shutdown(sftp_session);

    sftp_session = NULL;
  }

  if (ssh_session != NULL) {
    libssh2_session_disconnect(ssh_session, "csync_sftp - shutdown on error");
    libssh2_session_free(ssh_session);

    ssh_session = NULL;
  }

  sleep(1);
  close(sock);

  sock = 0;
  connected = 0;
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

static csync_vio_method_handle_t *_open(const char *uri, int access, mode_t mode) {
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;
  unsigned long sftp_errno;
  int flags = 0;

  if (_libssh2_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  if(access & O_RDONLY)
    flags |= LIBSSH2_FXF_READ;
  if(access & O_WRONLY)
    flags |= LIBSSH2_FXF_WRITE;
  if(access & O_RDWR)
    flags |= (LIBSSH2_FXF_WRITE | LIBSSH2_FXF_READ);
  if(access & O_CREAT)
    flags |= LIBSSH2_FXF_CREAT;
  if(access & O_TRUNC)
    flags |= LIBSSH2_FXF_TRUNC;
  if(access & O_EXCL)
    flags |= LIBSSH2_FXF_EXCL;

  libssh2_session_set_blocking(ssh_session, 1);

  mh = (csync_vio_method_handle_t *) libssh2_sftp_open(sftp_session, path, flags, mode);
  if (mh == NULL) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_FILE_ALREADY_EXISTS:
        errno = EEXIST;
        break;
      case LIBSSH2_FX_NO_SUCH_FILE:
        errno = ENOENT;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      case LIBSSH2_FX_OP_UNSUPPORTED:
        errno = EINVAL;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  SAFE_FREE(path);
  return mh;
}

static csync_vio_method_handle_t *_creat(const char *uri, mode_t mode) {
  unsigned long sftp_errno = 0;
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_libssh2_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  libssh2_session_set_blocking(ssh_session, 1);

  mh = (csync_vio_method_handle_t *) libssh2_sftp_open(sftp_session, path,
      LIBSSH2_FXF_CREAT|LIBSSH2_FXF_WRITE|LIBSSH2_FXF_TRUNC, mode);
  if (mh == NULL) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_FILE_ALREADY_EXISTS:
        errno = EEXIST;
        break;
      case LIBSSH2_FX_NO_SUCH_FILE:
        errno = ENOENT;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      case LIBSSH2_FX_OP_UNSUPPORTED:
        errno = EINVAL;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  SAFE_FREE(path);
  return mh;
}

static int _close(csync_vio_method_handle_t *fhandle) {
  unsigned long sftp_errno = 0;
  int rc = -1;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_close(fhandle);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EBADF;
        break;
      case LIBSSH2_FX_NO_SPACE_ON_FILESYSTEM:
        errno = ENOSPC;
        break;
      case LIBSSH2_FX_QUOTA_EXCEEDED:
        errno = EDQUOT;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  return rc;
}

static ssize_t _read(csync_vio_method_handle_t *fhandle, void *buf, size_t count) {
  unsigned long sftp_errno = 0;
  int rc = -1;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_read(fhandle, buf, count);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EBADF;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  return rc;
}

static ssize_t _write(csync_vio_method_handle_t *fhandle, const void *buf, size_t count) {
  unsigned long sftp_errno = 0;
  int rc = -1;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_write(fhandle, buf, count);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EBADF;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  return rc;
}

static int64_t _lseek(csync_vio_method_handle_t *fhandle, int64_t offset, int whence) {
  /* FIXME: really implement seek for lseek? */
  (void) whence;

  libssh2_session_set_blocking(ssh_session, 1);

  libssh2_sftp_seek(fhandle, offset);
  return 0;
}

/*
 * directory functions
 */

static csync_vio_method_handle_t *_opendir(const char *uri) {
  unsigned long sftp_errno = 0;
  csync_vio_method_handle_t *mh = NULL;
  char *path = NULL;

  if (_libssh2_sftp_connect(uri) < 0) {
    return NULL;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return NULL;
  }

  libssh2_session_set_blocking(ssh_session, 1);

  mh = (csync_vio_method_handle_t *) libssh2_sftp_opendir(sftp_session, path);
  if (mh == NULL) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_NO_SUCH_FILE:
        errno = ENOENT;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  SAFE_FREE(path);
  return mh;
}

static int _closedir(csync_vio_method_handle_t *dhandle) {
  unsigned long sftp_errno = 0;
  int rc = -1;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_closedir(dhandle);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EBADF;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  return rc;
}

static csync_vio_file_stat_t *_readdir(csync_vio_method_handle_t *dhandle) {
  int rc = 0;
  unsigned long sftp_errno = 0;
  char file[512];
  LIBSSH2_SFTP_ATTRIBUTES dirent;
  csync_vio_file_stat_t *fs = NULL;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_readdir(dhandle, file, 512, &dirent);
  if (rc <= 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_NO_SUCH_FILE:
        errno = ENOENT;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      case LIBSSH2_FX_OK:
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
    return NULL;
  }

  if (! (dirent.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)) {
    return NULL;
  }

  fs = c_malloc(sizeof(csync_vio_file_stat_t));
  if (fs == NULL) {
    return NULL;
  }

  fs->name = c_strdup(file);
  fs->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch (dirent.permissions & S_IFMT) {
    case S_IFSOCK:
    case S_IFBLK:
    case S_IFCHR:
    case S_IFIFO:
    case S_IFLNK:
#if 0
      fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      fs->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
#endif
      break;
    case S_IFREG:
      fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      fs->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case S_IFDIR:
      fs->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;
      fs->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    default:
      break;
  }

  return fs;
}

static int _mkdir(const char *uri, mode_t mode) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  unsigned long sftp_errno = 0;
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  rc = libssh2_sftp_mkdir(sftp_session, path, mode);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_FAILURE:
        /*
         * mkdir always returns a failure, even if the path already exists.
         * To be POSIX conform and to be able to map it to EEXIST a stat
         * call should be issued here
         */
        libssh2_session_set_blocking(ssh_session, 1);

        if (libssh2_sftp_lstat(sftp_session, path, &attrs) == 0) {
          errno = EEXIST;
        }
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  SAFE_FREE(path);
  return rc;
}

static int _rmdir(const char *uri) {
  unsigned long sftp_errno = 0;
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_rmdir(sftp_session, path);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_DIR_NOT_EMPTY:
        errno = ENOTEMPTY;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
      case LIBSSH2_FX_WRITE_PROTECT:
        errno = EACCES;
        break;
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EINVAL;
        break;
      case LIBSSH2_FX_LINK_LOOP:
        errno = ELOOP;
        break;
      case LIBSSH2_FX_NOT_A_DIRECTORY:
        errno = ENOTDIR;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
  }

  SAFE_FREE(path);
  return rc;
}

static int _stat(const char *uri, csync_vio_file_stat_t *buf) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  unsigned long sftp_errno = 0;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_lstat(sftp_session, path, &attrs);
  if (rc < 0) {
    sftp_errno = libssh2_sftp_last_error(sftp_session);
    switch (sftp_errno) {
      case LIBSSH2_FX_NO_SUCH_FILE:
      case LIBSSH2_FX_NO_SUCH_PATH:
        errno = ENOENT;
        break;
      case LIBSSH2_FX_PERMISSION_DENIED:
        errno = EACCES;
        break;
      case LIBSSH2_FX_INVALID_HANDLE:
        errno = EBADF;
        break;
      case LIBSSH2_FX_NOT_A_DIRECTORY:
        errno = ENOTDIR;
        break;
      case LIBSSH2_FX_LINK_LOOP:
        errno = ELOOP;
        break;
      default:
        DEBUG_SFTP(("%s:%d - handle sftp_error: %lu\n", __FILE__, __LINE__, sftp_errno));
        break;
    }
    goto out;
  }

  buf->name = c_basename(path);
  if (buf->name == NULL) {
    csync_vio_file_stat_destroy(buf);
    goto out;
  }
  buf->fields = CSYNC_VIO_FILE_STAT_FIELDS_NONE;

  switch (attrs.permissions & S_IFMT) {
    case S_IFSOCK:
      buf->type = CSYNC_VIO_FILE_TYPE_SOCKET;
    case S_IFBLK:
      buf->type = CSYNC_VIO_FILE_TYPE_BLOCK_DEVICE;
    case S_IFCHR:
      buf->type = CSYNC_VIO_FILE_TYPE_CHARACTER_DEVICE;
    case S_IFIFO:
      buf->type = CSYNC_VIO_FILE_TYPE_FIFO;
    case S_IFLNK:
      buf->type = CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK;
      break;
    case S_IFREG:
      buf->type = CSYNC_VIO_FILE_TYPE_REGULAR;
      break;
    case S_IFDIR:
      buf->type = CSYNC_VIO_FILE_TYPE_DIRECTORY;
      break;
    default:
      buf->type = CSYNC_VIO_FILE_TYPE_UNKNOWN;
      break;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_TYPE;

  buf->mode = attrs.permissions;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_PERMISSIONS;

  if (buf->type == CSYNC_VIO_FILE_TYPE_SYMBOLIC_LINK) {
    /* FIXME: handle symlink */
    buf->flags = CSYNC_VIO_FILE_FLAGS_SYMLINK;
  } else {
    buf->flags = CSYNC_VIO_FILE_FLAGS_NONE;
  }
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_FLAGS;

  buf->uid = attrs.uid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_UID;

  buf->uid = attrs.gid;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_GID;

  buf->size = attrs.filesize;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_SIZE;

  buf->atime = attrs.atime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_ATIME;

  buf->mtime = attrs.mtime;
  buf->fields |= CSYNC_VIO_FILE_STAT_FIELDS_MTIME;

  rc = 0;
out:
  SAFE_FREE(path);

  return rc;
}

static int _rename(const char *olduri, const char *newuri) {
  char *oldpath = NULL;
  char *newpath = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(olduri) < 0) {
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

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_rename(sftp_session, oldpath, newpath);

out:
  SAFE_FREE(oldpath);
  SAFE_FREE(newpath);

  return rc;
}

static int _unlink(const char *uri) {
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_unlink(sftp_session, path);

  SAFE_FREE(path);
  return rc;
}

static int _chmod(const char *uri, mode_t mode) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);
  attrs.permissions = mode;
  attrs.flags |= LIBSSH2_SFTP_ATTR_PERMISSIONS;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_setstat(sftp_session, path, &attrs);

  SAFE_FREE(path);
  return rc;
}

static int _chown(const char *uri, uid_t owner, gid_t group) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);
  attrs.uid = owner;
  attrs.gid = group;
  attrs.flags |= LIBSSH2_SFTP_ATTR_UIDGID;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_setstat(sftp_session, path, &attrs);

  SAFE_FREE(path);
  return rc;
}

static int _utimes(const char *uri, const struct timeval *times) {
  LIBSSH2_SFTP_ATTRIBUTES attrs;
  char *path = NULL;
  int rc = -1;

  if (_libssh2_sftp_connect(uri) < 0) {
    return -1;
  }

  if (c_parse_uri(uri, NULL, NULL, NULL, NULL, NULL, &path) < 0) {
    return -1;
  }

  ZERO_STRUCT(attrs);

  attrs.atime = times[0].tv_sec;
  attrs.mtime = times[1].tv_sec;

  attrs.flags |= LIBSSH2_SFTP_ATTR_ACMODTIME;

  libssh2_session_set_blocking(ssh_session, 1);

  rc = libssh2_sftp_setstat(sftp_session, path, &attrs);

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

csync_vio_method_t *vio_module_init(const char *method_name, const char *args, csync_auth_callback cb) {
  DEBUG_SFTP(("csync_sftp - method_name: %s\n", method_name));
  DEBUG_SFTP(("csync_sftp - args: %s\n", args));

  (void) method_name;
  (void) args;

  auth_cb = cb;

  return &_method;
}

void vio_module_shutdown(csync_vio_method_t *method) {
  (void) method;

  if (connected) {
    if (sftp_session != NULL) {
      libssh2_sftp_shutdown(sftp_session);
    }

    if (ssh_session != NULL) {
      libssh2_session_disconnect(ssh_session, "Normal Shutdown, Thank you for playing");
      libssh2_session_free(ssh_session);

      ssh_session = NULL;
    }

    sleep(1);
    close(sock);
  }
}

/* vim: set ts=8 sw=2 et cindent: */
