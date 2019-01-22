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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>

#include "c_lib.h"
#include "csync_private.h"
#include "csync_exclude.h"
#include "csync_util.h"
#include "csync_misc.h"
#include "std/c_private.h"

#include "csync_update.h"
#include "csync_reconcile.h"

#include "vio/csync_vio.h"

#include "csync_rename.h"
#include "common/c_jhash.h"
#include "common/syncjournalfilerecord.h"
#include <common/asserts.h>

Q_LOGGING_CATEGORY(lcCSync, "sync.csync.csync", QtInfoMsg)

csync_s::csync_s(const char *localUri, OCC::SyncJournalDb *statedb)
  : statedb(statedb)
{
  size_t len = 0;

  /* remove trailing slashes */
  len = strlen(localUri);
  while(len > 0 && localUri[len - 1] == '/') --len;

  local.uri = c_strndup(localUri, len);
}

int csync_update(CSYNC *ctx) {
  int rc = -1;

  if(ctx->fuseEnabled){
    qCInfo(lcCSync, "Running FUSE!");
  }

  if (ctx == NULL) {
    errno = EBADF;
    return -1;
  }
  ctx->status_code = CSYNC_STATUS_OK;

  ctx->status_code = CSYNC_STATUS_OK;

  csync_memstat_check();

  if (!ctx->exclude_traversal_fn) {
      qCInfo(lcCSync, "No exclude file loaded or defined!");
  }

  /* update detection for local replica */
  QElapsedTimer timer;

  /* we don't do local discovery if fuse is in use */
  if(!ctx->fuseEnabled){
      timer.start();
      ctx->current = LOCAL_REPLICA;

      qCInfo(lcCSync, "## Starting local discovery ##");

      rc = csync_ftw(ctx, ctx->local.uri, csync_walker, MAX_DEPTH);
      if (rc < 0) {
        if(ctx->status_code == CSYNC_STATUS_OK) {
            ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
        }
        return rc;
      }

      qCInfo(lcCSync) << "Update detection for local replica took" << timer.elapsed() / 1000.
                      << "seconds walking" << ctx->local.files.size() << "files";
      csync_memstat_check();
  }

  /* update detection for remote replica */
  timer.restart();

  std::unordered_map<ByteArrayRef, std::unique_ptr<csync_file_stat_t>, ByteArrayRefHash>::iterator it = ctx->local.files.begin();
  qDebug() << "LOCAL ######################################################";
  while (it != ctx->local.files.end()) {
    qDebug() << "localFile->file_id " << it->second->file_id;
    qDebug() << "localFile->path " << it->second->path;
    qDebug() << "localFile->instruction " << it->second->instruction;
    it++;
  }
  qDebug() << "######################################################";

  qCInfo(lcCSync, "## Starting remote discovery ##");
  ctx->current = REMOTE_REPLICA;
  rc = csync_ftw(ctx, "", csync_walker, MAX_DEPTH);
  if (rc < 0) {
      if(ctx->status_code == CSYNC_STATUS_OK) {
          ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
      }
      return rc;
  }

  std::unordered_map<ByteArrayRef, std::unique_ptr<csync_file_stat_t>, ByteArrayRefHash>::iterator it2 = ctx->remote.files.begin();
  qDebug() << "REMOTE ######################################################";
  while (it2 != ctx->remote.files.end()) {
      qDebug() << "remote->file_id " << it2->second->file_id;
      qDebug() << "remote->path " << it2->second->path;
      qDebug() << "remote->instruction " << it2->second->instruction;
      it2++;
  }
  qDebug() << "######################################################";


  qCInfo(lcCSync) << "Update detection for remote replica took" << timer.elapsed() / 1000.
                  << "seconds walking" << ctx->remote.files.size() << "files";
  csync_memstat_check();

  ctx->status |= CSYNC_STATUS_UPDATE;

  rc = 0;
  return rc;
}

