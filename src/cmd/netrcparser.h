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

#ifndef NETRCPARSER_H
#define NETRCPARSER_H

#include <QHash>
#include <QPair>

namespace OCC {

/**
 * @brief Parser for netrc files
 * @ingroup cmd
 */
class NetrcParser
{
public:
    typedef QPair<QString, QString> LoginPair;

    NetrcParser(const QString &fileName = QString::null);
    bool parse();
    LoginPair find(const QString &machine);

private:
    void tryAddEntryAndClear(QString &machine, LoginPair &pair, bool &isDefault);
    QHash<QString, LoginPair> _entries;
    LoginPair _default;
    QString _fileName;
};

} // namespace OCC

#endif // NETRCPARSER_H
