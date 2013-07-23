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

#include <QObject>

namespace Mirall {


/**
 * @brief The FolderScheduler class schedules folders for sync
 */
class Progress
{
public:
    enum ProgressKind_s {
        Download,
        Upload,
        Context,
        Inactive,
        StartDownload,
        StartUpload,
        EndDownload,
        EndUpload
    };
    typedef ProgressKind_s Kind;

    static QString asString( Kind );
};

/**
 * @file progressdispatcher.h
 * @brief A singleton class to provide sync progress information to other gui classes.
 *
 * How to use the ProgressDispatcher:
 * Just connect to the two signals either to progress for every individual file
 * or the overall sync progress.
 *
 */
class ProgressDispatcher : public QObject
{
    Q_OBJECT

    friend class Folder; // only allow Folder class to access the setting slots.
public:
    static ProgressDispatcher* instance();
    ~ProgressDispatcher();

signals:
    /**
      @brief Signals the progress of a single file item.

      @param[out]  kind   The progress kind
      @param[out]  folder The folder which is being processed
      @param[out]  file   The current file.
      @param[out]  p1     The current progress in byte.
      @param[out]  p2     The maximum progress in byte.

     */
    void itemProgress( Progress::Kind kind, const QString& folder, const QString& file, qint64 p1, qint64 p2);

    /**
      @brief Signals the overall progress of a sync run.

      This signals the overall sync progress of a single sync run.
      If p1 == 0, the sync starts.
      If p1 == p2, the sync is finished.

      @param[out]  folder The folder which is being processed
      @param[out]  file   The current file.
      @param[out]  fileNo The current file number
      @param[out]  fileNo The overall file count to process.
      @param[out]  p1     The current progress in byte.
      @param[out]  p2     The maximum progress in byte.
     */
    void overallProgress(const QString& folder, const QString& file, int fileNo, int fileCnt, qint64 p1, qint64 p2);

protected:
    void setFolderProgress(Progress::Kind,  const QString&, const QString&, qint64, qint64);
    void setOverallProgress(const QString&, const QString&, int, int, qint64, qint64);

private:
    ProgressDispatcher(QObject* parent = 0);

    static ProgressDispatcher* _instance;
};

}
#endif // PROGRESSDISPATCHER_H
