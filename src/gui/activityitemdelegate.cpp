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
int ActivityItemDelegate::_buttonWidth = 0;
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

        _margin = fm.height() / 4;
    }
    return iconHeight() + 2 * _margin;
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
//  QIcon userIcon = qvariant_cast<QIcon>(index.data(UserIconRole));
//  QString pathText = qvariant_cast<QString>(index.data(PathRole));
//  QString remoteLink = qvariant_cast<QString>(index.data(LinkRole));
//  userIconRect.setLeft(actionIconRect.right() + margin);
//  userIconRect.setWidth(iconWidth);
//  userIconRect.setHeight(iconHeight);
//  userIconRect.setTop(actionIconRect.top());
//  QRect userIconRect = option.rect;
//  pm = userIcon.pixmap(iconWidth, iconHeight, QIcon::Normal);
//  painter->drawPixmap(QPoint(userIconRect.left(), userIconRect.top()), pm);

    QStyledItemDelegate::paint(painter, option, index);
    QFont font = option.font;
    QFontMetrics fm(font);
    int margin = fm.height() / 4;
    painter->save();

    QIcon actionIcon = qvariant_cast<QIcon>(index.data(ActionIconRole));
    Activity::Type activityType = qvariant_cast<Activity::Type>(index.data(ActionRole));
    QString actionText = qvariant_cast<QString>(index.data(ActionTextRole));
    QString messageText = qvariant_cast<QString>(index.data(MessageRole));
    QString timeText = qvariant_cast<QString>(index.data(PointInTimeRole));
    QString accountRole = qvariant_cast<QString>(index.data(AccountRole));
    bool accountOnline = qvariant_cast<bool>(index.data(AccountConnectedRole));

    // activity/notification icons
    QRect actionIconRect = option.rect;
    int iconHeight = qRound(fm.height() / 5.0 * 8.0);
    int iconWidth = iconHeight;
    actionIconRect.setLeft(option.rect.left() + margin);
    actionIconRect.setWidth(iconWidth);
    actionIconRect.setHeight(iconHeight);
    actionIconRect.setTop(actionIconRect.top() + margin);

    // time rect
    QRect timeBox;
    int atPos = accountRole.indexOf(QLatin1Char('@'));
    if (atPos > -1) {
        accountRole.remove(0, atPos + 1);
    }
    QString timeStr = tr("%1 on %2").arg(timeText, accountRole);
    if (!accountOnline) timeStr = tr("%1 on %2 (disconnected)").arg(timeText, accountRole);
    int textTopOffset = qRound((iconHeight - fm.height()) / 2.0);
    int timeBoxWidth = fm.width(timeStr);
    timeBox.setTop(actionIconRect.top() + textTopOffset);
    timeBox.setLeft(option.rect.right() - timeBoxWidth - margin);
    timeBox.setRight(option.rect.right() + margin);
    timeBox.setHeight(fm.height());
    _timeWidth = timeBox.width();

    // subject text rect
    QRect actionTextBox = timeBox;
    int actionTextBoxWidth = fm.width(actionText);
    actionTextBox.setLeft(actionIconRect.right() + margin);
    actionTextBox.setRight(actionTextBox.left() + actionTextBoxWidth + margin);

    // message text rect
    QRect messageTextBox = timeBox;
    if(!messageText.isEmpty()){
        int messageTextBoxWidth = fm.width(messageText.prepend(" - "));
        messageTextBox.setLeft(actionTextBox.right() - margin);
        messageTextBox.setRight(messageTextBox.left() + messageTextBoxWidth);
    } else {
        messageTextBox.setWidth(0);
    }

    QStyleOptionButton optionsButton;
    if(activityType == Activity::Type::NotificationType){
        int rightMargin = margin * 2;
        int leftMargin = margin * 4;
        int right = timeBox.left() - rightMargin;
        int left = timeBox.left() - leftMargin;
        optionsButton.rect = option.rect;
        optionsButton.text = "...";
        int btnTextWidth = fm.width(optionsButton.text.toAscii());
        optionsButton.rect.setLeft(left - btnTextWidth);
        optionsButton.rect.setRight(right);
        optionsButton.rect.setTop(option.rect.top() + 5);
        optionsButton.rect.setHeight(option.rect.height() - 10);
        optionsButton.features |= QStyleOptionButton::DefaultButton;
        optionsButton.state |= QStyle::State_Raised;
        _buttonWidth = optionsButton.rect.size().width();
        _buttonHeight = optionsButton.rect.height();
    }

    /* === start drawing === */
    QPixmap pm = actionIcon.pixmap(iconWidth, iconHeight, QIcon::Normal);
    painter->drawPixmap(QPoint(actionIconRect.left(), actionIconRect.top()), pm);

    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    if (option.state & QStyle::State_Selected) {
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(option.palette.color(cg, QPalette::Text));
    }

    int spaceLeftForText = (option.rect.width() - (actionIconRect.width() + _buttonWidth))/4;

    const QString elidedAction = fm.elidedText(actionText, Qt::ElideRight, spaceLeftForText);
    painter->drawText(actionTextBox, elidedAction);

    if(!messageText.isEmpty()){
        const QString elidedMessage = fm.elidedText(messageText, Qt::ElideRight, spaceLeftForText);
        painter->drawText(messageTextBox, elidedMessage);
    }

    if(activityType == Activity::Type::NotificationType)
        QApplication::style()->drawControl(QStyle::CE_PushButton, &optionsButton, painter);

    if (!accountOnline) {
        QPalette p = option.palette;
        painter->setPen(p.color(QPalette::Disabled, QPalette::Text));
    }

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
            int x = option.rect.left() + option.rect.width() - _buttonWidth - _timeWidth;
            int y = option.rect.top();
            if (mouseEvent->x() > x && mouseEvent->x() < (x + _buttonWidth)){
                if(mouseEvent->y() > y && mouseEvent->y() < (y + _buttonHeight))
                    emit buttonClickedOnItemView(index);
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace OCC
