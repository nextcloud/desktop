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

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>

namespace {
const int barHeightC = 7; // same height as quota bar

QFont makeAliasFont(const QFont &normalFont)
{
    QFont aliasFont = normalFont;
    aliasFont.setBold(true);
    aliasFont.setPointSize(normalFont.pointSize() + 2);
    return aliasFont;
}
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
    QFont aliasFont = makeAliasFont(option.font);
    QFont font = option.font;

    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);

    const auto classif = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::ItemType)).data().value<FolderStatusModel::ItemType>();
    if (classif != FolderStatusModel::RootFolder) {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    // calc height
    int h = rootFolderHeightWithoutErrors(fm, aliasFm);
    // this already includes the bottom margin

    // add some space for the message boxes.
    int margin = fm.height() / 4;
    for (auto column : { FolderStatusModel::Columns::FolderConflictMsg, FolderStatusModel::Columns::FolderErrorMsg, FolderStatusModel::Columns::FolderInfoMsg }) {
        auto msgs = index.siblingAtColumn(static_cast<int>(column)).data().toStringList();
        if (!msgs.isEmpty()) {
            h += margin + 2 * margin + msgs.count() * fm.height();
        }
    }

    return QSize(0, h);
}

int FolderStatusDelegate::rootFolderHeightWithoutErrors(const QFontMetrics &fm, const QFontMetrics &aliasFm)
{
    const int aliasMargin = aliasFm.height() / 2;
    const int margin = fm.height() / 4;

    int h = aliasMargin; // margin to top
    h += aliasFm.height(); // alias
    h += margin; // between alias and local path
    h += fm.height(); // sync text

    // quota or progress bar
    h += margin;
    h += fm.height(); // quota or progress bar
    h += margin;
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
    const auto textAlign = Qt::AlignLeft;

    // TODO: Port to float
    const QFont aliasFont = makeAliasFont(option.font);
    const QFont subFont = option.font;
    const QFont errorFont = subFont;
    const QFont progressFont = [progressFont = subFont]() mutable {
        progressFont.setPointSize(progressFont.pointSize() - 2);
        return progressFont;
    }();

    const QFontMetrics subFm(subFont);
    const QFontMetrics aliasFm(aliasFont);

    const int aliasMargin = aliasFm.height() / 2;
    const int margin = subFm.height() / 4;

    painter->save();

    const QIcon statusIcon = qvariant_cast<QIcon>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderStatusIconRole)).data());
    const QString aliasText = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::HeaderRole)).data());
    const QStringList conflictTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderConflictMsg)).data());
    const QStringList errorTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderErrorMsg)).data());
    const QStringList infoTexts = qvariant_cast<QStringList>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderInfoMsg)).data());

    const int overallPercent = qvariant_cast<int>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressOverallPercent)).data());
    const QString overallString = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressOverallString)).data());
    const QString itemString = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncProgressItemString)).data());
    const int warningCount = qvariant_cast<int>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::WarningCount)).data());
    const bool syncOngoing = qvariant_cast<bool>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::SyncRunning)).data());
    const bool syncEnabled = qvariant_cast<bool>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderAccountConnected)).data());
    const QString syncText = qvariant_cast<QString>(index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::FolderSyncText)).data());
    const bool showProgess = !overallString.isEmpty() || !itemString.isEmpty();

    const auto statusRect = option.rect.adjusted(0, 0, 0, rootFolderHeightWithoutErrors(subFm, aliasFm) - option.rect.height());
    const auto iconRect =
        QRect{statusRect.topLeft(), QSize{statusRect.height(), statusRect.height()}}.marginsRemoved({aliasMargin, aliasMargin, aliasMargin, 0});

    // the rectangle next to the icon which will contain the strings
    const auto infoRect = QRect{iconRect.topRight(), QSize{statusRect.width() - iconRect.width(), iconRect.height()}}.marginsRemoved({aliasMargin, 0, 0, 0});
    const auto aliasRect = QRect{infoRect.topLeft(), QSize{infoRect.width(), aliasFm.height()}};

    const auto marginOffset = QPoint{0, margin};
    const auto localPathRect = QRect{aliasRect.bottomLeft() + marginOffset, QSize{aliasRect.width(), subFm.height()}};
    const auto quotaTextRect = QRect{localPathRect.bottomLeft() + marginOffset, QSize{aliasRect.width(), subFm.height()}};

    const auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    painter->drawPixmap(QStyle::visualRect(option.direction, option.rect, iconRect).left(), iconRect.top(),
        statusIcon.pixmap(iconRect.width(), iconRect.width(), syncEnabled ? QIcon::Normal : QIcon::Disabled));

    // only show the warning icon if the sync is running. Otherwise its
    // encoded in the status icon.
    if (warningCount > 0 && syncOngoing) {
        const auto warnRect = QRect{iconRect.bottomLeft() - QPoint(0, 17), QSize{16, 16}};
        const auto warnIcon = Utility::getCoreIcon(QStringLiteral("warning"));
        const QPixmap pm = warnIcon.pixmap(warnRect.size(), syncEnabled ? QIcon::Normal : QIcon::Disabled);
        painter->drawPixmap(QStyle::visualRect(option.direction, option.rect, warnRect), pm);
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
    painter->setFont(aliasFont);
    painter->drawText(
        QStyle::visualRect(option.direction, option.rect, aliasRect), textAlign, aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width()));

    painter->setFont(subFont);
    painter->drawText(
        QStyle::visualRect(option.direction, option.rect, localPathRect), textAlign, subFm.elidedText(syncText, Qt::ElideRight, localPathRect.width()));

    if (!showProgess) {
        const auto totalQuota = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::QuotaTotal)).data().value<quint64>();
        // only draw a bar if we have a quota set
        if (totalQuota > 0) {
            const auto usedQuota = index.siblingAtColumn(static_cast<int>(FolderStatusModel::Columns::QuotaUsed)).data().value<quint64>();
            painter->setFont(subFont);
            painter->drawText(QStyle::visualRect(option.direction, option.rect, quotaTextRect),
                subFm.elidedText(
                    tr("%1 of %2 in use").arg(Utility::octetsToString(usedQuota), Utility::octetsToString(totalQuota)), Qt::ElideRight, quotaTextRect.width()));
        }
    } else {
        painter->save();

        const auto pogressRect = quotaTextRect.marginsAdded({0, 0, 0, barHeightC + margin + subFm.height()});
        // Overall Progress Bar.
        const QRect pBRect = {pogressRect.topLeft(), QSize{pogressRect.width() - 2 * margin, barHeightC}};

        QStyleOptionProgressBar pBarOpt;

        pBarOpt.state = option.state | QStyle::State_Horizontal;
        pBarOpt.minimum = 0;
        pBarOpt.maximum = 100;
        pBarOpt.progress = overallPercent;
        pBarOpt.orientation = Qt::Horizontal;
        pBarOpt.rect = QStyle::visualRect(option.direction, option.rect, pBRect);
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pBarOpt, painter, option.widget);

        // Overall Progress Text
        const QRect overallProgressRect = {pBRect.bottomLeft() + marginOffset, QSize{pogressRect.width(), subFm.height()}};
        painter->setFont(progressFont);

        painter->drawText(QStyle::visualRect(option.direction, option.rect, overallProgressRect), Qt::AlignLeft | Qt::AlignVCenter, overallString);

        painter->restore();
    }

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&, pos = option.rect.top() + rootFolderHeightWithoutErrors(subFm, aliasFm) + margin](const QStringList &texts, QColor color) mutable {
        QRect rect = quotaTextRect;
        rect.setLeft(iconRect.left());
        rect.setTop(pos);
        rect.setHeight(texts.count() * subFm.height() + 2 * margin);
        rect.setRight(option.rect.right() - margin);

        painter->save();
        painter->setBrush(color);
        painter->setPen(QColor(0xaa, 0xaa, 0xaa));
        painter->drawRoundedRect(QStyle::visualRect(option.direction, option.rect, rect),
            4, 4);
        painter->setPen(Qt::white);
        painter->setFont(errorFont);
        QRect textRect(rect.left() + margin,
            rect.top() + margin,
            rect.width() - 2 * margin,
            subFm.height());

        for (const auto &eText : texts) {
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign, subFm.elidedText(eText, Qt::ElideLeft, textRect.width()));
            textRect.translate(0, textRect.height());
        }
        painter->restore();

        pos = rect.bottom() + margin;
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
        btnOpt.rect = optionsButtonVisualRect;
        btnOpt.icon = Utility::getCoreIcon(QStringLiteral("more"));
        int e = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
        btnOpt.iconSize = QSize(e,e);
        QApplication::style()->drawComplexControl(QStyle::CC_ToolButton, &btnOpt, painter);
    }
}

