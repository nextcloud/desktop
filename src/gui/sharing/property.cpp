
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "property.h"

#include <QPointer>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Property> Property::fromJson(const QJsonObject &json)
{
    auto property = QPointer<Property>(new Property);
    property->_className = json.value("class"_L1).toString();
    property->_displayName = json.value("display_name"_L1).toString();
    property->_priority = json.value("priority"_L1).toInt();
    property->_required = json.value("required"_L1).toBool();
    property->_advanced = json.value("advanced"_L1).toBool();
    property->_hint = json.value("hint"_L1).toString();
    property->_type = json.value("type"_L1).toString();
    const auto validValues = json.value("valid_values"_L1).toArray();
    property->_validValues.reserve(validValues.size());
    std::ranges::transform(validValues, std::back_inserter(property->_validValues), [](const auto &value) {
        return value.toString();
    });
    property->_minDate = json.value("min_date"_L1).toVariant();
    property->_maxDate = json.value("max_date"_L1).toVariant();
    property->_minLength = json.value("min_length"_L1).toVariant();
    property->_maxLength = json.value("max_length"_L1).toVariant();
    property->_value = json.value("value"_L1).toVariant();
    return property;
}

Property::Property(QObject *parent)
    : QObject{parent}
{
}

QString Property::className() const
{
    return _className;
}

QString Property::displayName() const
{
    return _displayName;
}

int Property::priority() const
{
    return _priority;
}

bool Property::required() const
{
    return _required;
}

bool Property::advanced() const
{
    return _advanced;
}

QString Property::hint() const
{
    return _hint;
}

QStringList Property::validValues() const
{
    return _validValues;
}

QVariant Property::minDate() const
{
    return _minDate;
}

QVariant Property::maxDate() const
{
    return _maxDate;
}

QVariant Property::minLength() const
{
    return _minLength;
}

QVariant Property::maxLength() const
{
    return _maxLength;
}

QString Property::type() const
{
    return _type;
}

QVariant Property::value() const
{
    return _value;
}

void Property::setValue(const QVariant &value)
{
    if (_value == value) {
        return;
    }

    _value = value;
    Q_EMIT valueChanged();
}
