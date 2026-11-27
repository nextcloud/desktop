/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace OCC
{
class SimpleApiJob;

/** @brief Loads a chat attachment's metadata and preview using its account. */
class AssistantAttachment final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString previewSource READ previewSource NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool previewFailed READ previewFailed NOTIFY changed)
    Q_PROPERTY(QUrl openUrl READ openUrl NOTIFY changed)
    Q_PROPERTY(QUrl downloadUrl READ downloadUrl NOTIFY changed)

public:
    explicit AssistantAttachment(const AccountPtr &account, qint64 fileId, qint64 taskId, QObject *parent = nullptr);
    ~AssistantAttachment() override;

    [[nodiscard]] QString name() const
    {
        return _name;
    }
    [[nodiscard]] QString previewSource() const
    {
        return _previewSource;
    }
    [[nodiscard]] bool loading() const
    {
        return _loading;
    }
    [[nodiscard]] bool previewFailed() const
    {
        return _previewFailed;
    }
    /** @brief Returns the Files page for an attachment stored in the user's files. */
    [[nodiscard]] QUrl openUrl() const;
    /** @brief Returns the browser download URL for the attachment. */
    [[nodiscard]] QUrl downloadUrl() const;

    /** @brief Reloads metadata and a bounded preview, cancelling any previous request. */
    Q_INVOKABLE void load();
    /** @brief Cancels pending requests and clears preview data. */
    Q_INVOKABLE void cancel();

Q_SIGNALS:
    void changed();

private:
    void loadPreview();
    void finishPreview(const QByteArray &data);
    void stopRequest();
    [[nodiscard]] QString outputPath(const QString &action) const;

    AccountPtr _account;
    qint64 _fileId;
    qint64 _taskId;
    QPointer<SimpleApiJob> _job;
    QTimer _deadline;
    QString _name;
    QString _previewSource;
    bool _storedFile = false;
    bool _loading = false;
    bool _previewFailed = false;
};
}
