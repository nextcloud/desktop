/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "recipient.h"

#include <QPointer>
#include <QJsonObject>

using namespace Qt::StringLiterals;

using namespace OCC::Gui::Sharing;

QPointer<Recipient> Recipient::fromJson(const QJsonObject &json)
{
    auto recipient = QPointer<Recipient>(new Recipient);
    recipient->_className = json.value("class"_L1).toString();
    recipient->_displayName = json.value("display_name"_L1).toString();
    recipient->_value = json.value("value"_L1).toString();
    return recipient;
}

Recipient::Recipient(QObject *parent)
    : QObject{parent}
{
}

QString Recipient::className() const
{
    return _className;
}

QString Recipient::displayName() const
{
    return _displayName;
}

QString Recipient::value() const
{
    return _value;
}
