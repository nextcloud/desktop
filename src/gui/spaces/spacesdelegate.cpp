/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "spacesdelegate.h"

#include "gui/guiutility.h"
#include "spacesmodel.h"

#include <QApplication>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QPainter>
#include <QUrl>

using namespace OCC::Spaces;

void SpacesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *style = option.widget->style();
    if (index.column() == static_cast<int>(SpacesModel::Columns::Sync)) {
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        opt.rect.setSize(sizeHint(option, index));
        opt.rect.moveCenter(option.rect.center());
        opt.rect = QStyle::visualRect(option.direction, option.rect, opt.rect);

        if (opt.state & QStyle::State_Selected) {
            opt.state |= QStyle::State_On;
        }
        style->drawPrimitive(QStyle::PE_IndicatorRadioButton, &opt, painter, option.widget);
    } else if (index.column() == static_cast<int>(SpacesModel::Columns::WebUrl)) {
        // only display the button if we have a valid url
        if (index.data().toUrl().isValid()) {
            auto opt = openBrowserButtonRect(option);
            opt.state &= ~QStyle::State_Selected;
            opt.state |= QStyle::State_Raised;
            opt.rect = QStyle::visualRect(option.direction, option.rect, opt.rect);
            style->drawControl(QStyle::CE_PushButton, &opt, painter, option.widget);
        }
    } else if (index.column() == static_cast<int>(SpacesModel::Columns::Name)) {
        const QString subTitle =
            option.fontMetrics.elidedText(index.siblingAtColumn(static_cast<int>(SpacesModel::Columns::Subtitle)).data(Qt::DisplayRole).toString(), Qt::ElideRight, option.rect.width());
        const int titleTextFlags = Qt::AlignVCenter | Qt::TextWordWrap;
        const int subTitleTextFlags = Qt::AlignTop;
        QRect titleRect;
        QRect subtitleRect;
        if (!subTitle.isEmpty()) {
            subtitleRect = option.fontMetrics.boundingRect(option.rect, subTitleTextFlags, subTitle);
            subtitleRect = QStyle::visualRect(option.direction, option.rect, subtitleRect);
        }
        {
            painter->save();
            const QString title = index.data(Qt::DisplayRole).toString();
            auto font = option.font;
            font.setBold(true);
            font.setPointSizeF(font.pointSizeF() * 1.2);
            painter->setFont(font);
            const QFontMetrics fontMetric(font);
            const QString elidedTitle = fontMetric.elidedText(title, Qt::ElideRight, option.rect.width() * 2); // allow about two lines of title
            titleRect = fontMetric.boundingRect(option.rect, titleTextFlags, elidedTitle);
            if (subtitleRect.isValid()) {
                titleRect.moveTop(titleRect.top() - subtitleRect.height() / 2);
            }
            titleRect = QStyle::visualRect(option.direction, option.rect, titleRect);
            painter->drawText(titleRect, titleTextFlags, elidedTitle);
            painter->restore();
        }
        if (titleRect.isValid()) {
            subtitleRect.moveTop(titleRect.bottom());
            painter->drawText(QStyle::visualRect(option.direction, option.rect, subtitleRect), subTitleTextFlags, subTitle);
        }
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize SpacesDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == static_cast<int>(SpacesModel::Columns::WebUrl)) {
        auto opt = openBrowserButtonRect(option);
        return opt.rect.size();
    }
    return QStyledItemDelegate::sizeHint(option, index);
}

bool SpacesDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (index.column() == static_cast<int>(SpacesModel::Columns::WebUrl)) {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto opt = openBrowserButtonRect(option);
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (opt.rect.contains(mouseEvent->localPos().toPoint())) {
                const auto url = index.data().toUrl();
                if (url.isValid()) {
                    QDesktopServices::openUrl(index.data().toUrl());
                }
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QStyleOptionButton SpacesDelegate::openBrowserButtonRect(const QStyleOptionViewItem &option)
{
    QStyleOptionButton opt;
    static_cast<QStyleOption &>(opt) = option;
    opt.text = tr("Open in Web");
    opt.icon = Utility::getCoreIcon(QStringLiteral("arrow-up-right-from-square"));
    const auto px = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
    opt.iconSize = QSize { px, px };
    opt.rect.setSize(QApplication::style()->sizeFromContents(
        QStyle::CT_PushButton, &opt, opt.fontMetrics.size(Qt::TextSingleLine, opt.text)));
    opt.rect.moveCenter(option.rect.center());
    return opt;
}
