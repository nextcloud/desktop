
/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "property.h"

#include <QPointer>
#include <QJsonObject>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Property> Property::fromJson(const QJsonObject &json)
{
    auto property = QPointer<Property>(new Property);
    property->_className = json.value("class"_L1).toString();
    property->_displayName = json.value("display_name"_L1).toString();
    property->_required = json.value("required"_L1).toBool();
    property->_hint = json.value("hint"_L1).toString();
    property->_type = json.value("type"_L1).toString();
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

bool Property::required() const
{
    return _required;
}

QString Property::hint() const
{
    return _hint;
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
