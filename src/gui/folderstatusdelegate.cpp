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

inline static QFont makeAliasFont(const QFont &normalFont)
{
    QFont aliasFont = normalFont;
    aliasFont.setBold(true);
    aliasFont.setPointSize(normalFont.pointSize() + 2);
    return aliasFont;
}

namespace OCC {

FolderStatusDelegate::FolderStatusDelegate()
    : QStyledItemDelegate()
{
}

QString FolderStatusDelegate::addFolderText(bool useSpaces)
{
    return !useSpaces ? tr("Add Folder Sync Connection") : tr("Add a Space");
}

// allocate each item size in listview.
QSize FolderStatusDelegate::sizeHint(const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    QFont aliasFont = makeAliasFont(option.font);
    QFont font = option.font;

    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);

    auto classif = static_cast<const FolderStatusModel *>(index.model())->classify(index);
    if (classif == FolderStatusModel::AddButton) {
        const int margins = aliasFm.height(); // same as 2*aliasMargin of paint
        QFontMetrics fm(qApp->font("QPushButton"));
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = static_cast<const QStyleOption &>(option);
        opt.text = addFolderText(index.data(FolderStatusDelegate::IsUsingSpaces).toBool());
        return QApplication::style()->sizeFromContents(
                                        QStyle::CT_PushButton, &opt, fm.size(Qt::TextSingleLine, opt.text))
                   .expandedTo(QApplication::globalStrut())
            + QSize(0, margins);
    }

    if (classif != FolderStatusModel::RootFolder) {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    // calc height
    int h = rootFolderHeightWithoutErrors(fm, aliasFm);
    // this already includes the bottom margin

    // add some space for the message boxes.
    int margin = fm.height() / 4;
    for (auto role : {FolderConflictMsg, FolderErrorMsg, FolderInfoMsg}) {
        auto msgs = qvariant_cast<QStringList>(index.data(role));
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
    h += fm.height(); // local path
    h += margin; // between local and remote path
    h += fm.height(); // remote path
    h += margin; // bottom margin
    return h;
}

void FolderStatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    const bool useSpaces = index.data(FolderStatusDelegate::IsUsingSpaces).toBool();

    auto textAlign = Qt::AlignLeft;

    QFont aliasFont = makeAliasFont(option.font);
    QFont subFont = option.font;
    QFont errorFont = subFont;
    QFont progressFont = subFont;

    progressFont.setPointSize(subFont.pointSize() - 2);

    QFontMetrics subFm(subFont);
    QFontMetrics aliasFm(aliasFont);

    int aliasMargin = aliasFm.height() / 2;
    int margin = subFm.height() / 4;

    if (index.data(AddButton).toBool()) {
        QSize hint = sizeHint(option, index);
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = static_cast<const QStyleOption &>(option);
        opt.state &= ~QStyle::State_Selected;
        opt.state |= QStyle::State_Raised;
        opt.text = addFolderText(useSpaces);
        opt.rect.setWidth(qMin(opt.rect.width(), hint.width()));
        opt.rect.adjust(0, aliasMargin, 0, -aliasMargin);
        opt.rect = QStyle::visualRect(option.direction, option.rect, opt.rect);
        painter->save();
        painter->setFont(qApp->font("QPushButton"));
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt, painter, option.widget);
        painter->restore();
        return;
    }

    if (static_cast<const FolderStatusModel *>(index.model())->classify(index) != FolderStatusModel::RootFolder) {
        return;
    }
    painter->save();

