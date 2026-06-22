/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    using LoginPair = QPair<QString, QString>;

    NetrcParser(const QString &file = QString());
    bool parse();
    LoginPair find(const QString &machine);

private:
    void tryAddEntryAndClear(QString &machine, LoginPair &pair, bool &isDefault);
    QHash<QString, LoginPair> _entries;
    LoginPair _default;
    QString _netrcLocation;
};

} // namespace OCC

#endif // NETRCPARSER_H
