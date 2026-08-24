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
#include "whitelabeltheme.h"

#include "pushbuttonstylehelper.h"
#include "moreoptionsbuttonstylehelper.h"

#include <QCheckBox>
#include <QPushButton>
#include <QStyleOptionButton>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStyleOption>

sesStyle::sesStyle(QStyle* baseStyle)
    : super(baseStyle)
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

    case PE_IndicatorCheckBox:
    case PE_IndicatorItemViewItemCheck:
        drawCheckboxIndicator(pe, option, painter, widget);
        break;

#ifdef Q_OS_MAC
    case PE_PanelItemViewItem:
        if (option->state & State_MouseOver) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(OCC::WLTheme.treeViewHoverColor()));
            painter->drawRoundedRect(option->rect, 4, 4);
            painter->restore();
        } else {
            super::drawPrimitive(pe, option, painter, widget);
        }
        break;
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

void sesStyle::drawCheckboxIndicator(PrimitiveElement pe, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    Q_UNUSED(pe);
    Q_UNUSED(widget);

    // Own fill/checkmark colors - checked fill matches the app's actual primary-button blue
    // (WLTheme.buttonPrimaryColor(), same getter PushButtonStyleHelper uses), so it stays
    // consistent with buttons in both Light and Dark Mode instead of a fixed single value.
    // The box shape approximates each OS's own corner rounding rather than one fixed radius -
    // Qt doesn't expose the native style's actual radius to query, so this is a maintained
    // approximation, not a literal native shape.
#if defined(Q_OS_WIN)
    constexpr qreal radius = 3.0; // QWindows11Style's Fluent rounding
#elif defined(Q_OS_MACOS)
    constexpr qreal radius = 5.0; // macOS's more rounded checkbox
#else
    constexpr qreal radius = 2.0; // GTK/Breeze-style checkboxes sit closer to square
#endif

    const bool checked = option->state & (State_On | State_NoChange);
    const bool enabled = option->state & State_Enabled;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (!enabled) {
        painter->setOpacity(0.5);
    }

    const QRectF rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath boxPath;
    boxPath.addRoundedRect(rect, radius, radius);

    if (checked) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(OCC::WLTheme.buttonPrimaryColor()));
    } else {
        const auto borderWidth = qMax(1.0, rect.height() / 10.0);
        painter->setPen(QPen(QColor(OCC::WLTheme.buttonSecondaryBorderColor()), borderWidth));
        painter->setBrush(QColor(OCC::WLTheme.dialogBackgroundColor()));
    }
    painter->drawPath(boxPath);

    if (checked) {
        const auto color = QColor(OCC::WLTheme.checkboxCheckmarkColor());
        const auto penWidth = qMax(2, qRound(rect.height() / 7));
        painter->setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        if (option->state & State_NoChange) {
            painter->drawLine(QPointF(rect.left() + rect.width() * 0.25, rect.center().y()),
                               QPointF(rect.right() - rect.width() * 0.25, rect.center().y()));
        } else {
            QPainterPath checkPath;
            checkPath.moveTo(rect.left() + rect.width() * 0.22, rect.center().y());
            checkPath.lineTo(rect.left() + rect.width() * 0.42, rect.bottom() - rect.height() * 0.22);
            checkPath.lineTo(rect.right() - rect.width() * 0.20, rect.top() + rect.height() * 0.25);
            painter->drawPath(checkPath);
        }
    }

    painter->restore();
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