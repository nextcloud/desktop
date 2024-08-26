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

#include <QtWidgets/QWidget>

namespace OCC {
namespace Resources {

    class OWNCLOUDRESOURCES_EXPORT ThemeWatcher : public QObject
    {
        Q_OBJECT
    public:
        ThemeWatcher(QObject *parent = nullptr);

    Q_SIGNALS:
        void themeChanged();

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;
    };

} // Resources
} // OCC
