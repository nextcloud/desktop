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
#include "folderstatusview.h"
#include "folderman.h"
#include "accountstate.h"
#include "sesstyle.h"
#include "buttonstyle.h"

#include <theme.h>
#include <ionostheme.h>
#include <account.h>

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QStyleFactory>
#include <iostream>

namespace {
#ifdef Q_OS_MACOS
    const auto backupStyle = QStyleFactory::create("Fusion");
#endif
}

namespace OCC {

inline static QFont makeAliasFont(const QFont &normalFont)
{
    QFont aliasFont = normalFont;
    aliasFont.setWeight(IonosTheme::settingsTitleWeightDemiBold());
    aliasFont.setPixelSize(IonosTheme::settingsBigTitlePixel());
    return aliasFont;
}

FolderStatusDelegate::FolderStatusDelegate()
    : QStyledItemDelegate()
{
    customizeStyle();
}

QString FolderStatusDelegate::addFolderText()
{
    return tr("Add live backup");
}

QString FolderStatusDelegate::addInfoText()
{
    return tr("Synchronize any other local folder with your %1").arg(Theme::instance()->appNameGUI());
}

// allocate each item size in listview.
QSize FolderStatusDelegate::sizeHint(const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    QFont font = QFont(option.font);
    font.setPixelSize(IonosTheme::settingsTextPixel());
    QFont aliasFont = makeAliasFont(font);

    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);

