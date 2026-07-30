/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QVariant>

namespace OCC::Gui::Sharing {

/**
 * @brief A server-defined setting associated with a unified share.
 */
class Property : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString className READ className CONSTANT)
    Q_PROPERTY(QString displayName READ displayName CONSTANT)
    Q_PROPERTY(int priority READ priority CONSTANT)
    Q_PROPERTY(bool required READ required CONSTANT)
    Q_PROPERTY(bool advanced READ advanced CONSTANT)
    Q_PROPERTY(QString hint READ hint CONSTANT)
    Q_PROPERTY(QString type READ type CONSTANT)
    Q_PROPERTY(QStringList validValues READ validValues CONSTANT)
    Q_PROPERTY(QVariant minDate READ minDate CONSTANT)
    Q_PROPERTY(QVariant maxDate READ maxDate CONSTANT)
    Q_PROPERTY(QVariant minLength READ minLength CONSTANT)
    Q_PROPERTY(QVariant maxLength READ maxLength CONSTANT)
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)

public:
    /** @brief Creates a property from its unified sharing API representation. */
    [[nodiscard]] static QPointer<Property> fromJson(const QJsonObject &json);

    /** @brief Returns the registered server class identifying this property. */
    [[nodiscard]] QString className() const;
    /** @brief Returns the user-facing property label. */
    [[nodiscard]] QString displayName() const;
    /** @brief Returns the server-provided ordering priority. */
    [[nodiscard]] int priority() const;
    /** @brief Returns whether the share requires a value for this property. */
    [[nodiscard]] bool required() const;
    /** @brief Returns whether this property belongs in the advanced settings section. */
    [[nodiscard]] bool advanced() const;
    /** @brief Returns the server-defined property type. */
    [[nodiscard]] QString type() const;
    /** @brief Returns the optional user-facing input hint. */
    [[nodiscard]] QString hint() const;
    /** @brief Returns the allowed values for an enum property. */
    [[nodiscard]] QStringList validValues() const;
    /** @brief Returns the optional ISO 8601 lower bound for a date property. */
    [[nodiscard]] QVariant minDate() const;
    /** @brief Returns the optional ISO 8601 upper bound for a date property. */
    [[nodiscard]] QVariant maxDate() const;
    /** @brief Returns the optional minimum length for a string property. */
    [[nodiscard]] QVariant minLength() const;
    /** @brief Returns the optional maximum length for a string property. */
    [[nodiscard]] QVariant maxLength() const;
    /** @brief Returns the current server value. */
    [[nodiscard]] QVariant value() const;

    /** @brief Updates the current value and emits valueChanged when it changes. */
    void setValue(const QVariant &value);

Q_SIGNALS:
    /** @brief Emitted after the current value changes. */
    void valueChanged();

private:
    explicit Property(QObject *parent = nullptr);

    QString _className;
    QString _displayName;
    int _priority = 0;
    bool _required = false;
    bool _advanced = false;
    QString _hint;
    QString _type;
    QStringList _validValues;
    QVariant _minDate;
    QVariant _maxDate;
    QVariant _minLength;
    QVariant _maxLength;
    QVariant _value;
};

}
