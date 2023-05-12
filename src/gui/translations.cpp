/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 * Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "translations.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

namespace OCC {

namespace Translations {

    Q_LOGGING_CATEGORY(lcTranslations, "gui.translations", QtInfoMsg)

    const QString translationsFilePrefix()
    {
        return QStringLiteral("client_");
    }

    const QString translationsFileSuffix()
    {
        return QStringLiteral(".qm");
    }

    QString translationsDirectoryPath()
    {
        return QStringLiteral(":/client/translations/");
    }

    QSet<QString> listAvailableTranslations()
    {
        QSet<QString> availableTranslations;

        // calculate a glob pattern which can be used in the iterator below to match only translations files
        const QString pattern = translationsFilePrefix() + QLatin1Char('*') + translationsFileSuffix();

        QDirIterator it(Translations::translationsDirectoryPath(), QStringList() << pattern);

        while (it.hasNext()) {
            // extract locale part from filename
            // of course, this method relies on the filenames to be accurate
            const auto fileName = QFileInfo(it.next()).fileName();
            const auto localePartLength = fileName.length() - translationsFileSuffix().length() - translationsFilePrefix().length();
            QString localeName = fileName.mid(translationsFilePrefix().length(), localePartLength);

            availableTranslations.insert(localeName);
        }

        return availableTranslations;
    }

} // namespace Translations

} // namespace OCC
