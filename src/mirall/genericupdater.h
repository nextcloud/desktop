/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#ifndef UPDATEDETECTOR_H
#define UPDATEDETECTOR_H

#include <QObject>
#include <QUrl>
#include <QTemporaryFile>

#include "mirall/updateinfo.h"
#include "mirall/updater.h"

class QNetworkAccessManager;
class QNetworkReply;

namespace Mirall {

class GenericUpdater : public QObject, public Updater
{
    Q_OBJECT
public:
    enum DownloadState { Unknown = 0, UpToDate, DownloadingUpdate, DownloadedUpdate,
                         DownloadFailed, UpdateButNoDownloadAvailable };
    explicit GenericUpdater(const QUrl &url, QObject *parent = 0);

    UpdateState updateState() const;
    void performUpdate();

    void checkForUpdate();
    void backgroundCheckForUpdate();

    void showFallbackMessage();

    QString statusString() const;
    int state() const;
    void setState(int state);

signals:
    void stateChanged();

public slots:
    void slotStartInstaller();

private slots:
    void slotOpenUpdateUrl();
    void slotSetVersionSeen();
    void slotVersionInfoArrived();
    void slotWriteFile();
    void slotDownloadFinished();

private:
    bool updateSucceeded() const;
    QString clientVersion() const;
    QString getSystemInfo();
    void showDialog();

    QString _targetFile;
    QUrl _updateUrl;
    QNetworkAccessManager *_accessManager;
    UpdateInfo _updateInfo;
    QScopedPointer<QTemporaryFile> _file;
    int _state;
    bool _showFallbackMessage;
};

}

#endif // UPDATEDETECTOR_H
