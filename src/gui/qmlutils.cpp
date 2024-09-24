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

#include "common/asserts.h"
#include "resources/resources.h"

#include <QMessageBox>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>

void OCC::QmlUtils::OCQuickWidget::setOCContext(const QUrl &src, QWidget *parentWidget, QObject *ocContext, QJSEngine::ObjectOwnership ownership)
{
    if (ownership == QJSEngine::CppOwnership) {
        // Destroying the `ocContext` will result in property changed signals, causing the re-evaluation
        // of the bindings in the QML file, which in turn results in warnings about accessing a property
        // of a `null` object.
        // To prevent this, reset the source to an empty URL.
        connect(
            ocContext, &QObject::destroyed, this, [this] { setSource(QUrl()); }, Qt::DirectConnection);
    }

    rootContext()->setContextProperty(QStringLiteral("ocParentWidget"), parentWidget);
    rootContext()->setContextProperty(QStringLiteral("ocContext"), ocContext);
    engine()->setObjectOwnership(ocContext, ownership);
    engine()->addImageProvider(QStringLiteral("ownCloud"), new OCC::Resources::CoreImageProvider());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(src);
    if (!errors().isEmpty()) {
        auto box = new QMessageBox(QMessageBox::Critical, QStringLiteral("QML Error"), QDebug::toString(errors()));
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->exec();
        qFatal("A qml error occured %s", qPrintable(QDebug::toString(errors())));
    }

    // string based connects as they are provided by OC_DECLARE_WIDGET_FOCUS and not inherited, assert to ensure the connection works
    OC_ASSERT(QObject::connect(this, SIGNAL(focusFirst()), parentWidget, SIGNAL(focusFirst())));
    OC_ASSERT(QObject::connect(this, SIGNAL(focusLast()), parentWidget, SIGNAL(focusLast())));
}

void OCC::QmlUtils::OCQuickWidget::setOCContext(const QUrl &src, QWidget *ocContext)
{
    setOCContext(src, ocContext, ocContext, QJSEngine::ObjectOwnership::CppOwnership);
}

void OCC::QmlUtils::OCQuickWidget::focusInEvent(QFocusEvent *event)
{
    switch (event->reason()) {
    case Qt::TabFocusReason:
        Q_EMIT focusFirst();
        break;
    case Qt::BacktabFocusReason:
        Q_EMIT focusLast();
        break;
    default:
        break;
    }
    QQuickWidget::focusInEvent(event);
}
bool OCC::QmlUtils::OCQuickWidget::event(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        rootObject()->setEnabled(isEnabled());
    }

    return QQuickWidget::event(event);
}
