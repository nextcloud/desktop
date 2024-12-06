/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "editlocallymanager.h"

#include <QLoggingCategory>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

#include "accountmanager.h"
#include "systray.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcEditLocallyManager, "nextcloud.gui.editlocallymanager", QtInfoMsg)

EditLocallyManager *EditLocallyManager::_instance = nullptr;

EditLocallyManager::EditLocallyManager(QObject *parent)
    : QObject{parent}
{
}

EditLocallyManager *EditLocallyManager::instance()
{
    if (!_instance) {
        _instance = new EditLocallyManager();
    }
    return _instance;
}

void EditLocallyManager::showError(const QString &message, const QString &informativeText)
{
    showErrorNotification(message, informativeText);
    // to make sure the error is not missed, show a message box in addition
    showErrorMessageBox(message, informativeText);
    qCWarning(lcEditLocallyManager) << message << informativeText;
}

void EditLocallyManager::showErrorNotification(const QString &message, 
                                               const QString &informativeText)
{
    Systray::instance()->showMessage(message, informativeText, Systray::MessageIcon::Critical);
}

void EditLocallyManager::showErrorMessageBox(const QString &message, 
                                             const QString &informativeText)
{
    const auto messageBox = new QMessageBox;
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setText(message);
    messageBox->setInformativeText(informativeText);
    messageBox->setIcon(QMessageBox::Warning);
    messageBox->addButton(QMessageBox::StandardButton::Ok);
    messageBox->show();
    messageBox->activateWindow();
    messageBox->raise();
}

void EditLocallyManager::handleRequest(const QUrl &url)
{
    const auto inputs = parseEditLocallyUrl(url);
    const auto accountState = AccountManager::instance()->accountFromUserId(inputs.userId); 
    verify(accountState, inputs.relPath, inputs.token);
}

EditLocallyManager::EditLocallyInputData EditLocallyManager::parseEditLocallyUrl(const QUrl &url)
{
    const auto separator = QChar::fromLatin1('/');
    auto pathSplit = url.path().split(separator, Qt::SkipEmptyParts);

    if (pathSplit.size() < 2) {
        qCWarning(lcEditLocallyManager) << "Invalid URL for file local editing: " + pathSplit.join(separator);
        return {};
    }

    // for a sample URL "nc://open/admin@nextcloud.lan:8080/Photos/lovely.jpg", QUrl::path would return "admin@nextcloud.lan:8080/Photos/lovely.jpg"
    const auto userId = pathSplit.takeFirst();
    const auto fileRemotePath = pathSplit.join(separator);
    const auto urlQuery = QUrlQuery{url};

    auto token = QString{};
    if (urlQuery.hasQueryItem(QStringLiteral("token"))) {
        token = urlQuery.queryItemValue(QStringLiteral("token"));
    } else {
        qCWarning(lcEditLocallyManager) << "Invalid URL for file local editing: missing token";
    }

    return {userId, fileRemotePath, token};
}

void EditLocallyManager::verify(const AccountStatePtr &accountState, 
                                const QString &relPath, 
                                const QString &token)
{
    // Show the loading dialog but don't show the filename until we have
    // verified the token
    Systray::instance()->createEditFileLocallyLoadingDialog({});
    
    const auto finishedHandler = [this, token] {
        Systray::instance()->destroyEditFileLocallyLoadingDialog();
        _verificationJobs.remove(token); 
    };
    const auto errorEditLocally = [finishedHandler] {
        finishedHandler();
        showError(tr("Could not validate the request to open a file from server."), 
                  tr("Please try again."));
    };
    const auto startEditLocally = [this, accountState, relPath, token, finishedHandler] {
        finishedHandler();
        qCDebug(lcEditLocallyManager) << "Starting to edit file locally" << relPath 
                                      << "with token" << token;
#ifdef BUILD_FILE_PROVIDER_MODULE
        editLocallyFileProvider(accountState, relPath, token);
#else
        editLocally(accountState, relPath, token);    
#endif
    };
    const auto verificationJob = EditLocallyVerificationJobPtr(
        new EditLocallyVerificationJob(accountState, relPath, token)
    );
    _verificationJobs.insert(token, verificationJob);
    connect(verificationJob.data(), &EditLocallyVerificationJob::error, this, errorEditLocally);
    connect(verificationJob.data(), &EditLocallyVerificationJob::finished, this, startEditLocally);
    
    // We now ask the server to verify the token, before we again modify any
    // state or look at local files
    verificationJob->start();
}

#ifdef BUILD_FILE_PROVIDER_MODULE
void EditLocallyManager::editLocallyFileProvider(const AccountStatePtr &accountState,
                                                 const QString &relPath,
                                                 const QString &token)
{
    if (_editLocallyFpJobs.contains(token)) {
        return;
    }

    qCDebug(lcEditLocallyManager) << "Starting to edit file locally with file provider" << relPath 
                                  << "with token" << token;

    const auto removeJob = [&] { _editLocallyFpJobs.remove(token); };
    const auto tryStandardJob = [this, accountState, relPath, token, removeJob] {
        removeJob();
        editLocally(accountState, relPath, token);
    };
    const Mac::FileProviderEditLocallyJobPtr job(
        new Mac::FileProviderEditLocallyJob(accountState, relPath)
    );
    // We need to make sure the job sticks around until it is finished
    _editLocallyFpJobs.insert(token, job);

    connect(job.data(), &Mac::FileProviderEditLocallyJob::error, this, removeJob);
    connect(job.data(), &Mac::FileProviderEditLocallyJob::notAvailable, this, tryStandardJob);
    connect(job.data(), &Mac::FileProviderEditLocallyJob::finished, this, removeJob);

    job->start();
}
#endif

void EditLocallyManager::editLocally(const AccountStatePtr &accountState,
                                     const QString &relPath,
                                     const QString &token)
{
    if (_editLocallyJobs.contains(token)) {
        return;
    }

    qCDebug(lcEditLocallyManager) << "Starting to edit file locally" << relPath 
                                  << "with token" << token;

    const auto removeJob = [this, token] { _editLocallyJobs.remove(token); };
    const EditLocallyJobPtr job(new EditLocallyJob(accountState, relPath));
    // We need to make sure the job sticks around until it is finished
    _editLocallyJobs.insert(token, job);
 
    connect(job.data(), &EditLocallyJob::error, this, removeJob);
    connect(job.data(), &EditLocallyJob::finished, this, removeJob);

    job->startSetup();
}

}
