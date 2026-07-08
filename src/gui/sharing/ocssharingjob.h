/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

#include "ocsjob.h"

#include "accountfwd.h"

namespace OCC::Gui::Sharing {

class Share;

/**
 * \brief The OcsSharingJob class handles requests to the new Unified Sharing API.
 */
class OcsSharingJob : public OcsJob
{
    Q_OBJECT

public:
    explicit OcsSharingJob(AccountPtr account, const QString &shareId = {});

    void createShare();
    void destroyShare();

    void addSource(const QString &fileId);
    void addRecipient(const QString &recipientType, const QString &recipientValue);
    void removeRecipient(const QString &recipientType, const QString &recipientValue);

    void searchRecipients(const QString &query, int64_t offset, int64_t limit);

    void setPermission(const QString &permissionClass, bool enabled);
    void setPermissionPreset(const QString &permissionPreset);

Q_SIGNALS:
    void shareCreated(QPointer<Share> share);

private:
    QString _shareId;
};

}
