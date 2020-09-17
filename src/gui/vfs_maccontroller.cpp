/*
 * Copyright (C) 2018 by AMCO
 * Copyright (C) 2018 by Jesús Deloya <jdeloya_ext@amco.mx>
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

#include <QMessageBox>
#include <QApplication>

#include "vfs_maccontroller.h"
#include "vfs_mac.h"
#include "theme.h"

#include <AvailabilityMacros.h>

void VfsMacController::mountFailed(QVariantMap userInfo)
{
    qDebug() << "Got mountFailed notification.";

    qDebug() << "kGMUserFileSystem Error code: " << userInfo.value("code") << ", userInfo=" << userInfo.value("localizedDescription");

    QMessageBox alert;
    alert.setText(userInfo.contains("localizedDescription") ? userInfo.value("localizedDescription").toString() : "Unknown error");
    alert.exec();

    QApplication::quit();
}

void VfsMacController::didMount(QVariantMap userInfo)
{
    qDebug() << "Got didMount notification.";

    QString mountPath = userInfo.value(kGMUserFileSystemMountPathKey).toString();
}

void VfsMacController::didUnmount(QVariantMap userInfo)
{
    Q_UNUSED(userInfo);
    qDebug() << "Got didUnmount notification.";
    if (fuse->closeExternally) {
        QApplication::quit();
    }
    fuse->closeExternally = true;
}

void VfsMacController::mount()
{
    if (fuse) {
        fuse->mountAtPath(mountPath, options);
    }
}

void VfsMacController::unmount()
{
    if (fuse) {
        fuse->closeExternally = false;
        fuse->unmount();
    }
}

void VfsMacController::cleanCacheFolder()
{
    QDir mirror_path(rootPath);

    sleep(1000);
    mirror_path.removeRecursively();
}

void VfsMacController::slotquotaUpdated(qint64 total, qint64 used)
{
    fuse->setTotalQuota(total);
    fuse->setUsedQuota(used);
}

void VfsMacController::initialize(OCC::AccountState *accountState)
{
    rootPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/.cachedFiles";
    mountPath = "/Volumes/" + OCC::Theme::instance()->appName() + "fs";

    fuse = new VfsMac(rootPath, false, accountState, this);

    cfgFile.setDefaultFileStreamMirrorPath(rootPath);
    cfgFile.setDefaultFileStreamSyncPath(mountPath);

    QFileInfo root(rootPath);
    if (root.exists() && !root.isDir()) {
        QFile().remove(rootPath);
    }
    if (!root.exists()) {
        QDir().mkdir(rootPath);
    }

    connect(fuse, &VfsMac::FuseFileSystemDidMount, this, &VfsMacController::didMount);
    connect(fuse, &VfsMac::FuseFileSystemMountFailed, this, &VfsMacController::mountFailed);
    connect(fuse, &VfsMac::FuseFileSystemDidUnmount, this, &VfsMacController::didUnmount);

    QStringList options;

    QFileInfo icons(QCoreApplication::applicationDirPath() + "/../Resources/Nextcloud.icns");
    const auto volArg = QString("volicon=%1").arg(icons.canonicalFilePath());

    options.append(volArg);

    // Do not use the 'native_xattr' mount-time option unless the underlying
    // file system supports native extended attributes. Typically, the user
    // would be mounting an HFS+ directory through VfsMac, so we do want
    // this option in that case.
    options.append("native_xattr");
    options.append("kill_on_unmount");
    options.append("local");

    options.append("volname=" + QApplication::applicationName() + "FS");
    fuse->mountAtPath(mountPath, options);
}

VfsMacController::VfsMacController(QObject *parent)
    : OCC::VirtualDriveInterface(parent)
{
}

VfsMacController::~VfsMacController()
{
    if (fuse) {
        disconnect(fuse, &VfsMac::FuseFileSystemDidMount, this, &VfsMacController::didMount);
        disconnect(fuse, &VfsMac::FuseFileSystemMountFailed, this, &VfsMacController::mountFailed);
        disconnect(fuse, &VfsMac::FuseFileSystemDidUnmount, this, &VfsMacController::didUnmount);
    }
}
