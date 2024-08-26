/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "template.h"

#include "common/asserts.h"

#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

using namespace OCC::Resources;

Q_LOGGING_CATEGORY(lcResourcesTeplate, "sync.resoruces.template", QtInfoMsg)

QString Template::renderTemplateFromFile(const QString &filePath, const QMap<QString, QString> &values)
{
    return renderTemplate(
        [&] {
            QFile f(filePath);
            OC_ASSERT(f.open(QFile::ReadOnly));
            return QString::fromUtf8(f.readAll());
        }(),
        values);
}

QString Template::renderTemplate(QString &&templ, const QMap<QString, QString> &values)
{
    static const QRegularExpression pattern(QStringLiteral("@{([^{}]+)}"));
    const auto replace = [&templ, &values](QRegularExpressionMatchIterator it) {
        while (it.hasNext()) {
            const auto match = it.next();
            Q_ASSERT(match.lastCapturedIndex() == 1);
            Q_ASSERT([&] {
                if (!values.contains(match.captured(1))) {
                    qWarning(lcResourcesTeplate) << "Unknown key:" << match.captured(1);
                    return false;
                }
                return true;
            }());
            templ.replace(match.captured(0), values.value(match.captured(1)));
        }
    };

    auto matches = pattern.globalMatch(templ);
    do {
        replace(matches);
        // the placeholder can again contain a placeholder
        matches = pattern.globalMatch(templ);
    } while (matches.hasNext());

    return templ;
}