int csync_reconcile(CSYNC *ctx) {
  Q_ASSERT(ctx);
  ctx->status_code = CSYNC_STATUS_OK;

  /* Reconciliation for local replica */
  QElapsedTimer timer;
  timer.start();

  ctx->current = LOCAL_REPLICA;

  csync_reconcile_updates(ctx);

  qCInfo(lcCSync) << "Reconciliation for local replica took " << timer.elapsed() / 1000.
                  << "seconds visiting " << ctx->local.files.size() << " files.";

  /* Reconciliation for remote replica */
  timer.restart();

  ctx->current = REMOTE_REPLICA;

  csync_reconcile_updates(ctx);

  qCInfo(lcCSync) << "Reconciliation for remote replica took " << timer.elapsed() / 1000.
                  << "seconds visiting " << ctx->remote.files.size() << " files.";

  ctx->status |= CSYNC_STATUS_RECONCILE;
  return 0;
}

/*
 * local visitor which calls the user visitor with repacked stat info.
 */
static int _csync_treewalk_visitor(csync_file_stat_t *cur, CSYNC * ctx, const csync_treewalk_visit_func &visitor) {
    csync_s::FileMap *other_tree = nullptr;

    /* we need the opposite tree! */
    switch (ctx->current) {
    case LOCAL_REPLICA:
        other_tree = &ctx->remote.files;
        break;
    case REMOTE_REPLICA:
        other_tree = &ctx->local.files;
        break;
    default:
        break;
    }

    csync_s::FileMap::const_iterator other_file_it = other_tree->find(cur->path);

    if (other_file_it == other_tree->cend()) {
        /* Check the renamed path as well. */
        QByteArray renamed_path = csync_rename_adjust_parent_path(ctx, cur->path);
        if (renamed_path != cur->path)
            other_file_it = other_tree->find(renamed_path);
    }

    if (other_file_it == other_tree->cend()) {
        /* Check the source path as well. */
        QByteArray renamed_path = csync_rename_adjust_parent_path_source(ctx, cur->path);
        if (renamed_path != cur->path)
            other_file_it = other_tree->find(renamed_path);
    }

    csync_file_stat_t *other = (other_file_it != other_tree->cend()) ? other_file_it->second.get() : NULL;

    ctx->status_code = CSYNC_STATUS_OK;

    Q_ASSERT(visitor);
    return visitor(cur, other);
}

/*
 * treewalk function, called from its wrappers below.
 */
static int _csync_walk_tree(CSYNC *ctx, csync_s::FileMap &tree, const csync_treewalk_visit_func &visitor)
{
    for (auto &pair : tree) {
        if (_csync_treewalk_visitor(pair.second.get(), ctx, visitor) < 0) {
            return -1;
        }
    }
    return 0;
}

/*
 * wrapper function for treewalk on the remote tree
 */
int csync_walk_remote_tree(CSYNC *ctx, const csync_treewalk_visit_func &visitor)
{
    ctx->status_code = CSYNC_STATUS_OK;
    ctx->current = REMOTE_REPLICA;
    return _csync_walk_tree(ctx, ctx->remote.files, visitor);
}

/*
 * wrapper function for treewalk on the local tree
 */
int csync_walk_local_tree(CSYNC *ctx, const csync_treewalk_visit_func &visitor)
{
    ctx->status_code = CSYNC_STATUS_OK;
    ctx->current = LOCAL_REPLICA;
    return _csync_walk_tree(ctx, ctx->local.files, visitor);
}

int csync_s::reinitialize() {
  int rc = 0;

  status_code = CSYNC_STATUS_OK;

  remote.read_from_db = 0;
  read_remote_from_db = true;

  //FUSE
  //local.files.clear();
  remote.files.clear();

  renames.folder_renamed_from.clear();
  renames.folder_renamed_to.clear();

  status = CSYNC_STATUS_INIT;
  SAFE_FREE(error_string);

  rc = 0;
  return rc;
}

csync_s::~csync_s() {
  SAFE_FREE(local.uri);
  SAFE_FREE(error_string);
}

void *csync_get_userdata(CSYNC *ctx) {
  if (ctx == NULL) {
    return NULL;
  }
  return ctx->callbacks.userdata;
}

int csync_set_userdata(CSYNC *ctx, void *userdata) {
  if (ctx == NULL) {
    return -1;
  }

  ctx->callbacks.userdata = userdata;

  return 0;
}

