/*
 * Copyright (C) by Michael Schuster <michael@schuster.ms>
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

/****************************************************************************
**
** Based on Qt sourcecode:
**   qt5/qtbase/src/widgets/dialogs/qwizard.cpp
**
** https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/dialogs/qwizard.cpp?h=v5.13.0
**
** Original license:
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HEADERBANNER_H
#define HEADERBANNER_H

#include <QWidget>

class QLabel;
class QGridLayout;
class QPixmap;

namespace OCC {

class HeaderBanner : public QWidget
{
    Q_OBJECT
public:
    HeaderBanner(QWidget *parent = nullptr);

    void setup(const QString &title, const QPixmap &logo, const QPixmap &banner,
               const Qt::TextFormat titleFormat, const QString &styleSheet);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *titleLabel;
    QLabel *logoLabel;
    QGridLayout *layout;
    QPixmap bannerPixmap;
};

} // namespace OCC

#endif // HEADERBANNER_H
