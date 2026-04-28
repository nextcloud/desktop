/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QVariant>

namespace OCC::Gui::Sharing {

class Property : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(bool required READ required CONSTANT)
    Q_PROPERTY(QString hint READ hint CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)

public:
    [[nodiscard]] static QPointer<Property> fromJson(const QJsonObject &json);

    [[nodiscard]] QString className() const;
    [[nodiscard]] QString displayName() const;
    [[nodiscard]] bool required() const;
    [[nodiscard]] QString type() const;
    [[nodiscard]] QString hint() const;
    [[nodiscard]] QVariant value() const;

    void setValue(const QVariant &value);

Q_SIGNALS:
    void valueChanged();

private:
    explicit Property(QObject *parent = nullptr);

    QString _className;
    QString _displayName;
    bool _required;
    QString _hint;
    QString _type;
    QVariant _value;
};

}
