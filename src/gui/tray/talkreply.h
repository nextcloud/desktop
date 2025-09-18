/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtCore>
#include <QPointer>

namespace OCC {
class AccountState;

class TalkReply : public QObject
{
    Q_OBJECT

public:
    explicit TalkReply(AccountState *accountState, QObject *parent = nullptr);

    void sendReplyMessage(const QString &conversationToken, const QString &message, const QString &replyTo = {});

signals:
    void replyMessageSent(const QString &message);

private:
    AccountState *_accountState = nullptr;
};
}
