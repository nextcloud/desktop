/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"

#include "folderman.h"
#include "accountstate.h"
#include "theme.h"
#include "account.h"
#include "guiutility.h"

#include "resources/resources.h"

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>

namespace {
const int barHeightC = 7;
}

namespace OCC {

FolderStatusDelegate::FolderStatusDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

// allocate each item size in listview.
QSize FolderStatusDelegate::sizeHint(const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    const_cast<FolderStatusDelegate *>(this)->updateFont(option.font);
    QFontMetricsF fm(_font);

    const auto classif = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>();
    if (classif != FolderStatusModel::RootFolder) {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    // calc height
    qreal h = rootFolderHeightWithoutErrors() + _margin;
    // this already includes the bottom margin

    // add some space for the message boxes.
    for (auto column : { FolderStatusModel::Columns::FolderConflictMsg, FolderStatusModel::Columns::FolderErrorMsg, FolderStatusModel::Columns::FolderInfoMsg }) {
        auto msgs = index.siblingAtColumn(static_cast<int>(column)).data().toStringList();
        if (!msgs.isEmpty()) {
            h += _margin + 2 * _margin + msgs.count() * fm.height();
        }
    }

    return QSize(0, h);
}

qreal FolderStatusDelegate::rootFolderHeightWithoutErrors() const
{
    if (!_ready) {
        return {};
    }
    const QFontMetricsF fm(_font);
    const QFontMetricsF aliasFm(_aliasFont);
    qreal h = _aliasMargin; // margin to top
    h += aliasFm.height(); // alias
    h += _margin; // between alias and local path
    h += fm.height(); // sync text

    // quota or progress bar
    h += _margin;
    h += fm.height(); // quota or progress bar
    h += _margin;
    h += fm.height(); // possible progress string
    return h;
}

void FolderStatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    if (index.column() != 0) {
        return;
    }

    if (index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>()
        != FolderStatusModel::RootFolder) {
        return QStyledItemDelegate::paint(painter, option, index);
    }
    const_cast<FolderStatusDelegate *>(this)->updateFont(option.font);
    const auto textAlign = Qt::AlignLeft;

    const QFont errorFont = _font;
    const QFont progressFont = [progressFont = _font]() mutable {
        progressFont.setPointSize(progressFont.pointSize() - 2);
        return progressFont;
    }();

    const QFontMetricsF subFm(_font);
    const QFontMetricsF aliasFm(_aliasFont);

    painter->save();

    const QString statusIconName = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderStatusIconRole)).data().toString();
    const QString aliasText = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::HeaderRole)).data());
    const QStringList conflictTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderConflictMsg)).data());
    const QStringList errorTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderErrorMsg)).data());
    const QStringList infoTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderInfoMsg)).data());
    const QIcon spaceImage = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderImage)).data().value<QIcon>();

    const int overallPercent = qvariant_cast<int>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressOverallPercent)).data());
    const QString overallString = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressOverallString)).data());
    const QString itemString = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressItemString)).data());
    const int warningCount = qvariant_cast<int>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::WarningCount)).data());
    const bool syncOngoing = qvariant_cast<bool>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncRunning)).data());

    const QString syncText = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderSyncText)).data());
    const bool showProgess = !overallString.isEmpty() || !itemString.isEmpty();

    const auto iconState =
        index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderAccountConnected)).data().toBool() ? QIcon::Normal : QIcon::Disabled;

    const auto statusRect = QRectF{option.rect}.adjusted(0, 0, 0, rootFolderHeightWithoutErrors() - option.rect.height());
    const auto iconRect =
        QRectF{statusRect.topLeft(), QSizeF{statusRect.height(), statusRect.height()}}.marginsRemoved({_aliasMargin, _aliasMargin, _aliasMargin, _aliasMargin});

    // the rectangle next to the icon which will contain the strings
    const auto infoRect = QRectF{iconRect.topRight(), QSizeF{statusRect.width() - iconRect.width(), iconRect.height()}}.marginsRemoved({_aliasMargin, 0, 0, 0});
    const auto aliasRect = QRectF{infoRect.topLeft(), QSizeF{infoRect.width(), aliasFm.height()}};

    const auto marginOffset = QPointF{0, _margin};
    const auto localPathRect = QRectF{aliasRect.bottomLeft() + marginOffset, QSizeF{aliasRect.width(), subFm.height()}};
    const auto quotaTextRect = QRectF{localPathRect.bottomLeft() + marginOffset, QSizeF{aliasRect.width(), subFm.height()}};

    const auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    {
        const auto iconVisualRect = QStyle::visualRect(option.direction, option.rect, iconRect.toRect());
        spaceImage.paint(painter, iconVisualRect, Qt::AlignCenter, iconState);
        Theme::instance()->themeIcon(QStringLiteral("states/%1").arg(statusIconName)).paint(painter, iconVisualRect, Qt::AlignCenter, iconState);
    }

    // only show the warning icon if the sync is running. Otherwise its
    // encoded in the status icon.
    if (warningCount > 0 && syncOngoing) {
        Resources::getCoreIcon(QStringLiteral("warning"))
            .paint(painter, QStyle::visualRect(option.direction, option.rect, QRectF{iconRect.bottomLeft() - QPointF(0, 17), QSizeF{16, 16}}.toRect()),
                Qt::AlignCenter, iconState);
    }

    auto palette = option.palette;

    if (qApp->style()->inherits("QWindowsVistaStyle")) {
        // Hack: Windows Vista's light blue is not contrasting enough for white

        // (code from QWindowsVistaStyle::drawControl for CE_ItemViewItem)
        palette.setColor(QPalette::All, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::Text));
        palette.setColor(QPalette::All, QPalette::Highlight, palette.base().color().darker(108));
    }


    QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
        ? QPalette::Normal
        : QPalette::Disabled;
    if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;

    if (option.state & QStyle::State_Selected) {
        painter->setPen(palette.color(cg, QPalette::HighlightedText));
    } else {
        painter->setPen(palette.color(cg, QPalette::Text));
    }
    painter->setFont(_aliasFont);
    painter->drawText(
        QStyle::visualRect(option.direction, option.rect, aliasRect.toRect()), textAlign, aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width()));

    painter->setFont(_font);
    painter->drawText(QStyle::visualRect(option.direction, option.rect, localPathRect.toRect()), textAlign,
        subFm.elidedText(syncText, Qt::ElideRight, localPathRect.width()));

    if (!showProgess) {
        const auto totalQuota = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::QuotaTotal)).data().value<int64_t>();
        // only draw a bar if we have a quota set
        if (totalQuota > 0) {
            const auto usedQuota = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::QuotaUsed)).data().value<int64_t>();
            painter->setFont(_font);
            painter->drawText(QStyle::visualRect(option.direction, option.rect, quotaTextRect.toRect()),
                subFm.elidedText(
                    tr("%1 of %2 in use").arg(Utility::octetsToString(usedQuota), Utility::octetsToString(totalQuota)), Qt::ElideRight, quotaTextRect.width()));
        }
    } else {
        painter->save();

        const auto pogressRect = quotaTextRect.marginsAdded({0, 0, 0, barHeightC + _margin + subFm.height()});
        // Overall Progress Bar.
        const auto pBRect = QRectF{pogressRect.topLeft(), QSizeF{pogressRect.width() - 2 * _margin, barHeightC}};

        QStyleOptionProgressBar pBarOpt;

        pBarOpt.state = option.state | QStyle::State_Horizontal;
        pBarOpt.minimum = 0;
        pBarOpt.maximum = 100;
        pBarOpt.progress = overallPercent;
        pBarOpt.orientation = Qt::Horizontal;
        pBarOpt.rect = QStyle::visualRect(option.direction, option.rect, pBRect.toRect());
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pBarOpt, painter, option.widget);

        // Overall Progress Text
        const QRectF overallProgressRect = {pBRect.bottomLeft() + marginOffset, QSizeF{pogressRect.width(), subFm.height()}};
        painter->setFont(progressFont);

        painter->drawText(QStyle::visualRect(option.direction, option.rect, overallProgressRect.toRect()), Qt::AlignLeft | Qt::AlignVCenter, overallString);

        painter->restore();
    }

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&, pos = option.rect.top() + rootFolderHeightWithoutErrors() + _margin](const QStringList &texts, QColor color) mutable {
        QRectF rect = quotaTextRect;
        rect.setLeft(iconRect.left());
        rect.setTop(pos);
        rect.setHeight(texts.count() * subFm.height() + 2 * _margin);
        rect.setRight(option.rect.right() - _margin);

        painter->save();
        painter->setBrush(color);
        painter->setPen(QColor(0xaa, 0xaa, 0xaa));
        painter->drawRoundedRect(QStyle::visualRect(option.direction, option.rect, rect.toRect()), 4, 4);
        painter->setPen(Qt::white);
        painter->setFont(errorFont);
        QRect textRect(rect.left() + _margin, rect.top() + _margin, rect.width() - 2 * _margin, subFm.height());

        for (const auto &eText : texts) {
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign, subFm.elidedText(eText, Qt::ElideLeft, textRect.width()));
            textRect.translate(0, textRect.height());
        }
        painter->restore();

        pos = rect.bottom() + _margin;
    };

    if (!conflictTexts.isEmpty())
        drawTextBox(conflictTexts, QColor(0xba, 0xba, 0x4d));
    if (!errorTexts.isEmpty())
        drawTextBox(errorTexts, QColor(0xbb, 0x4d, 0x4d));
    if (!infoTexts.isEmpty())
        drawTextBox(infoTexts, QColor(0x4d, 0x4d, 0xba));


    {
        // was saved before we fetched the data from the model
        painter->restore();
        QStyleOptionToolButton btnOpt;
        btnOpt.state = option.state;
        btnOpt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
        btnOpt.state |= QStyle::State_Raised;
        btnOpt.arrowType = Qt::NoArrow;
        btnOpt.subControls = QStyle::SC_ToolButton;
        btnOpt.rect = optionsButtonVisualRect.toRect();
        btnOpt.icon = Resources::getCoreIcon(QStringLiteral("more"));
        int e = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
        btnOpt.iconSize = QSize(e,e);
        QApplication::style()->drawComplexControl(QStyle::CC_ToolButton, &btnOpt, painter);
    }
}

