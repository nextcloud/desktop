// ================================================================
// Copyright (C) 2007 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ================================================================
//
//  LoopbackController.m
//  LoopbackFS
//
//  Created by ted on 12/27/07.
//
#include <QMessageBox>
#include <QApplication>

#include "LoopbackController.h"
#include "LoopbackFS.h"

#include <AvailabilityMacros.h>


void LoopbackController::mountFailed(QVariantMap userInfo)
{
    qDebug() << "Got mountFailed notification.";
    
    qDebug() << "kGMUserFileSystem Error code: " << userInfo.value("code") << ", userInfo=" << userInfo.value("localizedDescription");
    
    QMessageBox alert;
    alert.setText(userInfo.contains("localizedDescription")?userInfo.value("localizedDescription").toString() : "Unknown error");
}

void LoopbackController::didMount(QVariantMap userInfo)
{
    qDebug() << "Got didMount notification.";
    
    QString mountPath = userInfo.value(kGMUserFileSystemMountPathKey).toString();
    QMessageBox alert;
    alert.setText(tr(QString("Mounted at: %1").arg(mountPath).toLatin1().data()));
    alert.exec();
}

void LoopbackController::didUnmount(QVariantMap userInfo) {
    qDebug() << "Got didUnmount notification.";
    
    QApplication::quit();
}

void LoopbackController::slotquotaUpdated(qint64 total, qint64 used)
{
    fs_->setTotalQuota(total);
    fs_->setUsedQuota(used);
}

LoopbackController::LoopbackController(QString rootPath, QString mountPath, OCC::AccountState *accountState, QObject *parent):QObject(parent)
{
    fs_ = new LoopbackFS(rootPath, false, this);
    qi_ = new OCC::QuotaInfo(accountState, this);
    
    connect(qi_, &OCC::QuotaInfo::quotaUpdated, this, &LoopbackController::slotquotaUpdated);
    connect(fs_, &LoopbackFS::FuseFileSystemDidMount, this, didMount);
    connect(fs_, &LoopbackFS::FuseFileSystemMountFailed, this, mountFailed);
    connect(fs_, &LoopbackFS::FuseFileSystemDidUnmount, this, didUnmount);
    
    qi_->setActive(true);
    
    QStringList options;
    
    QFileInfo icons(QCoreApplication::applicationDirPath() + "/../Resources/LoopbackFS.icns");
    QString volArg = QString("volicon=%1").arg(icons.canonicalFilePath());
    
    options.append(volArg);
    
    // Do not use the 'native_xattr' mount-time option unless the underlying
    // file system supports native extended attributes. Typically, the user
    // would be mounting an HFS+ directory through LoopbackFS, so we do want
    // this option in that case.
    options.append("native_xattr");
    
    options.append("volname=LoopbackFS");
    fs_->mountAtPath(mountPath, options);
}

LoopbackController::~LoopbackController()
{
    fs_->unmount();
    fs_->deleteLater();
}
