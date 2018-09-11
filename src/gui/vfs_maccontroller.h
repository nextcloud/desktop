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

 
#ifndef LOOPBACKCONTROLLER_H
#define LOOPBACKCONTROLLER_H

#include <QtCore>

#include "quotainfo.h"
#include "accountstate.h"

class VfsMac;

class VfsMacController: public QObject
{
    Q_OBJECT
public:
    explicit VfsMacController(QString rootPath, QString mountPath, OCC::AccountState *accountState, QObject *parent);
    //~LoopbackController();
    
public slots:
    void slotquotaUpdated(qint64 total, qint64 used);
    void unmount();
    
private:
    QScopedPointer<VfsMac> fs_;
    OCC::QuotaInfo* qi_;
private slots:
    static void mountFailed (QVariantMap userInfo);
    static void didMount(QVariantMap userInfo);
    static void didUnmount (QVariantMap userInfo);
};


#endif
