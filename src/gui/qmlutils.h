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
#include <QJSEngine>
#include <QtQuickWidgets/QQuickWidget>

class QUrl;

#define OC_DECLARE_WIDGET_FOCUS                                                                                                                                \
public:                                                                                                                                                        \
    Q_INVOKABLE void focusNext()                                                                                                                               \
    {                                                                                                                                                          \
        focusNextChild();                                                                                                                                      \
    }                                                                                                                                                          \
    Q_INVOKABLE void focusPrevious()                                                                                                                           \
    {                                                                                                                                                          \
        focusPreviousChild();                                                                                                                                  \
    }                                                                                                                                                          \
                                                                                                                                                               \
Q_SIGNALS:                                                                                                                                                     \
    void focusFirst();                                                                                                                                         \
    void focusLast();                                                                                                                                          \
                                                                                                                                                               \
private:

namespace OCC::QmlUtils {
class OCQuickWidget : public QQuickWidget
{
    Q_OBJECT
    // override of the enabled property of QWidget, but with a notifier
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(QWidget* parentFocusWidget MEMBER _parentFocusWidget FINAL)
    QML_ELEMENT
    QML_UNCREATABLE("C++")
public:
    using QQuickWidget::QQuickWidget;
    void setOCContext(const QUrl &src, QWidget *parentFocusWidget, QObject *ocContext, QJSEngine::ObjectOwnership ownership);
    void setOCContext(const QUrl &src, QWidget *ocContext);

Q_SIGNALS:
    void focusFirst();
    void focusLast();

    void enabledChanged();

protected:
    void focusInEvent(QFocusEvent *event) override;

    bool event(QEvent *event) override;

private:
    QWidget *_parentFocusWidget;
};
}
