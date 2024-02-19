/*
 * Copyright (C) 2024 by Uwe Runtemund <uwe.runtemund@jameo.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */


#ifndef filetags_h
#define filetags_h

#include "account.h"
#include "discoveryphase.h"
#include "networkjobs.h"
#include "owncloudlib.h"
#include "owncloudpropagator.h"
#include "syncfileitem.h"

#include <QByteArray>
#include <QString>

namespace OCC
{
/*!
 * THE FILE TAG SYNCING PROCESS
 *
 * Tags are special keywords for files or directories to give them an additional meaning. This
 * feature is provided by Nextcloud as well as by many operating systems. So it is a straight
 * forward assumption to expect, that also Nextcloud will sync tags with the local file system.
 *
 * Tags are stored on Nextcloud and can be fetched by a PROPFIND query. Locally the properties
 * are stored in the operating file system as well as in the syncronization database. Because the
 * tags can only be queried by a PROPFIND and our goal is not to heavily grow the number of slow
 * network queries, we go the following way for synchronizing the tags:
 *
 * In theory for synchronizing correctly, three locations are relevant:
 * 1. The local file system (LC)
 * 2. The local sync database (DB)
 * 3. The remote nextcloud server (RM)
 *
 * Changes of the tags are done on the RM or LC. Also we add the implementation on already existing
 * Nextcloud instances with tags existing propably local or remote, we need a gracefully strategy
 * to synchronizes all these tags without loosing any of them.
 *
 *The following matrix show all possible situations and solutions for syncing the tags:
 *
 * LC: Local, DB: SyncDatabase, RM: Remote
 * X: Tag is present, -: Tag is not present
 *-----------------------------------------
 *  LC DB RM  What happend   ACTION
 *-----------------------------------------
 * 1 -  -  -  no tags        NOP
 * 2 X  X  X  tag present    NOP
 * 3 X  -  X  lc/rm added    ADD  (DB)
 * 4 X  -  -  lc added       ADD  (RM+DB)
 * 5 -  -  X  rm added       ADD  (LC+DB)
 * 6 -  X  -  lc/rm deleted  DEL  (DB)
 * 7 -  X  X  lc deleted     DEL  (RM+DB)
 * 8 X  X  -  rm deleted     DEL  (LC+DB)
 *------------------------------------------
 *
 * IF WE HAVE ALL THREE POINTS, WE SYNC THAT WAY!
 *
 * In fact, most of the time, we have not all three data points available. Most of the time we have
 * LC/DB or RM/DB, so we have to deal a little bit different, to reach the same approach.
 * To avoid an massive increasment of the network polls, we use following approach:
 *
 * - DB is frequently overridden by the client with the RM value, so we cannot say for sure the
 *   correct sync status
 *  LC DB  What happened    Action
 * 1 -  -  no tags          NOP
 * 2 X  X  tag present      NOP
 * 3 -  X  rm add or lc del FULL SYNC
 * 4 X  -  lc add or rm del FULL SYNC
 *
 */
class OCSYNC_EXPORT FileTagManager : public QObject
{
    Q_OBJECT
 public:

    /*!
     * \brief Converts the given XML fragment of system-tag entries to a sorted list of tags.
     *
     * The list is a QByteArray with the content. The tags are delimited by 0x0A ('\\n')
     */
    static void fromPropertiesToTagList(QByteArray &list,const QString &properties);

    /*!
     * \brief Reads the tags from the local file system.
     */
    static QByteArray readTagListFromLocalFile(const QString &localpath);

    /*! Restore the tags in the local file system.
     * \note: is needed after file download.
     */
    static bool restoreLocalFileTags(SyncJournalDb* journal,
                                 const QString &localdir,
                                 const QString &file);

    /*!
     * \brief Makes a "full" synchronization. If all data is valid, full synchronization is directly
     * done. If not, local sync is checked, if data is not synchronized, a full sync is started.
     */
    static void syncTags(AccountPtr account,
                         SyncJournalDb* journal,
                         const QString &localDir,
                         const QString &filePath,
                         const LocalInfo &localEntry,
                         const RemoteInfo &serverEntry,
                         const SyncJournalFileRecord &dbEntry);

private:

    //! Private constructor. No need for an instance actually.
    FileTagManager();

    //! Needed for QT connect
    static FileTagManager* GetInstance();

    //! Helper method for syncTags
    void syncTagsStep1(AccountPtr account,
                       SyncJournalDb* journal,
                       const QString &fullpath,
                       QStringList list,
                       const SyncJournalFileRecord &dbEntry,
                       bool syncLocal,bool syncDb);

    //! Helper method for synTags
    void syncTagsStep2(SyncJournalDb* journal,
                       const QString &fullpath,
                       const QByteArray &newTagList,
                       const SyncJournalFileRecord &dbEntry,
                       bool syncLocal,bool syncDb);

    /*!
     * \brief Makes a "local" synchronization. Will do a server sync, if initial sync is assumed.
     */
    static void localSync(AccountPtr account,
                          SyncJournalDb* journal,
                          const QString &localDir,
                          const QString &filePath,
                          const LocalInfo &localEntry,
                          const SyncJournalFileRecord &dbEntry);

    //! Write the tag list to the local file.
    static bool writeTagListToLocalFile(const QString &localpath,const QByteArray &taglist);

 };

} // namespace OCC

#endif /* filetags_h */
