/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
#ifndef SITECOPYCONFIG_H
#define SITECOPYCONFIG_H

#include <QtCore>

namespace Mirall {
class SitecopyConfig
{
public:
    SitecopyConfig();

    void writeSiteConfig( const QString& localPath, const QString& siteAlias,
                          const QString& host, const QString& user,
                          const QString& passwd );
    bool parseSiteConfig();

private:
    void processConfigLine( const QString& );

    QHash<QString, QHash<QString, QString> > mSites;
    QHash<QString, QString>                  mCurrSite;
    QHash<QString, QStringList>              mChangesHash;
    QString mCurrSiteName;
};
};

#endif // SITECOPYCONFIG_H
