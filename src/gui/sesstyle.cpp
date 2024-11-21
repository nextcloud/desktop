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

#include "sesstyle.h"
#include "ionostheme.h"

#include "pushbuttonstylehelper.h"
#include "moreoptionsbuttonstylehelper.h"

#include <QCheckBox>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QPainter>
#include <QStyleOption>

sesStyle::sesStyle()
    : super()
    , mPushButtonStyleHelper(new PushButtonStyleHelper)
    , mMoreOptionsButtonStyleHelper(new MoreOptionsButtonStyleHelper)
{
}

void sesStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    switch (pe) {
    case PE_FrameFocusRect:
        // nothing, we don't want focus rects
        break;
    #ifdef Q_OS_MAC   
    case PE_IndicatorBranch:
        {

            QStyleOption optCopy = *option;
            int originalWidth = optCopy.rect.width();
            int originalHeight = optCopy.rect.height();
            optCopy.rect.setWidth(static_cast<int>(originalWidth * 0.5));
            optCopy.rect.setHeight(static_cast<int>(originalHeight * 0.5));
            optCopy.rect.translate(5, 5);

            if (!(option->state & State_Children))
                break;
            if (option->state & State_Open)
                drawPrimitive(PE_IndicatorArrowDown, &optCopy, painter, widget);
            else {
                const bool reverse = (option->direction == Qt::RightToLeft);
                drawPrimitive(reverse ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight, &optCopy, painter, widget);
            }
            break;
        }
    #endif
    default:
        super::drawPrimitive(pe, option, painter, widget);
        break;
    }
}

int sesStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0; // no shift
    case PM_ButtonMargin:
        return 16;
    default:
        return super::pixelMetric(metric, option, widget);
    }
}

void sesStyle::drawButton(const QStyleOptionButton *btn, QPainter *painter, const QWidget *widget) const {
    proxy()->drawControl(CE_PushButtonBevel, btn, painter, widget);
    QStyleOptionButton subopt = *btn;
    subopt.rect = subElementRect(SE_PushButtonContents, btn, widget);
    proxy()->drawControl(CE_PushButtonLabel, &subopt, painter, widget);
    if (btn->state & State_HasFocus) {
        QStyleOptionFocusRect fropt;
        fropt.QStyleOption::operator=(*btn);
        fropt.rect = subElementRect(SE_PushButtonFocusRect, btn, widget);
        proxy()->drawPrimitive(PE_FrameFocusRect, &fropt, painter, widget);
    }
}


void sesStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    switch (element) {
    case CE_TreeViewMoreOptions:
    {
        if (const auto *btn = qstyleoption_cast<const QStyleOptionButton *>(option))
        {
            // Bevel
            mMoreOptionsButtonStyleHelper->drawToolButtonShape(btn, painter, widget);

            // Label / Icon
            QStyleOptionButton subopt = *btn;
            subopt.rect = subElementRect(SE_PushButtonContents, btn, widget);
            mMoreOptionsButtonStyleHelper->adjustIconColor(&subopt, widget);
            QCommonStyle::drawControl(CE_PushButtonLabel, &subopt, painter, widget);
        }
        return;
    }
    case CE_PushButton:
    {
        if (const auto *btn = qstyleoption_cast<const QStyleOptionButton *>(option))
        {
             drawButton(btn, painter, widget);
        }
        return;
    }
    case CE_PushButtonBevel:
        if (const auto *optionButton = qstyleoption_cast<const QStyleOptionButton *>(option))
        {
             mPushButtonStyleHelper->drawButtonShape(optionButton, painter, widget);
        }
        return;
    case CE_PushButtonLabel:
        if (const auto *optionButton = qstyleoption_cast<const QStyleOptionButton *>(option))
        {
            QStyleOptionButton customStyleCopy = *optionButton;
            mPushButtonStyleHelper->adjustTextPalette(&customStyleCopy, widget);

            painter->save();
            QFont font = painter->font();
            mPushButtonStyleHelper->setFont(font);
            painter->setFont(font);

            QCommonStyle::drawControl(element, &customStyleCopy, painter, widget);
            painter->restore();
        }
        return;
    default:
        super::drawControl(element, option, painter, widget);
    }
}

void sesStyle::polish(QWidget *w)
{
    if (qobject_cast<QPushButton *>(w) || qobject_cast<QCheckBox *>(w)) {
        w->setAttribute(Qt::WA_Hover);
    }
    super::polish(w);
}

bool sesStyle::eventFilter(QObject *obj, QEvent *event)
{
    return super::eventFilter(obj, event);
}

PushButtonStyleHelper *sesStyle::pushButtonStyleHelper() const
{
    return mPushButtonStyleHelper.get();
}

int sesStyle::styleHint(StyleHint stylehint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    switch (stylehint) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return 0;
    default:
        break;
    }

    return super::styleHint(stylehint, option, widget, returnData);
}

QSize sesStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize, const QWidget *widget) const
{
    switch (type) {
    case CT_PushButton:
        if (const auto *buttonOption = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            return mPushButtonStyleHelper->sizeFromContents(buttonOption, contentsSize, widget, pixelMetric(PM_ButtonMargin, buttonOption, widget));
        }
        break;
    case CT_TreeViewMoreOptions:
    {
        if (const auto *buttonOption = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            return super::sizeFromContents(CT_ToolButton, option, contentsSize, widget);
        }
        break;
    }
    default:
        break;
    }
    return super::sizeFromContents(type, option, contentsSize, widget);
}

QRect sesStyle::subElementRect(SubElement subElement, const QStyleOption *option, const QWidget *widget) const
{
    switch (subElement) {
    default:
        return super::subElementRect(subElement, option, widget);
    }
}

void sesStyle::drawComplexControl(ComplexControl complexControl, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    switch (complexControl) {
    default:
        super::drawComplexControl(complexControl, option, painter, widget);
    }
}