/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>

class QUrl;

namespace OCC {

class Updater {
public:
    struct Helper {
        static qint64 stringVersionToInt(const QString& version);
        static qint64 currentVersionToInt();
        static qint64 versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build);
    };

    static Updater *instance();

    virtual void checkForUpdate() = 0;
    virtual void backgroundCheckForUpdate() = 0;

    virtual bool handleStartup() = 0;

protected:
    static QString clientVersion();

private:
    static QString getSystemInfo();
    static QUrl addQueryParams(const QUrl &url);
    static Updater *create();
    static Updater *_instance;
};

} // namespace OCC

#endif // UPDATER_H
