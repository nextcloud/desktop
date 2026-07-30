/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QHash>

#include "config.h"
#include "editlocallyjob.h"
#include "editlocallyverificationjob.h"

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovidereditlocallyjob.h"
#endif

namespace OCC {

class EditLocallyManager : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] static EditLocallyManager *instance();
    static void showError(const QString &message, const QString &informativeText);

public slots:
    void handleRequest(const QUrl &url);

private slots:
    void verify(const OCC::AccountStatePtr &accountState,
                const QString &relPath,
                const QString &token);
    void editLocally(const OCC::AccountStatePtr &accountState,
                     const QString &relPath,
                     const QString &token);
    
#ifdef BUILD_FILE_PROVIDER_MODULE
    void editLocallyFileProvider(const OCC::AccountStatePtr &accountState,
                                 const QString &relPath,
                                 const QString &token);
#endif


private:
    explicit EditLocallyManager(QObject *parent = nullptr);
    static EditLocallyManager *_instance;

    struct EditLocallyInputData {
        QString userId;
        QString relPath;
        QString token;
    };

    static void showErrorNotification(const QString &message, const QString &informativeText);
    static void showErrorMessageBox(const QString &message, const QString &informativeText);
    [[nodiscard]] static EditLocallyInputData parseEditLocallyUrl(const QUrl &url);

    QHash<QString, EditLocallyVerificationJobPtr> _verificationJobs;
    QHash<QString, EditLocallyJobPtr> _editLocallyJobs;

#ifdef BUILD_FILE_PROVIDER_MODULE
    QHash<QString, Mac::FileProviderEditLocallyJobPtr> _editLocallyFpJobs;
#endif
};

}
