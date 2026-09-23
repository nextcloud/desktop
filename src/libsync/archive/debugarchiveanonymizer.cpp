/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archive/debugarchiveanonymizer.h"

#include "archive/anonymizer.h"

#include <zlib.h>

#include <QFile>
#include <QMap>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

#include <array>

namespace
{
bool isCredentialKey(const QString &key)
{
    static const QStringList credentialMarkers = {
        QStringLiteral("password"),
        QStringLiteral("passwd"),
        QStringLiteral("token"),
        QStringLiteral("secret"),
        QStringLiteral("credential"),
        QStringLiteral("apppassword"),
    };
    const auto loweredKey = key.toLower();
    for (const auto &marker : credentialMarkers) {
        if (loweredKey.contains(marker)) {
            return true;
        }
    }
    return false;
}
}

namespace OCC
{

namespace DebugArchiveAnonymizer
{

bool anonymizeLogFile(Anonymizer &anonymizer, const QString &sourcePath, const QString &destinationPath)
{
    QSaveFile destinationFile(destinationPath);
    if (!destinationFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QTextStream destinationStream(&destinationFile);

    if (sourcePath.endsWith(QStringLiteral(".gz"))) {
        const auto gzipFile = gzopen(sourcePath.toUtf8().constData(), "rb");
        if (!gzipFile) {
            return false;
        }
        std::array<char, 8192> buffer{};
        QString pendingLine;
        while (gzgets(gzipFile, buffer.data(), static_cast<int>(buffer.size()))) {
            pendingLine += QString::fromUtf8(buffer.data());
            if (pendingLine.endsWith(QLatin1Char('\n'))) {
                destinationStream << anonymizer.anonymizeText(pendingLine);
                pendingLine.clear();
            }
        }
        if (!pendingLine.isEmpty()) {
            destinationStream << anonymizer.anonymizeText(pendingLine);
        }
        gzclose(gzipFile);
    } else {
        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            return false;
        }
        QTextStream sourceStream(&sourceFile);
        while (!sourceStream.atEnd()) {
            const auto line = sourceStream.readLine();
            destinationStream << anonymizer.anonymizeText(line) << '\n';
        }
    }

    destinationStream.flush();
    return destinationFile.commit();
}

bool anonymizeConfigFile(Anonymizer &anonymizer, const QString &sourcePath, const QString &destinationPath)
{
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    QSaveFile destinationFile(destinationPath);
    if (!destinationFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream sourceStream(&sourceFile);
    QTextStream destinationStream(&destinationFile);
    while (!sourceStream.atEnd()) {
        const auto line = sourceStream.readLine();
        const auto separatorIndex = line.indexOf(QLatin1Char('='));
        if (separatorIndex > 0 && isCredentialKey(line.left(separatorIndex))) {
            continue; // drop credential lines entirely
        }
        destinationStream << anonymizer.anonymizeText(line) << '\n';
    }

    destinationStream.flush();
    return destinationFile.commit();
}

bool writeLookupTable(const Anonymizer &anonymizer, const QString &destinationPath)
{
    QSaveFile destinationFile(destinationPath);
    if (!destinationFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QTextStream destinationStream(&destinationFile);
    destinationStream << "# real\tanonymized\n";
    destinationStream << "# Keep this file private. Do not share it with the archive.\n";
    const auto table = anonymizer.mapping();
    for (auto iterator = table.cbegin(); iterator != table.cend(); ++iterator) {
        destinationStream << iterator.key() << '\t' << iterator.value() << '\n';
    }
    destinationStream.flush();
    return destinationFile.commit();
}

} // namespace DebugArchiveAnonymizer

} // namespace OCC
