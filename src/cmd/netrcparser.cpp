/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include <QDebug>

#include "netrcparser.h"

namespace OCC {

namespace {
    QString defaultKeyword = QLatin1String("default");
    QString machineKeyword = QLatin1String("machine");
    QString loginKeyword = QLatin1String("login");
    QString passwordKeyword = QLatin1String("password");
}

NetrcParser::NetrcParser(const QString &file)
{
    _netrcLocation = file;
    if (_netrcLocation.isEmpty()) {
        _netrcLocation = QDir::homePath() + QLatin1String("/.netrc");
    }
}

void NetrcParser::tryAddEntryAndClear(QString &machine, LoginPair &pair, bool &isDefault)
{
    if (isDefault) {
        _default = pair;
    } else if (!machine.isEmpty() && !pair.first.isEmpty()) {
        _entries.insert(machine, pair);
    }
    pair = qMakePair(QString(), QString());
    machine.clear();
    isDefault = false;
}

bool NetrcParser::parse()
{
    QFile netrc(_netrcLocation);
    if (!netrc.open(QIODevice::ReadOnly)) {
        return false;
    }
    QString content = netrc.readAll();
    if (content.isEmpty()) {
        return false;
    }

    auto tokens = content.split(QRegularExpression("\\s+"));

    LoginPair pair;
    QString machine;
    bool isDefault = false;
    for(int i=0; i<tokens.count(); i++) {
        const auto key = tokens[i];
        if (key == defaultKeyword) {
            tryAddEntryAndClear(machine, pair, isDefault);
            isDefault = true;
            continue; // don't read a value
        }

        i++;
        if (i >= tokens.count()) {
            qDebug() << "error fetching value for" << key;
            break;
        }
        auto value = tokens[i];

        if (key == machineKeyword) {
            tryAddEntryAndClear(machine, pair, isDefault);
            machine = value;
        } else if (key == loginKeyword) {
            pair.first = value;
        } else if (key == passwordKeyword) {
            pair.second = value;
        } // ignore unsupported tokens
    }
    tryAddEntryAndClear(machine, pair, isDefault);

    if (!_entries.isEmpty() || _default != qMakePair(QString(), QString())) {
        return true;
    } else {
        return false;
    }
}

NetrcParser::LoginPair NetrcParser::find(const QString &machine)
{
    QHash<QString, LoginPair>::const_iterator it = _entries.find(machine);
    if (it != _entries.end()) {
        return *it;
    } else {
        return _default;
    }
}

} // namespace OCC
