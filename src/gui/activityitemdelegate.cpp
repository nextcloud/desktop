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
#include <theme.h>
#include <account.h>

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>

namespace OCC {

int ActivityItemDelegate::_iconHeight = 0;
int ActivityItemDelegate::_margin = 0;

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
    QStyledItemDelegate::paint(painter, option, index);

    QFont font = option.font;

    QFontMetrics fm(font);
    int margin = fm.height() / 4;

    painter->save();

    QIcon actionIcon = qvariant_cast<QIcon>(index.data(ActionIconRole));
    QIcon userIcon = qvariant_cast<QIcon>(index.data(UserIconRole));
    QString actionText = qvariant_cast<QString>(index.data(ActionTextRole));
    QString pathText = qvariant_cast<QString>(index.data(PathRole));

    QString remoteLink = qvariant_cast<QString>(index.data(LinkRole));
    QString timeText = qvariant_cast<QString>(index.data(PointInTimeRole));
    QString accountRole = qvariant_cast<QString>(index.data(AccountRole));
    bool accountOnline = qvariant_cast<bool>(index.data(AccountConnectedRole));

    QRect actionIconRect = option.rect;
    QRect userIconRect = option.rect;

    int iconHeight = qRound(fm.height() / 5.0 * 8.0);
    int iconWidth = iconHeight;

    actionIconRect.setLeft(option.rect.left() + margin);
    actionIconRect.setWidth(iconWidth);
    actionIconRect.setHeight(iconHeight);
    actionIconRect.setTop(actionIconRect.top() + margin);
    userIconRect.setLeft(actionIconRect.right() + margin);
    userIconRect.setWidth(iconWidth);
    userIconRect.setHeight(iconHeight);
    userIconRect.setTop(actionIconRect.top());

    int textTopOffset = qRound((iconHeight - fm.height()) / 2.0);
    // time rect
    QRect timeBox;
    int timeBoxWidth = fm.boundingRect(QLatin1String("4 hour(s) ago on longlongdomain.org")).width(); // FIXME.
    timeBox.setTop(actionIconRect.top() + textTopOffset);
    timeBox.setLeft(option.rect.right() - timeBoxWidth - margin);
    timeBox.setWidth(timeBoxWidth);
    timeBox.setHeight(fm.height());

    QRect actionTextBox = timeBox;
    actionTextBox.setLeft(userIconRect.right() + margin);
    actionTextBox.setRight(timeBox.left() - margin);

    /* === start drawing === */
    QPixmap pm = actionIcon.pixmap(iconWidth, iconHeight, QIcon::Normal);
    painter->drawPixmap(QPoint(actionIconRect.left(), actionIconRect.top()), pm);

    pm = userIcon.pixmap(iconWidth, iconHeight, QIcon::Normal);
    painter->drawPixmap(QPoint(userIconRect.left(), userIconRect.top()), pm);

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

    const QString elidedAction = fm.elidedText(actionText, Qt::ElideRight, actionTextBox.width());
    painter->drawText(actionTextBox, elidedAction);

    int atPos = accountRole.lastIndexOf(QLatin1Char('@'));
    if (atPos > -1) {
        accountRole.remove(0, atPos + 1);
    }

    QString timeStr;
    if (accountOnline) {
        timeStr = tr("%1 on %2").arg(timeText, accountRole);
    } else {
        timeStr = tr("%1 on %2 (disconnected)").arg(timeText, accountRole);
        QPalette p = option.palette;
        painter->setPen(p.color(QPalette::Disabled, QPalette::Text));
    }
    const QString elidedTime = fm.elidedText(timeStr, Qt::ElideRight, timeBox.width());

    painter->drawText(timeBox, elidedTime);
    painter->restore();
}

bool ActivityItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace OCC
