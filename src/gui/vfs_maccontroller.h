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
//  VfsMacController.h
//
//
//  Created by ted on 12/27/07.
//


#ifndef VFSMACCONTROLLER_H
#define VFSMACCONTROLLER_H

#include <QtCore>

#include "accountstate.h"
#include "configfile.h"

class VfsMac;

class VfsMacController : public QObject
{
    Q_OBJECT
public:
    ~VfsMacController();
    static VfsMacController* instance();
    void mount();
    void unmount();
    void cleanCacheFolder();
    void initialize(const QString rootPath, const QString mountPath, OCC::AccountState *accountState);

public slots:
    void slotquotaUpdated(qint64 total, qint64 used);
    void mountFailed (QVariantMap userInfo);
    void didMount(QVariantMap userInfo);
    void didUnmount (QVariantMap userInfo);

private:
    VfsMac *fuse;
    static VfsMacController *_instance;
    QStringList options;
    QString rootPath;
    QString mountPath;
    explicit VfsMacController();
    OCC::ConfigFile cfgFile;
};


#endif