    const QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIconRole));
    const QString aliasText = qvariant_cast<QString>(index.data(HeaderRole));
    const QStringList conflictTexts = qvariant_cast<QStringList>(index.data(FolderConflictMsg));
    const QStringList errorTexts = qvariant_cast<QStringList>(index.data(FolderErrorMsg));
    const QStringList infoTexts = qvariant_cast<QStringList>(index.data(FolderInfoMsg));

    const int overallPercent = qvariant_cast<int>(index.data(SyncProgressOverallPercent));
    const QString overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    const QString itemString = qvariant_cast<QString>(index.data(SyncProgressItemString));
    const int warningCount = qvariant_cast<int>(index.data(WarningCount));
    const bool syncOngoing = qvariant_cast<bool>(index.data(SyncRunning));
    const bool syncEnabled = qvariant_cast<bool>(index.data(FolderAccountConnected));
    const QString syncText = qvariant_cast<QString>(index.data(FolderSyncText));

    QRect iconRect = option.rect;
    QRect aliasRect = option.rect;

    iconRect.setLeft(option.rect.left() + aliasMargin);
    iconRect.setTop(iconRect.top() + aliasMargin); // (iconRect.height()-iconsize.height())/2);

    // alias box
    aliasRect.setTop(aliasRect.top() + aliasMargin);
    aliasRect.setBottom(aliasRect.top() + aliasFm.height());
    aliasRect.setRight(aliasRect.right() - aliasMargin);

    // remote directory box
    QRect remotePathRect = aliasRect;
    remotePathRect.setTop(aliasRect.bottom() + margin);
    remotePathRect.setBottom(remotePathRect.top() + subFm.height());

    // local directory box
    QRect localPathRect = remotePathRect;
    localPathRect.setTop(remotePathRect.bottom() + margin);
    localPathRect.setBottom(localPathRect.top() + subFm.height());

    iconRect.setBottom(localPathRect.bottom());
    iconRect.setWidth(iconRect.height());

    int nextToIcon = iconRect.right() + aliasMargin;
    aliasRect.setLeft(nextToIcon);
    localPathRect.setLeft(nextToIcon);
    remotePathRect.setLeft(nextToIcon);

    int iconSize = iconRect.width();

    auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    QPixmap pm = statusIcon.pixmap(iconSize, iconSize, syncEnabled ? QIcon::Normal : QIcon::Disabled);
    painter->drawPixmap(QStyle::visualRect(option.direction, option.rect, iconRect).left(),
        iconRect.top(), pm);

    // only show the warning icon if the sync is running. Otherwise its
    // encoded in the status icon.
    if (warningCount > 0 && syncOngoing) {
        QRect warnRect;
        warnRect.setLeft(iconRect.left());
        warnRect.setTop(iconRect.bottom() - 17);
        warnRect.setWidth(16);
        warnRect.setHeight(16);

        const QIcon warnIcon = Utility::getCoreIcon(QStringLiteral("warning"));
        QPixmap pm = warnIcon.pixmap(16, 16, syncEnabled ? QIcon::Normal : QIcon::Disabled);
        warnRect = QStyle::visualRect(option.direction, option.rect, warnRect);
        painter->drawPixmap(QPoint(warnRect.left(), warnRect.top()), pm);
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
    QString elidedAlias = aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width());
    painter->setFont(aliasFont);
    painter->drawText(QStyle::visualRect(option.direction, option.rect, aliasRect), textAlign, elidedAlias);

    const bool showProgess = !overallString.isEmpty() || !itemString.isEmpty();
    if (!showProgess) {
        painter->setFont(subFont);
        const QString elidedRemotePathText = subFm.elidedText(
            syncText,
            Qt::ElideRight, remotePathRect.width());
        painter->drawText(QStyle::visualRect(option.direction, option.rect, remotePathRect),
            textAlign, elidedRemotePathText);
    }

    int h = iconRect.bottom() + margin;

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&](const QStringList &texts, QColor color) {
        QRect rect = localPathRect;
        rect.setLeft(iconRect.left());
        rect.setTop(h);
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
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign,
                subFm.elidedText(eText, Qt::ElideLeft, textRect.width()));
            textRect.translate(0, textRect.height());
        }
        painter->restore();

        h = rect.bottom() + margin;
    };

    if (!conflictTexts.isEmpty())
        drawTextBox(conflictTexts, QColor(0xba, 0xba, 0x4d));
    if (!errorTexts.isEmpty())
        drawTextBox(errorTexts, QColor(0xbb, 0x4d, 0x4d));
    if (!infoTexts.isEmpty())
        drawTextBox(infoTexts, QColor(0x4d, 0x4d, 0xba));

    // Sync File Progress Bar: Show it if syncFile is not empty.
    if (showProgess) {
        int fileNameTextHeight = subFm.boundingRect(tr("File")).height();
        int barHeight = 7; // same height as quota bar
        int overallWidth = option.rect.right() - aliasMargin - optionsButtonVisualRect.width() - nextToIcon;

        painter->save();

        // Overall Progress Bar.
        QRect pBRect;
        pBRect.setTop(remotePathRect.top());
        pBRect.setLeft(nextToIcon);
        pBRect.setHeight(barHeight);
        pBRect.setWidth(overallWidth - 2 * margin);

        QStyleOptionProgressBar pBarOpt;

        pBarOpt.state = option.state | QStyle::State_Horizontal;
        pBarOpt.minimum = 0;
        pBarOpt.maximum = 100;
        pBarOpt.progress = overallPercent;
        pBarOpt.orientation = Qt::Horizontal;
        pBarOpt.rect = QStyle::visualRect(option.direction, option.rect, pBRect);
        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pBarOpt, painter, option.widget);


        // Overall Progress Text
        QRect overallProgressRect;
        overallProgressRect.setTop(pBRect.bottom() + margin);
        overallProgressRect.setHeight(fileNameTextHeight);
        overallProgressRect.setLeft(pBRect.left());
        overallProgressRect.setWidth(pBRect.width());
        painter->setFont(progressFont);

        painter->drawText(QStyle::visualRect(option.direction, option.rect, overallProgressRect),
            Qt::AlignLeft | Qt::AlignVCenter, overallString);
        // painter->drawRect(overallProgressRect);

        painter->restore();
    }

    painter->restore();

    {
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
    for (auto role : {FolderConflictMsg, FolderErrorMsg}) {
        auto msgs = qvariant_cast<QStringList>(index.data(role));
        if (!msgs.isEmpty()) {
            h += margin + 2 * margin + msgs.count() * fm.height();
        }
    }
    within.setHeight(h);
    return within;
}


} // namespace OCC