    auto classif = dynamic_cast<const FolderStatusModel *>(index.model())->classify(index);
    if (classif == FolderStatusModel::AddButton) {
        const int margins = aliasFm.height(); // same as 2*aliasMargin of paint
        QFontMetrics fm(qApp->font("QPushButton"));
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        opt.text = addInfoText();
        return QApplication::style()->sizeFromContents( QStyle::CT_PushButton, &opt, fm.size(Qt::TextSingleLine, opt.text)) +
                QSize(0, margins);
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

    return {0, h};
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

void FolderStatusDelegate::drawAddButton(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFont titleFont = option.font;
    titleFont.setWeight(IonosTheme::settingsTitleWeightDemiBold());
    titleFont.setPixelSize(IonosTheme::settingsTitlePixel());
    QFontMetrics titleTextFm(titleFont);
    const auto baseDistanceForCalculus = titleTextFm.height() / 2;

    QFont subtitleFont = option.font;

    QFontMetrics subtitleTextFm(subtitleFont);
    const auto distanceToSubline = subtitleTextFm.height() / 4;

    auto iconBox = option.rect;
    iconBox.setTop(iconBox.top() + baseDistanceForCalculus);
    iconBox.setBottom(iconBox.top() + IonosTheme::treeViewIconSize());
    iconBox.setLeft(iconBox.left() + baseDistanceForCalculus);
    iconBox.setWidth(iconBox.height());

    auto titleBox = option.rect;
    titleBox.setTop(iconBox.top());
    titleBox.setBottom(iconBox.bottom() - distanceToSubline);
    titleBox.setRight(titleBox.right() - baseDistanceForCalculus);
    titleBox.setLeft(iconBox.right() + baseDistanceForCalculus);

    auto subtitleBox = option.rect;
    subtitleBox.setTop(titleBox.bottom() + distanceToSubline);
    subtitleBox.setBottom(subtitleBox.top() + 4 * distanceToSubline);
    subtitleBox.setLeft(iconBox.right() + baseDistanceForCalculus);
    subtitleBox.setRight(subtitleBox.right() - baseDistanceForCalculus);

    auto titleText = addFolderText();
    auto subtitleText = addInfoText();
    auto addIcon = QIcon(IonosTheme::liveBackupPlusIcon());
    const auto addPixmap = addIcon.pixmap(iconBox.size(), QIcon::Normal);

    painter->save();
    painter->drawPixmap(QStyle::visualRect(option.direction, option.rect, iconBox).left(), iconBox.top(), addPixmap);

    drawElidedText(painter, option, titleTextFm, titleFont, titleText, titleBox);

    drawElidedText(painter, option, subtitleTextFm, subtitleFont, subtitleText, subtitleBox);

    painter->restore();
}

void FolderStatusDelegate::drawElidedText(QPainter *painter, QStyleOptionViewItem option, QFontMetrics fontMetric, QFont font, QString text, QRect rect) const{
    const auto elidedText = fontMetric.elidedText(text, Qt::ElideRight, rect.width());
    painter->setFont(font);
    painter->drawText(QStyle::visualRect(option.direction, option.rect, rect), Qt::AlignLeft, elidedText);
}

void FolderStatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    QFont font = opt.font;
    font.setPixelSize(IonosTheme::settingsTextPixel());
    opt.font = font;

    QStyledItemDelegate::paint(painter, opt, index);

    if (index.data(AddButton).toBool()) {
        drawAddButton(painter, opt, index);
        return;
    }

    auto textAlign = Qt::AlignLeft;

    const auto aliasFont = makeAliasFont(opt.font);
    const auto subFont = opt.font;

    const auto errorFont = subFont;

    QFontMetrics subFm(subFont);
    QFontMetrics aliasFm(aliasFont);

    const auto aliasMargin = aliasFm.height() / 2;
    const auto margin = subFm.height() / 4;

    if (dynamic_cast<const FolderStatusModel *>(index.model())->classify(index) != FolderStatusModel::RootFolder) {
        return;
    }
    painter->save();

    auto statusIcon = qvariant_cast<QIcon>(index.data(FolderStatusIconRole));
    auto aliasText = qvariant_cast<QString>(index.data(HeaderRole));
    auto pathText = qvariant_cast<QString>(index.data(FolderPathRole));
    auto conflictTexts = qvariant_cast<QStringList>(index.data(FolderConflictMsg));
    auto errorTexts = qvariant_cast<QStringList>(index.data(FolderErrorMsg));
    auto infoTexts = qvariant_cast<QStringList>(index.data(FolderInfoMsg));

    auto overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    auto itemString = qvariant_cast<QString>(index.data(SyncProgressItemString));
    auto warningCount = qvariant_cast<int>(index.data(WarningCount));
    auto syncOngoing = qvariant_cast<bool>(index.data(SyncRunning));
    auto syncEnabled = qvariant_cast<bool>(index.data(FolderAccountConnected));
    auto syncText = qvariant_cast<QString>(index.data(FolderSyncText));

    auto iconRect = option.rect;
    auto aliasRect = option.rect;

    iconRect.setLeft(option.rect.left() + aliasMargin);
    iconRect.setTop(iconRect.top() + aliasMargin); // (iconRect.height()-iconsize.height())/2);

    // alias box
    aliasRect.setTop(aliasRect.top() + aliasMargin);
    aliasRect.setBottom(aliasRect.top() + aliasFm.height());
    aliasRect.setRight(aliasRect.right() - aliasMargin);

    // remote directory box
    auto remotePathRect = aliasRect;
    remotePathRect.setTop(aliasRect.bottom() + margin);
    remotePathRect.setBottom(remotePathRect.top() + subFm.height());

    // local directory box
    auto localPathRect = remotePathRect;
    localPathRect.setTop(remotePathRect.bottom() + margin);
    localPathRect.setBottom(localPathRect.top() + subFm.height());

    iconRect.setBottom(iconRect.top() + IonosTheme::treeViewIconSize());
    iconRect.setWidth(iconRect.height());

    const auto nextToIcon = iconRect.right() + aliasMargin;
    aliasRect.setLeft(nextToIcon);
    localPathRect.setLeft(nextToIcon);
    remotePathRect.setLeft(nextToIcon);

    const auto iconSize = iconRect.width();

    const auto statusPixmap = statusIcon.pixmap(iconSize, iconSize, syncEnabled ? QIcon::Normal : QIcon::Disabled);
    painter->drawPixmap(QStyle::visualRect(option.direction, option.rect, iconRect).left(), iconRect.top(), statusPixmap);

    // only show the warning icon if the sync is running. Otherwise its
    // encoded in the status icon.
    if (warningCount > 0 && syncOngoing) {
        QRect warnRect;
        warnRect.setLeft(iconRect.left());
        warnRect.setTop(iconRect.bottom() - 17);
        warnRect.setWidth(16);
        warnRect.setHeight(16);

        QIcon warnIcon(":/client/theme/warning");
        const auto warnPixmap = warnIcon.pixmap(16, 16, syncEnabled ? QIcon::Normal : QIcon::Disabled);
        warnRect = QStyle::visualRect(option.direction, option.rect, warnRect);
        painter->drawPixmap(QPoint(warnRect.left(), warnRect.top()), warnPixmap);
    }

    auto palette = option.palette;

    auto colourGroup = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (colourGroup == QPalette::Normal && !(option.state & QStyle::State_Active)) {
        colourGroup = QPalette::Inactive;
    }

    if (option.state & QStyle::State_Selected) {
        painter->setPen(palette.color(colourGroup, QPalette::HighlightedText));
    } else {
        painter->setPen(palette.color(colourGroup, QPalette::Text));
    }

    drawElidedText(painter, option, aliasFm, aliasFont, aliasText, aliasRect);

    const auto showProgess = !overallString.isEmpty() || !itemString.isEmpty();
    if (!showProgess) {
        drawElidedText(painter, option, subFm, subFont, syncText, remotePathRect);

        drawElidedText(painter, option, subFm, subFont, pathText, localPathRect);
    }

    auto textBoxTop = qMax(localPathRect.bottom(), remotePathRect.bottom()) +  margin;

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&](const QStringList &texts, QColor color, QColor borderColor) {
        auto rect = localPathRect;
        rect.setLeft(iconRect.left());
        rect.setTop(textBoxTop);
        rect.setHeight(texts.count() * subFm.height() + 2 * margin);
        rect.setRight(option.rect.right() - aliasMargin);

        // save previous state to not mess up colours with the background (fixes issue: https://github.com/nextcloud/desktop/issues/1237)
        painter->save();
        painter->setBrush(color);
        painter->setPen(borderColor);
        painter->drawRoundedRect(QStyle::visualRect(option.direction, option.rect, rect),
            4, 4);
        painter->setPen(Qt::black);
        painter->setFont(errorFont);
        QRect textRect(rect.left() + margin,
            rect.top() + margin,
            rect.width() - 2 * margin,
            subFm.height());

        for (const auto &eText : texts) {
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign, subFm.elidedText(eText, Qt::ElideRight, textRect.width()));
            textRect.translate(0, textRect.height());
        }
        // restore previous state
        painter->restore();

