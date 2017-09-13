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
#include <theme.h>
#include <account.h>

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
    m_moreIcon = QIcon(QLatin1String(":/client/resources/more.png"));
}

QString FolderStatusDelegate::addFolderText()
{
    return tr("Add Folder Sync Connection");
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
        QFontMetrics fm(option.font);
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        opt.text = addFolderText();
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

    // add some space to show an conflict indicator.
    int margin = fm.height() / 4;
    if (!qvariant_cast<QStringList>(index.data(FolderConflictMsg)).isEmpty()) {
        QStringList msgs = qvariant_cast<QStringList>(index.data(FolderConflictMsg));
        h += margin + 2 * margin + msgs.count() * fm.height();
    }
    // add some space to show an error condition.
    if (!qvariant_cast<QStringList>(index.data(FolderErrorMsg)).isEmpty()) {
        QStringList errMsgs = qvariant_cast<QStringList>(index.data(FolderErrorMsg));
        h += margin + 2 * margin + errMsgs.count() * fm.height();
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

    auto textAlign = Qt::AlignLeft;

    QFont aliasFont = makeAliasFont(option.font);
    QFont subFont = option.font;
    QFont errorFont = subFont;
    QFont progressFont = subFont;

    progressFont.setPointSize(subFont.pointSize() - 2);

    QFontMetrics subFm(subFont);
    QFontMetrics aliasFm(aliasFont);
    QFontMetrics progressFm(progressFont);

    int aliasMargin = aliasFm.height() / 2;
    int margin = subFm.height() / 4;

    if (index.data(AddButton).toBool()) {
        QSize hint = sizeHint(option, index);
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        opt.state &= ~QStyle::State_Selected;
        opt.state |= QStyle::State_Raised;
        opt.text = addFolderText();
        opt.rect.setWidth(qMin(opt.rect.width(), hint.width()));
        opt.rect.adjust(0, aliasMargin, 0, -aliasMargin);
        opt.rect = QStyle::visualRect(option.direction, option.rect, opt.rect);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt, painter, option.widget);
        return;
    }

    if (static_cast<const FolderStatusModel *>(index.model())->classify(index) != FolderStatusModel::RootFolder) {
        return;
    }
    painter->save();

    QIcon statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIconRole));
    QString aliasText = qvariant_cast<QString>(index.data(HeaderRole));
    QString pathText = qvariant_cast<QString>(index.data(FolderPathRole));
    QString remotePath = qvariant_cast<QString>(index.data(FolderSecondPathRole));
    QStringList conflictTexts = qvariant_cast<QStringList>(index.data(FolderConflictMsg));
    QStringList errorTexts = qvariant_cast<QStringList>(index.data(FolderErrorMsg));

    int overallPercent = qvariant_cast<int>(index.data(SyncProgressOverallPercent));
    QString overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    QString itemString = qvariant_cast<QString>(index.data(SyncProgressItemString));
    int warningCount = qvariant_cast<int>(index.data(WarningCount));
    bool syncOngoing = qvariant_cast<bool>(index.data(SyncRunning));
    bool syncEnabled = qvariant_cast<bool>(index.data(FolderAccountConnected));

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

        QIcon warnIcon(":/client/resources/warning");
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
        QString elidedRemotePathText = subFm.elidedText(
            tr("Synchronizing with local folder"),
            Qt::ElideRight, remotePathRect.width());
        painter->drawText(QStyle::visualRect(option.direction, option.rect, remotePathRect),
            textAlign, elidedRemotePathText);

        QString elidedPathText = subFm.elidedText(pathText, Qt::ElideMiddle, localPathRect.width());
        painter->drawText(QStyle::visualRect(option.direction, option.rect, localPathRect),
            textAlign, elidedPathText);
    }

    int h = iconRect.bottom() + margin;

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&](const QStringList &texts, QColor color) {
        QRect rect = localPathRect;
        rect.setLeft(iconRect.left());
        rect.setTop(h);
        rect.setHeight(texts.count() * subFm.height() + 2 * margin);
        rect.setRight(option.rect.right() - margin);

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

        foreach (QString eText, texts) {
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign,
                subFm.elidedText(eText, Qt::ElideLeft, textRect.width()));
            textRect.translate(0, textRect.height());
        }

        h = rect.bottom() + margin;
    };

    if (!conflictTexts.isEmpty())
        drawTextBox(conflictTexts, QColor(0xba, 0xba, 0x4d));
    if (!errorTexts.isEmpty())
        drawTextBox(errorTexts, QColor(0xbb, 0x4d, 0x4d));

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

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &pBarOpt, painter);

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
        //btnOpt.text = QLatin1String("...");
        btnOpt.state = option.state;
        btnOpt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
        btnOpt.state |= QStyle::State_Raised;
        btnOpt.arrowType = Qt::NoArrow;
        btnOpt.subControls = QStyle::SC_ToolButton;
        btnOpt.rect = optionsButtonVisualRect;
        btnOpt.icon = m_moreIcon;
        btnOpt.iconSize = btnOpt.rect.size();
        QApplication::style()->drawComplexControl(QStyle::CC_ToolButton, &btnOpt, painter);
    }
}

bool FolderStatusDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect FolderStatusDelegate::optionsButtonRect(QRect within, Qt::LayoutDirection direction)
{
    QFont font = QFont();
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setHeight(FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));

    QStyleOptionToolButton opt;
    opt.text = QLatin1String("...");
    QSize textSize = fm.size(Qt::TextShowMnemonic, opt.text);
    opt.rect.setSize(textSize);
    QSize size = QApplication::style()->sizeFromContents(QStyle::CT_ToolButton, &opt, textSize).expandedTo(QApplication::globalStrut());

    int margin = QApplication::style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    QRect r(QPoint(within.right() - size.width() - margin,
                within.top() + within.height() / 2 - size.height() / 2),
        size);
    return QStyle::visualRect(direction, within, r);
}

QRect FolderStatusDelegate::errorsListRect(QRect within)
{
    QFont font = QFont();
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setTop(within.top() + FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));
    return within;
}


} // namespace OCC
