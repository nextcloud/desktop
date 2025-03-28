// SPDX-FileCopyrightText: 2017 Julius Härtl <jus@bitgrid.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CLOUDPROVIDER_H
#define CLOUDPROVIDER_H

#include <QObject>
#include "folderman.h"

/* Forward declaration required since gio header files interfere with QObject headers */
struct _CloudProvidersProviderExporter;
using CloudProvidersProviderExporter = _CloudProvidersProviderExporter;
struct _CloudProvidersAccountExporter;
using CloudProvidersAccountExporter = _CloudProvidersAccountExporter;
struct _GMenuModel;
using GMenuModel = _GMenuModel;
struct _GMenu;
using GMenu = _GMenu;
struct _GActionGroup;
using GActionGroup = _GActionGroup;
using gchar = char;
using gpointer = void*;

using namespace OCC;

class CloudProviderWrapper : public QObject
{
    Q_OBJECT
public:
    explicit CloudProviderWrapper(QObject *parent = nullptr, Folder *folder = nullptr, int folderId = 0, CloudProvidersProviderExporter* cloudprovider = nullptr);
    ~CloudProviderWrapper() override;
    CloudProvidersAccountExporter* accountExporter();
    Folder* folder();
    GMenuModel* getMenuModel();
    GActionGroup* getActionGroup();
    void updateStatusText(QString statusText);
    void updatePauseStatus();

public slots:
    void slotSyncStarted();
    void slotSyncFinished(const OCC::SyncResult &);
    void slotUpdateProgress(const QString &folder, const OCC::ProgressInfo &progress);
    void slotSyncPausedChanged(OCC::Folder*, bool);

private:
    Folder *_folder;
    CloudProvidersProviderExporter *_cloudProvider;
    CloudProvidersAccountExporter *_cloudProviderAccount;
    QList<QPair<QString, QString>> _recentlyChanged;
    bool _paused;
    GMenu* _mainMenu = nullptr;
    GMenu* _recentMenu = nullptr;
};

#endif // CLOUDPROVIDER_H