csync_auth_callback csync_get_auth_callback(CSYNC *ctx) {
  if (ctx == NULL) {
    return NULL;
  }

  return ctx->callbacks.auth_function;
}

int csync_set_status(CSYNC *ctx, int status) {
  if (ctx == NULL || status < 0) {
    return -1;
  }

  ctx->status = status;

  return 0;
}

CSYNC_STATUS csync_get_status(CSYNC *ctx) {
  if (ctx == NULL) {
    return CSYNC_STATUS_ERROR;
  }

  return ctx->status_code;
}

const char *csync_get_status_string(CSYNC *ctx)
{
  return csync_vio_get_status_string(ctx);
}

void csync_request_abort(CSYNC *ctx)
{
  if (ctx != NULL) {
    ctx->abort = true;
  }
}

void csync_resume(CSYNC *ctx)
{
  if (ctx != NULL) {
    ctx->abort = false;
  }
}

int  csync_abort_requested(CSYNC *ctx)
{
  if (ctx != NULL) {
    return ctx->abort;
  } else {
    return (1 == 0);
  }
}

bool cysnc_update_file(CSYNC *ctx, const char *absolutePath, const QByteArray &relativePath, const QByteArray &fileName, csync_instructions_e instruction)
{
    csync_file_stat_t *fs = ctx->local.files.findFile(relativePath);
    if(fs){
      fs->instruction = instruction;

	  std::unordered_map<ByteArrayRef, std::unique_ptr<csync_file_stat_t>, ByteArrayRefHash>::iterator it = ctx->local.files.begin();
      qDebug() << "FOUND FILE IN TREE ######################################################";
      while (it != ctx->local.files.end()) {
          qDebug() << "localFile->file_id " << it->second->file_id;
          qDebug() << "localFile->path " << it->second->path;
          qDebug() << "localFile->original_path " << it->second->original_path;
          qDebug() << "localFile->instruction " << it->second->instruction;
          it++;
      }
      qDebug() << "######################################################";

      return true;

    } else {
        // do the whole process for a single file
        std::unique_ptr<csync_file_stat_t> newfile;
        csync_file_stat_t *previous_fs = NULL;
        csync_vio_handle_t *dh = NULL;
        unsigned int depth = MAX_DEPTH;
        QByteArray filenameBuffer;
        QByteArray fullpath;
        int read_from_db = 0;

        ctx->current = LOCAL_REPLICA;
		dh = csync_vio_opendir(ctx, absolutePath);
        if(dh){
            newfile = csync_vio_readfile(dh, absolutePath, fileName);

            if (newfile) {
				newfile->instruction = instruction;

				// csync_walker job

				/* Conversion error */
				if (newfile->path.isEmpty() && !newfile->original_path.isEmpty()) {
					ctx->status_code = CSYNC_STATUS_INVALID_CHARACTERS;
					ctx->error_string = c_strdup(newfile->original_path);
					newfile->original_path.clear();
					if (dh != nullptr) {
					  csync_vio_closedir(ctx, dh);
					}
					return false;
				}

				// At this point dirent->path only contains the file name.
				filenameBuffer = newfile->path;
                if (filenameBuffer.isEmpty()) {
				  ctx->status_code = CSYNC_STATUS_READDIR_ERROR;
				  if (dh != nullptr) {
					csync_vio_closedir(ctx, dh);
				  }
				  return false;
				}

				if (absolutePath[0] == '\0') {
					fullpath = filenameBuffer;
				} else {
                    fullpath = QByteArray() % absolutePath % '/' % filenameBuffer;
				}

				/* if the filename starts with a . we consider it a hidden file
				 * For windows, the hidden state is also discovered within the vio
				 * local stat function.
				 */
                 if (filenameBuffer[0] == '.') {
					if (filenameBuffer != ".sys.admin#recall#") { /* recall file shall not be ignored (#4420) */
						newfile->is_hidden = true;
					}
				}

				// Now process to have a relative path to the sync root for the local replica, or to the data root on the remote.
				newfile->path = fullpath;
				if (ctx->current == LOCAL_REPLICA) {
					ASSERT(newfile->path.startsWith(ctx->local.uri)); // path is relative to uri
					// "len + 1" to include the slash in-between.
					newfile->path = newfile->path.mid(strlen(ctx->local.uri) + 1);
				}

				previous_fs = ctx->current_fs;
				bool recurse = newfile->type == ItemTypeDirectory;

				/* Call walker for the  file */
				int rc = csync_walker(ctx, std::move(newfile));
				/* this function may update ctx->current and ctx->read_from_db */

				if (rc < 0) {
				  if (CSYNC_STATUS_IS_OK(ctx->status_code)) {
					  ctx->status_code = csync_errno_to_status(errno, CSYNC_STATUS_UPDATE_ERROR);
					  return false;
				  }
				  if (dh != nullptr) {
					csync_vio_closedir(ctx, dh);
				  }
				  return false;
				}


				// if it is a folder look into the files
				// CSYNC *ctx, const char *uri, const QByteArray &key, csync_instructions_e instruction
				if (recurse && rc == 0 && instruction != CSYNC_INSTRUCTION_IGNORE) {
				  rc = cysnc_update_file(ctx, absolutePath, relativePath, fileName, instruction);
				  if (!rc) {
					ctx->current_fs = previous_fs;
					if (dh != nullptr) {
					  csync_vio_closedir(ctx, dh);
					}
					return false;
				  }

				  if (ctx->current_fs && !ctx->current_fs->child_modified
					  && ctx->current_fs->instruction == CSYNC_INSTRUCTION_EVAL) {
					  if (ctx->current == REMOTE_REPLICA) {
						  ctx->current_fs->instruction = CSYNC_INSTRUCTION_UPDATE_METADATA;
					  } else {
						  ctx->current_fs->instruction = CSYNC_INSTRUCTION_NONE;
					  }
				  }

				  if (ctx->current_fs && previous_fs && ctx->current_fs->has_ignored_files) {
					  /* If a directory has ignored files, put the flag on the parent directory as well */
					  previous_fs->has_ignored_files = ctx->current_fs->has_ignored_files;
				  }
				}

				if (ctx->current_fs && previous_fs && ctx->current_fs->child_modified) {
					/* If a directory has modified files, put the flag on the parent directory as well */
					previous_fs->child_modified = ctx->current_fs->child_modified;
				}

				ctx->current_fs = previous_fs;
				ctx->remote.read_from_db = read_from_db;

				csync_vio_closedir(ctx, dh);
				qCDebug(lcCSync, " <= Closing walk for %s with read_from_db %d", absolutePath, read_from_db);

				ctx->current = REMOTE_REPLICA;

				std::unordered_map<ByteArrayRef, std::unique_ptr<csync_file_stat_t>, ByteArrayRefHash>::iterator it = ctx->local.files.begin();
                qDebug() << "AFTER ADDING LOCAL ######################################################";
                while (it != ctx->local.files.end()) {
                    qDebug() << "localFile->file_id " << it->second->file_id;
                    qDebug() << "localFile->path " << it->second->path;
                    qDebug() << "localFile->original_path " << it->second->original_path;
                    qDebug() << "localFile->instruction " << it->second->instruction;
                    it++;
                }
                qDebug() << "######################################################";

				return true;
			}
        }

    }

    return false;
}

std::unique_ptr<csync_file_stat_t> csync_file_stat_s::fromSyncJournalFileRecord(const OCC::SyncJournalFileRecord &rec)
{
    std::unique_ptr<csync_file_stat_t> st(new csync_file_stat_t);
    st->path = rec._path;
    st->inode = rec._inode;
    st->modtime = rec._modtime;
    st->type = static_cast<ItemType>(rec._type);
    st->etag = rec._etag;
    st->file_id = rec._fileId;
    st->remotePerm = rec._remotePerm;
    st->size = rec._fileSize;
    st->has_ignored_files = rec._serverHasIgnoredFiles;
    st->checksumHeader = rec._checksumHeader;
    st->e2eMangledName = rec._e2eMangledName;
    return st;
}

