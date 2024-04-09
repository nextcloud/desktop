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

#include "gui/qmlutils.h"

#include "resources/resources.h"

#include <QMessageBox>
#include <QQuickWidget>

void OCC::QmlUtils::initQuickWidget(QQuickWidget *widget, const QUrl &src)
{
    widget->engine()->addImageProvider(QStringLiteral("ownCloud"), new OCC::Resources::CoreImageProvider());
    widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    widget->setSource(src);
    if (!widget->errors().isEmpty()) {
        auto box = new QMessageBox(QMessageBox::Critical, QStringLiteral("QML Error"), QDebug::toString(widget->errors()));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->exec();
        qFatal("A qml error occured %s", qPrintable(QDebug::toString(widget->errors())));
    }
}