QRect FolderStatusDelegate::optionsButtonRect(QRect within, Qt::LayoutDirection direction)
{
    QFont font = QFont();
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setHeight(FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));

    QStyleOptionToolButton opt;
    int e = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
    opt.rect.setSize(QSize(e,e));
    QSize size = QApplication::style()->sizeFromContents(QStyle::CT_ToolButton, &opt, opt.rect.size()).expandedTo(QApplication::globalStrut());

    int margin = QApplication::style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    QRect r(QPoint(within.right() - size.width() - margin,
                within.top() + within.height() / 2 - size.height() / 2),
        size);
    return QStyle::visualRect(direction, within, r);
}

QRect FolderStatusDelegate::errorsListRect(QRect within, const QModelIndex &index)
{
    QFont font = QFont();
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setTop(within.top() + FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));
    int margin = fm.height() / 4;
    int h = 0;
    for (auto column : { FolderStatusModel::Columns::FolderConflictMsg, FolderStatusModel::Columns::FolderErrorMsg }) {
        const auto msgs = index.siblingAtColumn(static_cast<int>(column)).data().toStringList();
        if (!msgs.isEmpty()) {
            h += margin + 2 * margin + msgs.count() * fm.height();
        }
    }
    within.setHeight(h);
    return within;
}


} // namespace OCC
