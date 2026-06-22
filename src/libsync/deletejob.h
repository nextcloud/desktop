/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"
#include "networkjobs.h"

namespace OCC {

/**
 * @brief The DeleteJob class
 * @ingroup libsync
 */
class DeleteJob : public SimpleFileJob
{
    Q_OBJECT
public:
    explicit DeleteJob(AccountPtr account, const QString &path, const QMap<QByteArray, QByteArray> &headers = {}, QObject *parent = nullptr);
    explicit DeleteJob(AccountPtr account, const QUrl &url, const QMap<QByteArray, QByteArray> &headers = {}, QObject *parent = nullptr);

    void start() override;

    [[nodiscard]] QByteArray folderToken() const;
    void setFolderToken(const QByteArray &folderToken);

    [[nodiscard]] bool skipTrashbin() const;
    void setSkipTrashbin(bool skipTrashbin);

private:
    QMap<QByteArray, QByteArray> _headers = {};
    QUrl _url; // Only used if the constructor taking a url is taken.
    QByteArray _folderToken;
    bool _skipTrashbin = false;
};
}
