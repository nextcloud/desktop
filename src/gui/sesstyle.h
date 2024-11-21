/****************************************************************************
**
** This file is part of the Oxygen2 project.
**
** SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
**
** SPDX-License-Identifier: MIT
**
****************************************************************************/

/*************************************************************************
 *
 * Copyright (c) 2013-2019, Klaralvdalens Datakonsult AB (KDAB)
 * All rights reserved.
 *
 * See the LICENSE.txt file shipped along with this file for the license.
 *
 *************************************************************************/
#pragma once

#include <QProxyStyle>
#include <memory>
#include <QStyleOptionButton>

class PushButtonStyleHelper;

// QProxyStyle to see all widgets in an application (even if it means mixing with the default style)
// QCommonStyle to only see what we have styled, no bad interactions from the default style, but the other widgets won't be usable
using super = QProxyStyle;

class sesStyle : public super
{
public:
    enum CustomControlElement {
        CE_TreeViewMoreOptions = QStyle::CE_CustomBase + 1,  // Custom element for a button
    };

    enum CustomContentsType {
        CT_TreeViewMoreOptions = QStyle::CT_CustomBase + 1,  // Custom element for a button
    };

    sesStyle();

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;

    int pixelMetric(PixelMetric pm, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;

    void drawButton(const QStyleOptionButton *btn, QPainter *painter, const QWidget *widget) const;

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;

    void drawComplexControl(ComplexControl complexControl, const QStyleOptionComplex *opt, QPainter *painter,
                            const QWidget *widget = nullptr) const override;

    int styleHint(StyleHint stylehint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const override;

    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const override;

    QRect subElementRect(SubElement subElement, const QStyleOption *option, const QWidget *widget) const override;

    void polish(QWidget *w) override;

    bool eventFilter(QObject *obj, QEvent *event) override;


    PushButtonStyleHelper* pushButtonStyleHelper() const;

private:
    std::unique_ptr<class PushButtonStyleHelper> mPushButtonStyleHelper;
    std::unique_ptr<class MoreOptionsButtonStyleHelper> mMoreOptionsButtonStyleHelper;

};