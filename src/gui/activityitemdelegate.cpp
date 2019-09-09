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
const QString ActivityItemDelegate::_remote_share("remote_share");
const QString ActivityItemDelegate::_call("call");

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

#if defined(Q_OS_WIN)
        _margin += 5;
#endif
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
    QString objectType = qvariant_cast<QString>(index.data(ObjectTypeRole));
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
    int messageTextWidth = fm.width(messageText);
    int messageTextTop = option.rect.top() + fm.height() + margin;
    if(actionText.isEmpty()) messageTextTop = option.rect.top() + margin + offset/2;
    messageTextBox.setTop(messageTextTop);
    messageTextBox.setHeight(fm.height());
    messageTextBox.setBottom(messageTextBox.top() + fm.height());
    messageTextBox.setRight(messageTextBox.left() + messageTextWidth + margin);
    if(messageText.isEmpty()){
        messageTextBox.setHeight(0);
        messageTextBox.setBottom(messageTextBox.top());
    }

    // time box rect
    QRect timeBox = messageTextBox;
    QString timeStr = tr("%1").arg(timeText);
    int timeTextWidth = fm.width(timeStr);
    int timeTop = option.rect.top() + fm.height() + fm.height() + margin + offset/2;
    if(messageText.isEmpty() || actionText.isEmpty())
        timeTop = option.rect.top() + fm.height() + margin;
    timeBox.setTop(timeTop);
    timeBox.setHeight(fm.height());
    timeBox.setBottom(timeBox.top() + fm.height());
    timeBox.setRight(timeBox.left() + timeTextWidth + margin);

    // buttons - default values
    int rightMargin = margin;
    int leftMargin = margin * offset;
    int top = option.rect.top() + margin;
    int buttonSize = option.rect.height()/2;
    int right = option.rect.right() - rightMargin;
    int left = right - buttonSize;

    QStyleOptionButton secondaryButton;
    secondaryButton.rect = option.rect;
    secondaryButton.features |= QStyleOptionButton::Flat;
    secondaryButton.state |= QStyle::State_None;
    secondaryButton.rect.setLeft(left);
    secondaryButton.rect.setRight(right);
    secondaryButton.rect.setTop(top + margin);
    secondaryButton.rect.setHeight(iconSize);

    QStyleOptionButton primaryButton;
    primaryButton.rect = option.rect;
    primaryButton.features |= QStyleOptionButton::DefaultButton;
    primaryButton.state |= QStyle::State_Raised;
    primaryButton.rect.setTop(top);
    primaryButton.rect.setHeight(buttonSize);

    right = secondaryButton.rect.left() - rightMargin;
    left = secondaryButton.rect.left() - leftMargin;

    primaryButton.rect.setRight(right);

    if(activityType == Activity::Type::NotificationType){

        // Secondary will be 'Dismiss' or '...' multiple options button
        secondaryButton.icon = QIcon(QLatin1String(":/client/resources/close.svg"));
        if(customList.size() > 1)
            secondaryButton.icon = QIcon(QLatin1String(":/client/resources/more.svg"));
        secondaryButton.iconSize = QSize(iconSize, iconSize);

        // Primary button will be 'More Information' or 'Accept'
        primaryButton.text = tr("More information");
        if(objectType == _remote_share) primaryButton.text = tr("Accept");
        if(objectType == _call) primaryButton.text = tr("Join");

        primaryButton.rect.setLeft(left - margin * 2 - fm.width(primaryButton.text));

        // save info to be able to filter mouse clicks
        _buttonHeight = buttonSize;
        _primaryButtonWidth = primaryButton.rect.size().width();
        _secondaryButtonWidth = secondaryButton.rect.size().width();
        _spaceBetweenButtons = secondaryButton.rect.left() - primaryButton.rect.right();

    } else if(activityType == Activity::SyncResultType){

        // Secondary will be 'open file manager' with the folder icon
        secondaryButton.icon = QIcon(QLatin1String(":/client/resources/folder.svg"));
        secondaryButton.iconSize = QSize(iconSize, iconSize);

        // Primary button will be 'open browser'
        primaryButton.text = tr("Open Browser");
        primaryButton.rect.setLeft(left - margin * 2 - fm.width(primaryButton.text));

        // save info to be able to filter mouse clicks
        _buttonHeight = buttonSize;
        _primaryButtonWidth = primaryButton.rect.size().width();
        _secondaryButtonWidth = secondaryButton.rect.size().width();
        _spaceBetweenButtons = secondaryButton.rect.left() - primaryButton.rect.right();

    } else if(activityType == Activity::SyncFileItemType){

        // Secondary will be 'open file manager' with the folder icon
        secondaryButton.icon = QIcon(QLatin1String(":/client/resources/folder.svg"));
        secondaryButton.iconSize = QSize(iconSize, iconSize);

        // No primary button on this case
        // Whatever error we have at this case it is local, there is no point on opening the browser
        _primaryButtonWidth = 0;
        _secondaryButtonWidth = secondaryButton.rect.size().width();
        _spaceBetweenButtons = secondaryButton.rect.left() - primaryButton.rect.right();

    } else {
        _spaceBetweenButtons = leftMargin;
        _primaryButtonWidth = 0;
        _secondaryButtonWidth = 0;
    }

    // draw the icon
    QPixmap pm = actionIcon.pixmap(iconSize, iconSize, QIcon::Normal);
    painter->drawPixmap(QPoint(actionIconRect.left(), actionIconRect.top()), pm);

    // change pen color if use is not online
    QPalette p = option.palette;
    if(!accountOnline)
        p.setCurrentColorGroup(QPalette::Disabled);

    // change pen color if the line is selected
    if (option.state & QStyle::State_Selected)
        painter->setPen(p.color(QPalette::HighlightedText));
    else
        painter->setPen(p.color(QPalette::Text));

    // calculate space for text - use the max possible before using the elipses
    int spaceLeftForText = option.rect.width() - (actionIconRect.width() + margin + rightMargin + leftMargin) -
                             (_primaryButtonWidth + _secondaryButtonWidth + _spaceBetweenButtons);

    // draw the subject
    const QString elidedAction = fm.elidedText(actionText, Qt::ElideRight, spaceLeftForText);
    painter->drawText(actionTextBox, elidedAction);

    // draw the buttons
    if(activityType == Activity::Type::NotificationType || activityType == Activity::Type::SyncResultType)
        QApplication::style()->drawControl(QStyle::CE_PushButton, &primaryButton, painter);

    // Since they are errors on local syncing, there is nothing to do in the server
    if(activityType != Activity::Type::ActivityType)
        QApplication::style()->drawControl(QStyle::CE_PushButton, &secondaryButton, painter);

    // draw the message
    // change pen color for the message
    if(!messageText.isEmpty()){
        const QString elidedMessage = fm.elidedText(messageText, Qt::ElideRight, spaceLeftForText);
        painter->drawText(messageTextBox, elidedMessage);
    }

    // change pen color for the time
    if (option.state & QStyle::State_Selected)
        painter->setPen(p.color(QPalette::Disabled, QPalette::HighlightedText));
    else
        painter->setPen(p.color(QPalette::Disabled, QPalette::Text));

    // draw the time
    const QString elidedTime = fm.elidedText(timeStr, Qt::ElideRight, spaceLeftForText);
    painter->drawText(timeBox, elidedTime);

    painter->restore();
}

