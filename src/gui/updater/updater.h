/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UPDATER_H
#define UPDATER_H

#include <QLoggingCategory>
#include <QObject>

class QUrl;
class QUrlQuery;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcUpdater)

class Updater : public QObject
{
    Q_OBJECT
public:
    struct Helper
    {
        static qint64 stringVersionToInt(const QString &version);
        static qint64 currentVersionToInt();
        static qint64 versionToInt(qint64 major, qint64 minor, qint64 patch, qint64 build);
    };

    static Updater *instance();
    static QUrl updateUrl();

    virtual void checkForUpdate() = 0;
    virtual void backgroundCheckForUpdate() = 0;
    virtual bool handleStartup() = 0;

protected:
    static QString clientVersion();
    Updater()
        : QObject(nullptr)
    {
    }

private:
    static QString getSystemInfo();
    static QUrlQuery getQueryParams();
    static Updater *create();
    static Updater *_instance;
};

} // namespace OCC

#endif // UPDATER_H
