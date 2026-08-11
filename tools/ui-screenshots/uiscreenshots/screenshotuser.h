/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTUSER_H
#define SCREENSHOTUSER_H

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantList>

namespace OCC {

/** @brief Provides fictional account and Assistant data to production screenshot QML. */
class ScreenshotUser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString server READ server CONSTANT)
    Q_PROPERTY(QString avatar READ avatar CONSTANT)
    Q_PROPERTY(QColor accentColor READ accentColor CONSTANT)
    Q_PROPERTY(QColor headerColor READ headerColor CONSTANT)
    Q_PROPERTY(QColor headerTextColor READ headerTextColor CONSTANT)
    Q_PROPERTY(bool hasLocalFolder READ hasLocalFolder CONSTANT)
    Q_PROPERTY(bool hasFileProvider READ hasFileProvider CONSTANT)
    Q_PROPERTY(bool isConnected READ isConnected CONSTANT)
    Q_PROPERTY(bool needsToSignTermsOfService READ needsToSignTermsOfService CONSTANT)
    Q_PROPERTY(bool isAssistantEnabled READ isAssistantEnabled NOTIFY assistantStateChanged)
    Q_PROPERTY(QString assistantResponse READ assistantResponse NOTIFY assistantResponseChanged)
    Q_PROPERTY(QString assistantError READ assistantError NOTIFY assistantErrorChanged)
    Q_PROPERTY(QVariantList assistantMessages READ assistantMessages NOTIFY assistantMessagesChanged)
    Q_PROPERTY(bool assistantRequestInProgress READ assistantRequestInProgress NOTIFY assistantRequestInProgressChanged)

public:
    /** @brief Creates the deterministic fictional user. */
    explicit ScreenshotUser(QObject *parent = nullptr);

    /** @brief Returns the fictional display name. */
    [[nodiscard]] QString name() const;
    /** @brief Returns the fictional server hostname. */
    [[nodiscard]] QString server() const;
    /** @brief Returns a production resource URL used as the avatar. */
    [[nodiscard]] QString avatar() const;
    /** @brief Returns the account accent color. */
    [[nodiscard]] QColor accentColor() const;
    /** @brief Returns the account-header background color. */
    [[nodiscard]] QColor headerColor() const;
    /** @brief Returns the account-header foreground color. */
    [[nodiscard]] QColor headerTextColor() const;
    /** @brief Returns whether the fixture exposes a classic local folder. */
    [[nodiscard]] bool hasLocalFolder() const;
    /** @brief Returns whether the fixture exposes a macOS File Provider domain. */
    [[nodiscard]] bool hasFileProvider() const;
    /** @brief Returns whether the fictional account is connected. */
    [[nodiscard]] bool isConnected() const;
    /** @brief Returns whether terms of service need browser confirmation. */
    [[nodiscard]] bool needsToSignTermsOfService() const;
    /** @brief Returns whether the Assistant surface is enabled. */
    [[nodiscard]] bool isAssistantEnabled() const;
    /** @brief Returns any separate Assistant response text. */
    [[nodiscard]] QString assistantResponse() const;
    /** @brief Returns any Assistant error text. */
    [[nodiscard]] QString assistantError() const;
    /** @brief Returns the deterministic Assistant conversation. */
    [[nodiscard]] QVariantList assistantMessages() const;
    /** @brief Returns whether a fictional Assistant request is in progress. */
    [[nodiscard]] bool assistantRequestInProgress() const;

    /** @brief No-op production action exposed for the screenshot UI. */
    Q_INVOKABLE void forceSyncNow();
    /** @brief No-op production action exposed for the screenshot UI. */
    Q_INVOKABLE void openServer();
    /** @brief No-op Assistant submission exposed for the screenshot UI. */
    Q_INVOKABLE void submitAssistantQuestion(const QString &question);
    /** @brief No-op conversation reset exposed for the screenshot UI. */
    Q_INVOKABLE void clearAssistantResponse();

signals:
    /** @brief Mirrors the production user's Assistant-capability notification. */
    void assistantStateChanged();
    /** @brief Mirrors the production user's response notification. */
    void assistantResponseChanged();
    /** @brief Mirrors the production user's error notification. */
    void assistantErrorChanged();
    /** @brief Mirrors the production user's conversation notification. */
    void assistantMessagesChanged();
    /** @brief Mirrors the production user's request-state notification. */
    void assistantRequestInProgressChanged();

private:
    Q_DISABLE_COPY_MOVE(ScreenshotUser)
};

}

#endif // SCREENSHOTUSER_H
