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
#include <QDebug>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QPainter>
#include <QUrl>

using namespace OCC::Spaces;

Q_LOGGING_CATEGORY(lcSpacesDelegate, "spaces.delegate")

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

        const int nameTextFlags = Qt::AlignVCenter | Qt::TextWordWrap;
        const int subTitleTextFlags = Qt::AlignTop;

        QRect nameRect;

        // default constructor makes this an "invalid" rectangle
        // only when a subtitle is available, we assign proper values to it
        QRect subtitleRect;

        if (!subTitle.isEmpty()) {
            subtitleRect = option.fontMetrics.boundingRect(option.rect, subTitleTextFlags, subTitle);
            subtitleRect = QStyle::visualRect(option.direction, option.rect, subtitleRect);
        }

        // draw title
        {
            painter->save();

            const QString name = index.data(Qt::DisplayRole).toString();

            // the title should be larger than the subtitle, so we scale up the font a bit
            // rendering it bold also makes it more visible
            auto nameFont = option.font;
            nameFont.setBold(true);
            nameFont.setPointSizeF(nameFont.pointSizeF() * 1.2);

            painter->setFont(nameFont);

            const QFontMetrics fontMetric(nameFont);

            // allow about two lines of title
            const QString elidedName = fontMetric.elidedText(name, Qt::ElideRight, option.rect.width() * 2);

            nameRect = fontMetric.boundingRect(option.rect, nameTextFlags, elidedName);

            // in case we have to draw a subtitle, we want to center the combination of both title and subtitle vertically
            // therefore, we have to move the title up by half the height of the subtitle
            if (subtitleRect.isValid()) {
                nameRect.moveTop(nameRect.top() - subtitleRect.height() / 2);
            }

            nameRect = QStyle::visualRect(option.direction, option.rect, nameRect);

            painter->drawText(nameRect, nameTextFlags, elidedName);

            painter->restore();
        }
        if (nameRect.isValid()) {
            subtitleRect.moveTop(nameRect.bottom());
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
        // handle click on button
        // TODO: simulate "click on button" visually, too (i.e., render button accordingly)
        if (event->type() == QEvent::MouseButtonRelease) {
            auto opt = openBrowserButtonRect(option);

            auto *mouseEvent = static_cast<QMouseEvent *>(event);

            // we need to make sure the mouse click is within the button's boundary box
            if (opt.rect.contains(mouseEvent->localPos().toPoint())) {
                const auto url = index.data().toUrl();

                // we only display the button when the URL is valid (see above)
                Q_ASSERT(url.isValid());

                // log when opening fails
                if (!QDesktopServices::openUrl(url)) {
                    qCWarning(lcSpacesDelegate) << "failed to open browser for URL" << url;
                }

                return true;
            }
        }
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

// the main reason to put it in the class is to get access to tr(...)
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
