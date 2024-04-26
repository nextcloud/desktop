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

#include <QUrl>
#include <QUrlQuery>
#include <QLoggingCategory>

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

void EditLocallyManager::editLocally(const QUrl &url)
{
    const auto inputs = parseEditLocallyUrl(url);
    createJob(inputs.userId, inputs.relPath, inputs.token);
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

void EditLocallyManager::createJob(const QString &userId,
                                       const QString &relPath,
                                       const QString &token)
{
    if (_jobs.contains(token)) {
        return;
    }
    const EditLocallyJobPtr job(new EditLocallyJob(userId, relPath, token));
    // We need to make sure the job sticks around until it is finished
    _jobs.insert(token, job);

    const auto removeJob = [this, token] { _jobs.remove(token); };

    connect(job.data(), &EditLocallyJob::error, this, removeJob);
    connect(job.data(), &EditLocallyJob::finished, this, removeJob);

    job->startSetup();
}

}
