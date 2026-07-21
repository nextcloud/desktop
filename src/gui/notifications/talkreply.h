/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>

namespace OCC {
class AccountState;

/** @brief Sends an inline reply to a Nextcloud Talk notification. */
class TalkReply : public QObject
{
    Q_OBJECT

public:
    /** @brief Create a Talk reply sender for an account. */
    explicit TalkReply(AccountState *accountState, QObject *parent = nullptr);

    /** @brief Send a message to a conversation, optionally as a reply to a message id. */
    void sendReplyMessage(const QString &conversationToken, const QString &message, const QString &replyTo = {});

signals:
    /** @brief Emitted with the server-returned message after a successful request. */
    void replyMessageSent(const QString &message);

private:
    AccountState *_accountState = nullptr;
};
}
