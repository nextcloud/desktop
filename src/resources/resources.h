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

#pragma once
#include "resources/owncloudresources.h"

#include <QIcon>
#include <QUrl>
#include <QtQuick/QQuickImageProvider>

namespace OCC::Resources {
Q_NAMESPACE
/**
 * Wehther we allow a fallback to a vanilla icon
 */
enum class IconType { BrandedIcon, BrandedIconWithFallbackToVanillaIcon, VanillaIcon };
Q_ENUM_NS(IconType);


/**
 *
 * @return Whether we are using the vanilla theme
 */
bool OWNCLOUDRESOURCES_EXPORT isVanillaTheme();

/**
 * Whether use the dark icon theme
 * The function also ensures the theme supports the dark theme
 */
bool OWNCLOUDRESOURCES_EXPORT isUsingDarkTheme();

bool OWNCLOUDRESOURCES_EXPORT hasDarkTheme();

/** Whether the theme provides monochrome tray icons
 */
bool OWNCLOUDRESOURCES_EXPORT hasMonoTheme();

QUrl OWNCLOUDRESOURCES_EXPORT getCoreIconUrl(const QString &icon_name);
QIcon OWNCLOUDRESOURCES_EXPORT getCoreIcon(const QString &icon_name);

QIcon OWNCLOUDRESOURCES_EXPORT loadIcon(const QString &flavor, const QString &name, IconType iconType);
QIcon OWNCLOUDRESOURCES_EXPORT themeIcon(const QString &name, IconType iconType = IconType::BrandedIconWithFallbackToVanillaIcon);

/**
 * Returns a universal (non color schema aware) icon.
 */
QIcon OWNCLOUDRESOURCES_EXPORT themeUniversalIcon(const QString &name, IconType iconType = IconType::BrandedIcon);

class OWNCLOUDRESOURCES_EXPORT CoreImageProvider : public QQuickImageProvider
{
    Q_OBJECT
public:
    CoreImageProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
}
