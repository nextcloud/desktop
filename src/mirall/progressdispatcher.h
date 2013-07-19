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

class ProgressDispatcher : public QObject
{
    Q_OBJECT
public:
    static ProgressDispatcher* instance();
    ~ProgressDispatcher();

public:
    void setFolderProgress( Progress::Kind, const QString&, const QString&, long, long );
    void setOverallProgress(const QString&, const QString&, int, int, qlonglong, qlonglong);

signals:
    void folderProgress( Progress::Kind, const QString&, const QString&, long, long );
    void overallProgress(const QString&, const QString&, int, int, qlonglong, qlonglong );

public slots:

private:
    ProgressDispatcher(QObject* parent = 0);

    static ProgressDispatcher* _instance;
};

}
#endif // PROGRESSDISPATCHER_H