        textBoxTop = rect.bottom() + margin;
    };

    if (!conflictTexts.isEmpty()) {
        drawTextBox(conflictTexts, QColor(IonosTheme::warningColor()), QColor(IonosTheme::warningBorderColor()));
    }
    if (!errorTexts.isEmpty()) {
        drawTextBox(errorTexts, QColor(IonosTheme::errorColor()), QColor(IonosTheme::errorBorderColor()));
    }
    if (!infoTexts.isEmpty()) {
        drawTextBox(infoTexts, QColor(IonosTheme::infoColor()), QColor(IonosTheme::infoBorderColor()));
    }

    // Sync File Progress Bar: Show it if syncFile is not empty.
    if (showProgess) {
        drawSyncProgressBar(painter, opt, index, subFm, aliasMargin, remotePathRect, margin, nextToIcon);
    }

    drawMoreOptionsButton(painter, option, index);
}

void FolderStatusDelegate::drawSyncProgressBar(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index, const QFontMetrics &subFm, const int aliasMargin, const QRect &remotePathRect, const int margin, const int nextToIcon) const
{
    auto overallPercent = qvariant_cast<int>(index.data(SyncProgressOverallPercent));
    auto overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    const auto fileNameTextHeight = subFm.boundingRect(tr("File")).height();
    constexpr auto barHeight = 7; // same height as quota bar
    const auto overallWidth = option.rect.right() - aliasMargin - optionsButtonVisualRect.width() - nextToIcon;

    QFont progressFont(option.font);
    progressFont.setPixelSize(IonosTheme::settingsTextPixel());
    progressFont.setWeight(IonosTheme::settingsTitleWeightNormal());
    painter->save();

        // Overall Progress Bar.
    const auto progressBarRect = QRect(nextToIcon,
                                        remotePathRect.top(),
                                        overallWidth - 2 * margin,
                                        barHeight);

    QStyleOptionProgressBar progressBarOpt;

    progressBarOpt.state = option.state | QStyle::State_Horizontal;
    progressBarOpt.minimum = 0;
    progressBarOpt.maximum = 100;
    progressBarOpt.progress = overallPercent;
    progressBarOpt.state = QStyle::StateFlag::State_Horizontal;
    progressBarOpt.rect = QStyle::visualRect(option.direction, option.rect, progressBarRect);
#ifdef Q_OS_MACOS
    backupStyle->drawControl(QStyle::CE_ProgressBar, &progressBarOpt, painter, option.widget);
#else
    QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBarOpt, painter, option.widget);
#endif

