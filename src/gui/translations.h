/*
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

#ifndef TRANSLATIONSMANAGER_H
#define TRANSLATIONSMANAGER_H

#include <QSet>
#include <QString>

namespace OCC {

namespace Translations {

    /**
    * @return translation files' filename prefix
    */
    const QString translationsFilePrefix();

    /**
    * @returntranslation files' filename suffix
    */
    const QString translationsFileSuffix();

    /**
     * @return path to translation files
     */
    QString translationsDirectoryPath();

    /**
     * @return list of locales for which translations are available
     */
    QSet<QString> listAvailableTranslations();

} // namespace Translations

} // namespace OCC

#endif // TRANSLATIONSMANAGER_H
