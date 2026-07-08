/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>

namespace OCC::Gui::Sharing {

class Recipient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(QString value READ value CONSTANT)

public:
    [[nodiscard]] static QPointer<Recipient> fromJson(const QJsonObject &json);

    [[nodiscard]] QString className() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] QString value() const;

private:
    explicit Recipient(QObject *parent = nullptr);

    QString _className;
    QString _displayName;
    QString _value;
};

}
