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

#include "resources.h"

#include <QDebug>
#include <QFileInfo>
#include <QPalette>

using namespace OCC;
using namespace Resources;
namespace {

QString vanillaThemePath()
{
    return QStringLiteral(":/client/ownCloud/theme");
}

QString brandThemePath()
{
    return QStringLiteral(":/client/" APPLICATION_SHORTNAME "/theme");
}

QString darkTheme()
{
    return QStringLiteral("dark");
}

QString coloredTheme()
{
    return QStringLiteral("colored");
}

QString whiteTheme()
{
    return QStringLiteral("white");
}

constexpr bool isVanilla()
{
    return std::string_view(APPLICATION_SHORTNAME) == "ownCloud";
}

bool hasTheme(IconType type, const QString &theme)
{
    // <<is vanilla, theme name>, bool
    // caches the availability of themes for branded and unbranded themes
    static QMap<QPair<bool, QString>, bool> _themeCache;
    const auto key = qMakePair(type != IconType::VanillaIcon, theme);
    auto it = _themeCache.constFind(key);
    if (it == _themeCache.cend()) {
        return _themeCache[key] = QFileInfo(QStringLiteral("%1/%2/").arg(type == IconType::VanillaIcon ? vanillaThemePath() : brandThemePath(), theme)).isDir();
    }
    return it.value();
}

}

bool OCC::Resources::hasDarkTheme()
{
    static bool _hasBrandedColored = hasTheme(IconType::BrandedIcon, coloredTheme());
    static bool _hasBrandedDark = hasTheme(IconType::BrandedIcon, darkTheme());
    return _hasBrandedColored == _hasBrandedDark;
}

bool Resources::hasMonoTheme()
{
    // mono icons are only supported in vanilla and if a customer provides them
    // no fallback to vanilla
    return hasTheme(Resources::IconType::BrandedIcon, whiteTheme());
}

bool OCC::Resources::isUsingDarkTheme()
{
    // TODO: replace by a command line switch
    static bool forceDark = qEnvironmentVariableIntValue("OWNCLOUD_FORCE_DARK_MODE") != 0;
    return forceDark || QPalette().base().color().lightnessF() <= 0.5;
}

QUrl Resources::getCoreIconUrl(const QString &icon_name)
{
    if (icon_name.isEmpty()) {
        return {};
    }
    const QString theme = Resources::isUsingDarkTheme() ? QStringLiteral("dark") : QStringLiteral("light");
    return QUrl(QStringLiteral("qrc:/client/resources/%1/%2").arg(theme, icon_name));
}

QIcon OCC::Resources::getCoreIcon(const QString &icon_name)
{
    if (icon_name.isEmpty()) {
        return {};
    }
    // QIcon doesn't like qrc:// urls...
    const QIcon icon(QLatin1Char(':') + getCoreIconUrl(icon_name).path());
    // were we able to load the file?
    Q_ASSERT(icon.actualSize({100, 100}).isValid());
    return icon;
}


/*
 * helper to load a icon from either the icon theme the desktop provides or from
 * the apps Qt resources.
 */
QIcon OCC::Resources::loadIcon(const QString &flavor, const QString &name, IconType iconType)
{
    static QMap<QString, QIcon> _iconCache;
    // prevent recusion
    const bool useCoreIcon = (iconType == IconType::VanillaIcon) || isVanilla();
    const QString path = useCoreIcon ? vanillaThemePath() : brandThemePath();
    const QString key = name + QLatin1Char(',') + flavor;
    QIcon &cached = _iconCache[key]; // Take reference, this will also "set" the cache entry
    if (cached.isNull()) {
        if (isVanilla() && QIcon::hasThemeIcon(name)) {
            // use from theme
            return cached = QIcon::fromTheme(name);
        }
        const QString svg = QStringLiteral("%1/%2/%3.svg").arg(path, flavor, name);
        if (QFile::exists(svg)) {
            return cached = QIcon(svg);
        }

        const QString png = QStringLiteral("%1/%2/%3.png").arg(path, flavor, name);
        if (QFile::exists(png)) {
            return cached = QIcon(png);
        }

        const QList<int> sizes{16, 22, 32, 48, 64, 128, 256, 512, 1024};
        QString previousIcon;
        for (int size : sizes) {
            QString pixmapName = QStringLiteral("%1/%2/%3-%4.png").arg(path, flavor, name, QString::number(size));
            if (QFile::exists(pixmapName)) {
                previousIcon = pixmapName;
                cached.addFile(pixmapName, {size, size});
            } else if (size >= 128) {
                if (!previousIcon.isEmpty()) {
                    qWarning() << "Upscaling:" << previousIcon << "to" << size;
                    cached.addPixmap(QPixmap(previousIcon).scaled({size, size}, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            }
        }
    }
    if (cached.isNull()) {
        if (!useCoreIcon && iconType == IconType::BrandedIconWithFallbackToVanillaIcon) {
            return loadIcon(flavor, name, IconType::VanillaIcon);
        }
        qWarning() << "Failed to locate the icon" << name;
    }
    return cached;
}

QIcon OCC::Resources::themeIcon(const QString &name, IconType iconType)
{
    return loadIcon((Resources::isUsingDarkTheme() && hasDarkTheme()) ? darkTheme() : coloredTheme(), name, iconType);
}

QIcon OCC::Resources::themeUniversalIcon(const QString &name, IconType iconType)
{
    return loadIcon(QStringLiteral("universal"), name, iconType);
}

CoreImageProvider::CoreImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}
QPixmap CoreImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto [type, path] = [&id] {
        const QString type = id.mid(0, id.indexOf(QLatin1Char('/')));
        return std::make_tuple(type, id.mid(type.size()));
    }();
    Q_ASSERT(!path.isEmpty());
    QIcon icon;
    if (type == QLatin1String("theme")) {
        icon = themeIcon(path);
    } else if (type == QLatin1String("core")) {
        icon = getCoreIcon(path);
    } else {
        Q_UNREACHABLE();
    }
    const QSize actualSize = requestedSize.isValid() ? requestedSize : icon.availableSizes().first();
    if (size) {
        *size = actualSize;
    }
    return icon.pixmap(actualSize);
}