bool ActivityItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    Activity::Type activityType = qvariant_cast<Activity::Type>(index.data(ActionRole));
    if(activityType != Activity::Type::ActivityType){
        if (event->type() == QEvent::MouseButtonRelease){
            QMouseEvent *mouseEvent = (QMouseEvent*)event;
            if(mouseEvent){
                int mouseEventX = mouseEvent->x();
                int mouseEventY = mouseEvent->y();
                int buttonsWidth = _primaryButtonWidth + _spaceBetweenButtons + _secondaryButtonWidth;
                int x = option.rect.left() + option.rect.width() - buttonsWidth - _timeWidth;
                int y = option.rect.top();

                // clickable area for ...
                if (mouseEventX > x && mouseEventX < x + buttonsWidth){
                    if(mouseEventY > y && mouseEventY < y + _buttonHeight){

                        // ...primary button ('more information' or 'accept' on notifications or 'open browser' on errors)
                        if (mouseEventX > x && mouseEventX < x + _primaryButtonWidth){
                            emit primaryButtonClickedOnItemView(index);

                        // ...secondary button ('dismiss' on notifications or 'open file manager' on errors)
                        } else  {
                            x += _primaryButtonWidth + _spaceBetweenButtons;
                            if (mouseEventX > x && mouseEventX < x + _secondaryButtonWidth)
                                emit secondaryButtonClickedOnItemView(index);
                        }
                    }
                }
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace OCC
