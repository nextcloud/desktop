/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
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

#include "activityitemdelegate.h"
#include "folderstatusmodel.h"
#include "folderman.h"
#include "accountstate.h"
#include "activitydata.h"
#include <theme.h>
#include <account.h>

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>

namespace OCC {

int ActivityItemDelegate::_iconHeight = 0;
int ActivityItemDelegate::_margin = 0;
int ActivityItemDelegate::_primaryButtonWidth = 0;
int ActivityItemDelegate::_secondaryButtonWidth = 0;
int ActivityItemDelegate::_spaceBetweenButtons = 0;
int ActivityItemDelegate::_timeWidth = 0;
int ActivityItemDelegate::_buttonHeight = 0;

int ActivityItemDelegate::iconHeight()
{
    if (_iconHeight == 0) {
        QStyleOptionViewItem option;
        QFont font = option.font;

        QFontMetrics fm(font);

        _iconHeight = qRound(fm.height() / 5.0 * 8.0);
    }
    return _iconHeight;
}

int ActivityItemDelegate::rowHeight()
{
    if (_margin == 0) {
        QStyleOptionViewItem opt;

        QFont f = opt.font;
        QFontMetrics fm(f);

        _margin = fm.height() / 2;
    }
    return iconHeight() + 5 * _margin;
}

QSize ActivityItemDelegate::sizeHint(const QStyleOptionViewItem &option,
    const QModelIndex & /* index */) const
{
    QFont font = option.font;

    return QSize(0, rowHeight());
}

void ActivityItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);
    QFont font = option.font;
    QFontMetrics fm(font);
    int margin = fm.height() / 2.5;
    painter->save();
    int iconSize = 16;
    int iconOffset = qRound(fm.height() / 4.0 * 7.0);
    int offset = 4;

    // get the data
    Activity::Type activityType = qvariant_cast<Activity::Type>(index.data(ActionRole));
    QIcon actionIcon = qvariant_cast<QIcon>(index.data(ActionIconRole));
    QString actionText = qvariant_cast<QString>(index.data(ActionTextRole));
    QString messageText = qvariant_cast<QString>(index.data(MessageRole));
    QList<QVariant> customList = index.data(ActionsLinksRole).toList();
    QString timeText = qvariant_cast<QString>(index.data(PointInTimeRole));
    bool accountOnline = qvariant_cast<bool>(index.data(AccountConnectedRole));

    // activity/notification icons
    QRect actionIconRect = option.rect;
    actionIconRect.setLeft(option.rect.left() + iconOffset/3);
    actionIconRect.setRight(option.rect.left() + iconOffset);
    actionIconRect.setTop(option.rect.top() + qRound((option.rect.height() - 16)/3.0));

    // subject text rect
    QRect actionTextBox = actionIconRect;
    int actionTextBoxWidth = fm.width(actionText);
    actionTextBox.setTop(option.rect.top() + margin + offset/2);
    actionTextBox.setHeight(fm.height());
    actionTextBox.setLeft(actionIconRect.right() + margin);
    actionTextBox.setRight(actionTextBox.left() + actionTextBoxWidth + margin);

    // message text rect
    QRect messageTextBox = actionTextBox;
    messageTextBox.setTop(option.rect.top() + fm.height() + margin);
    messageTextBox.setHeight(actionTextBox.height());
    messageTextBox.setBottom(messageTextBox.top() + fm.height());
    if(messageText.isEmpty()){
        messageTextBox.setHeight(0);
        messageTextBox.setBottom(messageTextBox.top());
    }

    // time box rect
    QRect timeBox = messageTextBox;
    QString timeStr = tr("%1").arg(timeText);
    timeBox.setTop(option.rect.top() + fm.height() + fm.height() + margin + offset/2);
    timeBox.setHeight(actionTextBox.height());
    timeBox.setBottom(timeBox.top() + fm.height());

    // buttons
    QStyleOptionButton primaryButton;
    QStyleOptionButton secondaryButton;
    if(activityType == Activity::Type::NotificationType){
        int rightMargin = margin;
        int leftMargin = margin * offset;
        int top = option.rect.top() + margin - offset;
        int buttonSize = option.rect.height()/2.5;

        // Secondary will be 'Dismiss' or '...'
        secondaryButton.rect = option.rect;
        secondaryButton.icon = QIcon(QLatin1String(":/client/resources/close.svg"));
        if(customList.size() > 1)
            secondaryButton.icon = QIcon(QLatin1String(":/client/resources/more.png"));

        int right = option.rect.right() - rightMargin;
        int left = right - buttonSize;
        secondaryButton.iconSize = QSize(buttonSize, buttonSize);
        secondaryButton.rect.setLeft(left);
        secondaryButton.rect.setRight(right);
        secondaryButton.rect.setTop(top);
        secondaryButton.rect.setHeight(_buttonHeight);
        secondaryButton.features |= QStyleOptionButton::DefaultButton;
        secondaryButton.state |= QStyle::State_Raised;

        // Primary button will be 'More Information'
        primaryButton.rect = option.rect;
        primaryButton.text = tr("More information");
        right = secondaryButton.rect.left() - rightMargin;
        left = secondaryButton.rect.left() - leftMargin;
        primaryButton.rect.setLeft(left - fm.width(primaryButton.text));
        primaryButton.rect.setRight(right);
        primaryButton.rect.setTop(top);
        primaryButton.rect.setHeight(_buttonHeight);
        primaryButton.features |= QStyleOptionButton::DefaultButton;
        primaryButton.state |= QStyle::State_Raised;

        // save info to be able to filter mouse clicks
        _buttonHeight = buttonSize;
        _spaceBetweenButtons = leftMargin;
        _primaryButtonWidth = primaryButton.rect.size().width();
        _secondaryButtonWidth = secondaryButton.rect.size().width();
    } else {
        _spaceBetweenButtons = margin * offset;
        _primaryButtonWidth = 0;
        _secondaryButtonWidth = 0;
    }

    // draw the icon
    QPixmap pm = actionIcon.pixmap(iconSize, iconSize, QIcon::Normal);
    painter->drawPixmap(QPoint(actionIconRect.left(), actionIconRect.top()), pm);

    // change pen color if use is not online
    QPalette p = option.palette;
    if(!accountOnline)
        painter->setPen(p.color(QPalette::Disabled, QPalette::Text));

    // change pen color if the line is selected
    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;

    if (option.state & QStyle::State_Selected)
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    else
        painter->setPen(option.palette.color(cg, QPalette::Text));

    // calculate space for text - use the max possible before using the elipses
    int spaceLeftForText = option.rect.width() - (actionIconRect.width() + margin) -
                             (_primaryButtonWidth + _secondaryButtonWidth + _spaceBetweenButtons);

    // draw the subject
    const QString elidedAction = fm.elidedText(actionText, Qt::ElideRight, spaceLeftForText);
    painter->drawText(actionTextBox, elidedAction);

    // draw the buttons
    if(activityType == Activity::Type::NotificationType){
        QApplication::style()->drawControl(QStyle::CE_PushButton, &primaryButton, painter);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &secondaryButton, painter);
    }

    // draw the message
    // change pen color for the message
    if(!messageText.isEmpty()){
        painter->setPen(p.color(QPalette::Disabled, QPalette::Text));

        // check if line is selected
        if (option.state & QStyle::State_Selected)
            painter->setPen(option.palette.color(cg, QPalette::HighlightedText));

        const QString elidedMessage = fm.elidedText(messageText, Qt::ElideRight, spaceLeftForText);
        painter->drawText(messageTextBox, elidedMessage);
    }

    // change pen color for the time
    painter->setPen(p.color(QPalette::Disabled, QPalette::Highlight));

    // check if line is selected
    if (option.state & QStyle::State_Selected)
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));

    // draw the time
    const QString elidedTime = fm.elidedText(timeStr, Qt::ElideRight, spaceLeftForText);
    painter->drawText(timeBox, elidedTime);

    painter->restore();
}

bool ActivityItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if(qvariant_cast<Activity::Type>(index.data(ActionRole)) == Activity::Type::NotificationType){
        if (event->type() == QEvent::MouseButtonRelease){
            QMouseEvent *mouseEvent = (QMouseEvent*)event;
            if(mouseEvent){
                int mouseEventX = mouseEvent->x();
                int mouseEventY = mouseEvent->y();
                int buttonsWidth = _primaryButtonWidth + _spaceBetweenButtons + _secondaryButtonWidth;
                int x = option.rect.left() + option.rect.width() - buttonsWidth - _timeWidth;
                int y = option.rect.top();

                if (mouseEventX > x && mouseEventX < x + buttonsWidth){
                    if(mouseEventY > y && mouseEventY < y + _buttonHeight){
                        if (mouseEventX > x && mouseEventX < x + _primaryButtonWidth)
                            emit primaryButtonClickedOnItemView(index);

                        x += _primaryButtonWidth + _spaceBetweenButtons;
                        if (mouseEventX > x && mouseEventX < x + _secondaryButtonWidth)
                            emit secondaryButtonClickedOnItemView(index);
                    }
                }
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace OCC
