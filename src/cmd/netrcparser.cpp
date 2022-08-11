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

#include <qtokenizer.h>

#include <QDebug>

#include "netrcparser.h"

namespace OCC {

namespace {
    QString defaultKeyword = QStringLiteral("default");
    QString machineKeyword = QStringLiteral("machine");
    QString loginKeyword = QStringLiteral("login");
    QString passwordKeyword = QStringLiteral("password");
}

NetrcParser::NetrcParser(const QString &file)
{
    _netrcLocation = file;
    if (_netrcLocation.isEmpty()) {
        _netrcLocation = QDir::homePath() + QStringLiteral("/.netrc");
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

    QStringTokenizer tokenizer(content, QStringLiteral(" \n\t"));
    tokenizer.setQuoteCharacters(QStringLiteral("\"'"));

    LoginPair pair;
    QString machine;
    bool isDefault = false;
    while (tokenizer.hasNext()) {
        QString key = tokenizer.next();
        if (key == defaultKeyword) {
            tryAddEntryAndClear(machine, pair, isDefault);
            isDefault = true;
            continue; // don't read a value
        }

        if (!tokenizer.hasNext()) {
            qDebug() << "error fetching value for" << key;
            return false;
        }
        QString value = tokenizer.next();

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

NetrcParser::LoginPair NetrcParser::find(const QString &machine) const
{
    return _entries.value(machine, _default);
}

} // namespace OCC
