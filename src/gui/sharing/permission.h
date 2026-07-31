/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

namespace OCC::Gui::Sharing {

class Permission : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString hint READ hint CONSTANT)

public:
    [[nodiscard]] static QPointer<Permission> fromJson(const QJsonObject &json);

    [[nodiscard]] QString className() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool enabled() const;
    [[nodiscard]] QString hint() const;

    void setEnabled(bool enabled);

Q_SIGNALS:
    void enabledChanged();

private:
    explicit Permission(const QString &className, const QString &displayName, bool enabled, const QString &hint, QObject *parent = nullptr);

    QString _className;
    QString _displayName;
    bool _enabled;
    QString _hint;
};

}
