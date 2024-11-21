/****************************************************************************
**
** This file is part of the Oxygen2 project.
**
** SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
**
** SPDX-License-Identifier: MIT
**
****************************************************************************/

#include "pushbuttonstylehelper.h"


#include "buttonstyle.h"
#include "buttonstylestrategy.h"
#include "ionostheme.h"
#include <QMap>
#include <QPainter>
#include <QStyleOptionButton>
#include <QPushButton>
#include <QWidget>

void PushButtonStyleHelper::setupPainterForShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget)
{
    OCC::ButtonStyle& style = ButtonStyleStrategy::getButtonStyle(widget, option);

    // Disabled
    if (!(option->state & QStyle::State_Enabled)) {
        painter->setPen(QColor(style.buttonDisabledBorderColor()));
        painter->setBrush(QColor(style.buttonDisabledColor()));
    }
    //Pressed
    else if (option->state & QStyle::State_Sunken) {

        painter->setPen(QColor(style.buttonPressedBorderColor()));
        painter->setBrush(QColor(style.buttonPressedColor()));
    }
    // Hover
    else if(option->state & QStyle::State_MouseOver)
    {
        painter->setPen(QColor(style.buttonHoverBorderColor()));
        painter->setBrush(QColor(style.buttonHoverColor()));
    }
    // Focused
    else if (option->state & QStyle::State_HasFocus) {
        painter->setPen(QColor(style.buttonFocusedBorderColor()));
        painter->setBrush(QColor(style.buttonFocusedColor()));
    }
    // Else - Just beeing there
    else {
        painter->setPen(QColor(style.buttonDefaultBorderColor()));
        painter->setBrush(QColor(style.buttonDefaultColor()));
    }
}

void PushButtonStyleHelper::drawButtonShape(const QStyleOptionButton *option, QPainter *painter, const QWidget *widget)
{
    painter->save();
    painter->setRenderHints(QPainter::Antialiasing);
    setupPainterForShape(option, painter, widget);
    const int radius = OCC::IonosTheme::buttonRadiusInt();
    painter->drawRoundedRect(option->rect, radius, radius);
    painter->restore();
}

void PushButtonStyleHelper::setFont(QFont& font) const
{
    font.setWeight(OCC::IonosTheme::settingsTitleWeightDemiBold());
    font.setPixelSize(OCC::IonosTheme::settingsTextPixel());
}

void PushButtonStyleHelper::recalculateContentSize(QSize &contentsSize, const QWidget *widget) const
{
    QFont font = widget->font();
    setFont(font);
    QFontMetrics fm(font);

    //Code aus qpushbutton.cpp - sizeHint
    const QPushButton* pushButton = qobject_cast<const QPushButton*>(widget);
    int w = 0, h = 0;
    QString s(pushButton->text());
    bool empty = s.isEmpty();
    if (empty)
        s = QStringLiteral("XXXX");
    QSize sz = fm.size(Qt::TextShowMnemonic, s);
    if (!empty || !w)
        w += sz.width();
    if (!empty || !h)
        h = qMax(h, sz.height());
    // -- end code

    contentsSize.setHeight(h);
    contentsSize.setWidth(w);
}

QSize PushButtonStyleHelper::sizeFromContents(const QStyleOptionButton *option, QSize contentsSize, const QWidget *widget, int margin) const
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    if(widget != nullptr)
    {
        recalculateContentSize(contentsSize, widget);
    }
    const int frameWidth = 2; // due to pen width 1 in drawButtonBevel, on each side
    return QSize(qMax(80, contentsSize.width() + 2 * margin + frameWidth), qMin(40, contentsSize.height() + 2 * margin + frameWidth));
}

void PushButtonStyleHelper::adjustTextPalette(QStyleOptionButton *option, const QWidget *widget) const
{
    QColor textColor;
    OCC::ButtonStyle& style = ButtonStyleStrategy::getButtonStyle(widget, option);

    // Disabled
    if (!(option->state & QStyle::State_Enabled)) {
        textColor = style.buttonDisabledFontColor();
    }
    else
    {
        textColor = style.buttonFontColor();
    }
    option->palette.setColor(QPalette::ButtonText, textColor);
}