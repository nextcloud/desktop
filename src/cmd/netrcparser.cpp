/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include <QDir>
#include <QFile>
#include <QTextStream>

#include "netrcparser.h"

namespace OCC {

namespace {
QString defaultKeyword = QLatin1String("default");
QString machineKeyword = QLatin1String("machine");
QString loginKeyword = QLatin1String("login");
QString passwordKeyword = QLatin1String("password");

}

NetrcParser::NetrcParser(const QString &fileName)
    : _fileName(fileName)
{
    if (_fileName.isEmpty()) {
       _fileName = QDir::homePath()+QLatin1String("/.netrc");
    }
}

void NetrcParser::tryAddEntryAndClear(QString& machine, LoginPair& pair, bool& isDefault) {
    if (isDefault) {
        _default = pair;
    } else if (!machine.isEmpty() && !pair.first.isEmpty()){
        _entries.insert(machine, pair);
    }
    pair = qMakePair(QString(), QString());
    machine.clear();
    isDefault = false;
}

bool NetrcParser::parse()
{
    QFile netrc(_fileName);
    if (!netrc.open(QIODevice::ReadOnly)) {
        return false;
    }

    QTextStream ts(&netrc);
    LoginPair pair;
    QString machine;
    bool isDefault = false;
    while (!ts.atEnd()) {
        QString next;
        ts >> next;
        if (next == defaultKeyword) {
            tryAddEntryAndClear(machine, pair, isDefault);
            isDefault = true;
        }
        if (next == machineKeyword) {
            tryAddEntryAndClear(machine, pair, isDefault);
            ts >> machine;
        } else if (next == loginKeyword) {
            ts >> pair.first;
        } else if (next == passwordKeyword) {
            ts >> pair.second;
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
