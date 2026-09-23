/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "archive/anonymizer.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QRegularExpression>
#include <QStringList>
#include <QStringView>
#include <QUrl>

#include <algorithm>

namespace {
bool isIpv6Address(const QString &candidate)
{
    QHostAddress address;
    return address.setAddress(candidate) && address.protocol() == QAbstractSocket::IPv6Protocol;
}
}

namespace OCC {
void Anonymizer::seedUser(const QString &user)
{
    if (user.isEmpty()) {
        return;
    }
    addLiteral(user, tokenFor(user, Category::User));
}

void Anonymizer::seedServer(const QString &url)
{
    const auto host = QUrl(url).host();
    const auto value = host.isEmpty() ? url : host;
    if (value.isEmpty()) {
        return;
    }
    addLiteral(value, tokenFor(value, Category::Server));
}

void Anonymizer::seedPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    addLiteral(path, anonymizePath(path));
}

QString Anonymizer::anonymizeText(const QString &text)
{
    QString result = text;
    for (const auto &entry : _literals) {
        result.replace(entry.first, entry.second);
    }
    return anonymizeIps(result);
}

QString Anonymizer::anonymizePath(const QString &path)
{
    if (path.isEmpty()) {
        return path;
    }

    const auto segments = path.split(QLatin1Char('/'));
    auto lastIndex = -1;
    for (auto i = segments.size() - 1; i >= 0; --i) {
        if (!segments.at(i).isEmpty()) {
            lastIndex = i;
            break;
        }
    }

    QStringList out;
    out.reserve(segments.size());
    for (auto i = 0; i < segments.size(); ++i) {
        const auto &segment = segments.at(i);
        if (segment.isEmpty()) {
            out.append(segment);
        } else {
            out.append(tokenFor(segment, i == lastIndex ? Category::File : Category::Directory));
        }
    }
    return out.join(QLatin1Char('/'));
}

QMap<QString, QString> Anonymizer::mapping() const
{
    QMap<QString, QString> table;
    for (auto it = _values.cbegin(); it != _values.cend(); ++it) {
        table.insert(it.key(), it.value());
    }
    return table;
}

QString Anonymizer::tokenFor(const QString &real, Category category)
{
    if (const auto existing = _values.constFind(real); existing != _values.constEnd()) {
        return existing.value(); // same real string always yields the same pseudonym
    }

    QString token;
    switch (category) {
    case Category::User:
        token = QStringLiteral("user%1").arg(++_userCounter);
        break;
    case Category::Server:
        token = QStringLiteral("server%1.example").arg(++_serverCounter);
        break;
    case Category::Ip:
        token = QStringLiteral("127.0.0.%1").arg(++_ipCounter);
        break;
    case Category::Directory:
        token = QStringLiteral("dir_%1").arg(++_dirCounter);
        break;
    case Category::File: {
        const auto dot = real.lastIndexOf(QLatin1Char('.'));
        const auto extension = dot > 0 ? real.mid(dot) : QString();
        token = QStringLiteral("file_%1%2").arg(++_fileCounter).arg(extension);
        break;
    }
    }

    _values.insert(real, token);
    return token;
}

void Anonymizer::addLiteral(const QString &real, const QString &token)
{
    // Longest real first so a path containing a username is replaced before the bare username.
    const auto pos = std::find_if(_literals.cbegin(), _literals.cend(), [&real](const auto &entry) {
        return entry.first.size() < real.size();
    });
    _literals.insert(pos, {real, token});
}

QString Anonymizer::anonymizeIps(const QString &text)
{
    static const QRegularExpression ipv4(QStringLiteral(R"((?<![\w.])(?:(?:25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(?:25[0-5]|2[0-4]\d|1?\d?\d)(?![\w.]))"));
    // Broad IPv6 candidate; validated below so log timestamps like 12:34:56 are left alone.
    static const QRegularExpression ipv6(QStringLiteral(R"((?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{0,4}:){2,}[0-9A-Fa-f]{0,4})"));

    const auto replaceAll = [this](const QString &input, const QRegularExpression &re, bool validate) {
        QString output;
        output.reserve(input.size());
        qsizetype lastEnd = 0;
        auto it = re.globalMatch(input);
        while (it.hasNext()) {
            const auto match = it.next();
            const auto candidate = match.captured();
            output.append(QStringView{input}.mid(lastEnd, match.capturedStart() - lastEnd));
            lastEnd = match.capturedEnd();

            if (candidate.startsWith(QLatin1String("127.0.0."))) {
                output.append(candidate); // loopback or a token we already generated
                continue;
            }
            if (validate && !isIpv6Address(candidate)) {
                output.append(candidate); // not a real IPv6 address (e.g. a timestamp)
                continue;
            }
            output.append(tokenFor(candidate, Category::Ip));
        }
        output.append(QStringView{input}.mid(lastEnd));
        return output;
    };

    auto result = replaceAll(text, ipv6, true);
    result = replaceAll(result, ipv4, false);
    return result;
}

} // namespace OCC
