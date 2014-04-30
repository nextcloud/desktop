/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef PROGRESSDISPATCHER_H
#define PROGRESSDISPATCHER_H

#include "owncloudlib.h"
#include <QObject>
#include <QHash>
#include <QTime>
#include <QQueue>
#include <QElapsedTimer>
#include "syncfileitem.h"

namespace Mirall {

/**
 * @brief The FolderScheduler class schedules folders for sync
 */
namespace Progress
{
    /** Return true is the size need to be taken in account in the total amount of time */
    inline bool isSizeDependent(csync_instructions_e instruction) {
        return instruction == CSYNC_INSTRUCTION_CONFLICT || instruction == CSYNC_INSTRUCTION_SYNC
            || instruction == CSYNC_INSTRUCTION_NEW;
    }


    struct Info {
        Info() : _totalFileCount(0), _totalSize(0), _completedFileCount(0), _completedSize(0) {}

        quint64 _totalFileCount;
        quint64 _totalSize;
        quint64 _completedFileCount;
        quint64 _completedSize;

        struct ProgressItem {
            ProgressItem() : _completedSize(0) {}
            SyncFileItem _item;
            quint64 _completedSize;
        };
        QHash<QString, ProgressItem> _currentItems;
        SyncFileItem _lastCompletedItem;

        void setProgressComplete(const SyncFileItem &item) {
            _currentItems.remove(item._file);
            if (!item._isDirectory) {
                _completedFileCount++;
                if (Progress::isSizeDependent(item._instruction)) {
                    _completedSize += item._size;
                }
            }
            _lastCompletedItem = item;
        }
        void setProgressItem(const SyncFileItem &item, quint64 size) {
            _currentItems[item._file]._item = item;
            _currentItems[item._file]._completedSize = size;
            _lastCompletedItem = SyncFileItem();
        }

        quint64 completedSize() const {
            quint64 r = _completedSize;
            foreach(const ProgressItem &i, _currentItems) {
                if (!i._item._isDirectory)
                    r += i._completedSize;
            }
            return r;
        }
    };

    OWNCLOUDSYNC_EXPORT QString asActionString( const SyncFileItem& item );
    OWNCLOUDSYNC_EXPORT QString asResultString(  const SyncFileItem& item );

    OWNCLOUDSYNC_EXPORT bool isWarningKind( SyncFileItem::Status );

}

/**
 * @file progressdispatcher.h
 * @brief A singleton class to provide sync progress information to other gui classes.
 *
 * How to use the ProgressDispatcher:
 * Just connect to the two signals either to progress for every individual file
 * or the overall sync progress.
 *
 */
class OWNCLOUDSYNC_EXPORT ProgressDispatcher : public QObject
{
    Q_OBJECT

    friend class Folder; // only allow Folder class to access the setting slots.
public:
    static ProgressDispatcher* instance();
    ~ProgressDispatcher();

signals:
    /**
      @brief Signals the progress of data transmission.

      @param[out]  folder The folder which is being processed
      @param[out]  progress   A struct with all progress info.

     */
    void progressInfo( const QString& folder, const Progress::Info& progress );

protected:
    void setProgressInfo(const QString& folder, const Progress::Info& progress);

private:
    ProgressDispatcher(QObject* parent = 0);

    QElapsedTimer _timer;
    static ProgressDispatcher* _instance;
};

}
#endif // PROGRESSDISPATCHER_H