QRectF FolderStatusDelegate::optionsButtonRect(QRectF within, Qt::LayoutDirection direction) const
{
    if (!_ready) {
        return {};
    }
    within.setHeight(FolderStatusDelegate::rootFolderHeightWithoutErrors());

    QStyleOptionToolButton opt;
    int e = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
    opt.rect.setSize(QSize(e,e));
    QSize size = QApplication::style()->sizeFromContents(QStyle::CT_ToolButton, &opt, opt.rect.size()).expandedTo(QApplication::globalStrut());

    qreal margin = QApplication::style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    QRectF r(QPoint(within.right() - size.width() - margin, within.top() + within.height() / 2 - size.height() / 2), size);
    return QStyle::visualRect(direction, within.toRect(), r.toRect());
}

QRectF FolderStatusDelegate::errorsListRect(QRectF within, const QModelIndex &index) const
{
    if (!_ready) {
        return {};
    }
    const QFontMetrics fm(_font);
    within.setTop(within.top() + FolderStatusDelegate::rootFolderHeightWithoutErrors() + _margin);
    qreal h = 0;
    for (auto column : { FolderStatusModel::Columns::FolderConflictMsg, FolderStatusModel::Columns::FolderErrorMsg }) {
        const auto msgs = index.siblingAtColumn(static_cast<int>(column)).data().toStringList();
        if (!msgs.isEmpty()) {
            h += _margin + 2 * _margin + msgs.count() * fm.height() + _margin;
        }
    }
    within.setHeight(h);
    return within;
}

void FolderStatusDelegate::updateFont(const QFont &font)
{
    if (!_ready || _font != font) {
        _ready = true;
        _aliasFont = [&]() {
            auto aliasFont = font;
            aliasFont.setBold(true);
            aliasFont.setPointSizeF(font.pointSizeF() + 2);
            return aliasFont;
        }();

        _margin = QFontMetricsF(_font).height() / 4.0;
        _aliasMargin = QFontMetricsF(_aliasFont).height() / 2.0;
    }
}


} // namespace OCC
