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

#include "headerbanner.h"

#include <QVBoxLayout>
#include <QLabel>

#include <QPainter>
#include <QStyle>
#include <QGuiApplication>

namespace OCC {

// These fudge terms were needed a few places to obtain pixel-perfect results
const int GapBetweenLogoAndRightEdge = 5;
const int ModernHeaderTopMargin = 2;

HeaderBanner::HeaderBanner(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setBackgroundRole(QPalette::Base);
    titleLabel = new QLabel(this);
    titleLabel->setBackgroundRole(QPalette::Base);
    logoLabel = new QLabel(this);
    QFont font = titleLabel->font();
    font.setBold(true);
    titleLabel->setFont(font);
    layout = new QGridLayout(this);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    layout->setRowMinimumHeight(3, 1);
    layout->setRowStretch(4, 1);
    layout->setColumnStretch(2, 1);
    layout->setColumnMinimumWidth(4, 2 * GapBetweenLogoAndRightEdge);
    layout->setColumnMinimumWidth(6, GapBetweenLogoAndRightEdge);
    layout->addWidget(titleLabel, 1, 1, 5, 1);
    layout->addWidget(logoLabel, 1, 5, 5, 1);
}

void HeaderBanner::setup(const QString &title, const QPixmap &logo, const QPixmap &banner,
                         const Qt::TextFormat titleFormat, const QString &styleSheet)
{
    QStyle *style = parentWidget()->style();
    //const int layoutHorizontalSpacing = style->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
    int topLevelMarginLeft = style->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, parentWidget());
    int topLevelMarginRight = style->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, parentWidget());
    int topLevelMarginTop = style->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, parentWidget());
    //int topLevelMarginBottom = style->pixelMetric(QStyle::PM_LayoutBottomMargin, 0, parentWidget());

    layout->setRowMinimumHeight(0, ModernHeaderTopMargin);
    layout->setRowMinimumHeight(1, topLevelMarginTop - ModernHeaderTopMargin - 1);
    layout->setRowMinimumHeight(6, 3);
    int minColumnWidth0 = topLevelMarginLeft + topLevelMarginRight;
    int minColumnWidth1 = topLevelMarginLeft + topLevelMarginRight + 1;
    layout->setColumnMinimumWidth(0, minColumnWidth0);
    layout->setColumnMinimumWidth(1, minColumnWidth1);
    titleLabel->setTextFormat(titleFormat);
    titleLabel->setText(title);
    if(!styleSheet.isEmpty())
        titleLabel->setStyleSheet(styleSheet);
    logoLabel->setPixmap(logo);
    bannerPixmap = banner;
    if (bannerPixmap.isNull()) {
        QSize size = layout->totalMinimumSize();
        setMinimumSize(size);
        setMaximumSize(QWIDGETSIZE_MAX, size.height());
    } else {
        setFixedHeight(banner.height() + 2);
    }
    updateGeometry();
}

void HeaderBanner::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, width(), bannerPixmap.height(), bannerPixmap);
    int x = width() - 2;
    int y = height() - 2;
    const QPalette &pal = QGuiApplication::palette();
    painter.setPen(pal.mid().color());
    painter.drawLine(0, y, x, y);
    painter.setPen(pal.base().color());
    painter.drawPoint(x + 1, y);
    painter.drawLine(0, y + 1, x + 1, y + 1);
}

} // namespace OCC