// Overall Progress Text
    QRect overallProgressRect;
    overallProgressRect.setTop(progressBarRect.bottom() + margin);
    overallProgressRect.setHeight(fileNameTextHeight);
    overallProgressRect.setLeft(progressBarRect.left());
    overallProgressRect.setWidth(progressBarRect.width());
    painter->setFont(progressFont);

    painter->drawText(QStyle::visualRect(option.direction, option.rect, overallProgressRect), Qt::AlignLeft | Qt::AlignVCenter, overallString);

    // // itemString is e.g. Syncing fileName1, filename2
    // // syncText is Synchronizing files in local folders or Synchronizing virtual files in local folder
    // const auto generalSyncStatus = !itemString.isEmpty() ? itemString : syncText;
    // QRect generalSyncStatusRect;
    // generalSyncStatusRect.setTop(progressBarRect.bottom() + margin);
    // generalSyncStatusRect.setHeight(fileNameTextHeight);
    // generalSyncStatusRect.setLeft(progressBarRect.left());
    // generalSyncStatusRect.setWidth(progressBarRect.width());
    // // painter->setFont(progressFont);

    // painter->drawText(QStyle::visualRect(option.direction, option.rect, generalSyncStatusRect), Qt::AlignLeft | Qt::AlignVCenter, generalSyncStatus);

    painter->restore();
}

void FolderStatusDelegate::drawMoreOptionsButton(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    QStyleOptionButton btnOpt;
    btnOpt.state = option.state;
    btnOpt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus | QStyle::State_MouseOver);
    btnOpt.state |= QStyle::State_Raised;

    if(optionsButtonVisualRect.contains(MousePos)  )
    {
        btnOpt.state |= QStyle::State_MouseOver;
    }

    if (btnOpt.state & QStyle::State_Enabled && btnOpt.state & QStyle::State_MouseOver && index == _pressedIndex) {
        btnOpt.state |= QStyle::State_Sunken;
    } else {
        btnOpt.state |= QStyle::State_Raised;
    }

    btnOpt.rect = optionsButtonVisualRect;
    btnOpt.icon = _iconMore;
    const auto iconSize = optionsButtonIconSize();
    btnOpt.iconSize = QSize(iconSize, iconSize);
    QWidget buttonWidget;
    buttonWidget.setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::MoreOptions));

    QApplication::style()->
        drawControl(
            static_cast<QStyle::ControlElement>(sesStyle::CE_TreeViewMoreOptions), &btnOpt, painter, &buttonWidget);
}

int FolderStatusDelegate::optionsButtonIconSize() {
    // Using this calculation to use the DPI-Scaled values. The QStyleHelper::dpiScaled is not accessible from here.
    return QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize) - QApplication::style()->pixelMetric(QStyle::PM_MenuScrollerHeight);
}

bool FolderStatusDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
        if (const auto *view = qobject_cast<const QAbstractItemView *>(option.widget)) {
            auto *me = dynamic_cast<QMouseEvent *>(event);
            QModelIndex pressedIndex;
            if (me->buttons()) {
                pressedIndex = view->indexAt(me->pos());
            }
            if (_pressedIndex != pressedIndex) {
                _pressedIndex = pressedIndex;
                view->viewport()->update();
            }
            auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

            MousePos = me->pos();
            if(optionsButtonVisualRect.contains(MousePos))
            {
                _hoveredIndex = index;
                view->viewport()->update();
            } else if(_hoveredIndex.isValid())
            {
                _hoveredIndex = QModelIndex();
                view->viewport()->update();
            }
        }
        break;
    case QEvent::MouseButtonRelease:
        _pressedIndex = QModelIndex();
        break;
    default:
        break;
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

QRect FolderStatusDelegate::optionsButtonRect(QRect within, Qt::LayoutDirection direction)
{
    QFont font = QFont(IonosTheme::settingsFont());
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setHeight(FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));

    QStyleOptionButton opt;
    int iconSize = optionsButtonIconSize();
    opt.rect.setSize(QSize(iconSize,iconSize));
    QSize size = QApplication::style()->sizeFromContents(
        static_cast<QStyle::ContentsType>(sesStyle::CT_TreeViewMoreOptions), &opt, opt.rect.size());

    // Using PM_LargeIconSize as margin because it get DPI Scaled, which I canot access from here
    int margin = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
    QRect r(QPoint(within.right() - size.width() - aliasFm.height() / 2,
                within.top() + within.height() / 2 - size.height() / 2),
        size);
    return QStyle::visualRect(direction, within, r);
}

QRect FolderStatusDelegate::errorsListRect(QRect within)
{
    QFont font = QFont(IonosTheme::settingsFont());
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setTop(within.top() + FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));
    return within;
}

void FolderStatusDelegate::slotStyleChanged()
{
    customizeStyle();
}

void FolderStatusDelegate::customizeStyle()
{
    _iconMore = Theme::createColorAwareIcon(QLatin1String(":/client/theme/ses/ses-more.svg"), QSize(128, 128));
}

} // namespace OCC
