/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderstatusdelegate.h"
#include "folderstatusmodel.h"
#include "folderstatusview.h"
#include "folderman.h"
#include "accountstate.h"
#include <theme.h>
#include <account.h>

#include <QFileIconProvider>
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QStyleFactory>

inline static QFont makeAliasFont(const QFont &normalFont)
{
    QFont aliasFont = normalFont;
    aliasFont.setBold(true);
    aliasFont.setPointSize(normalFont.pointSize() + 2);
    return aliasFont;
}

namespace {
#ifdef Q_OS_MACOS
    const auto backupStyle = QStyleFactory::create("Fusion");
#endif
}

namespace OCC {

FolderStatusDelegate::FolderStatusDelegate()
    : QStyledItemDelegate()
{
    customizeStyle();
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

    auto classif = dynamic_cast<const FolderStatusModel *>(index.model())->classify(index);
    if (classif == FolderStatusModel::AddButton) {
        const int margins = aliasFm.height(); // same as 2*aliasMargin of paint
        QFontMetrics fm(qApp->font("QPushButton"));
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        opt.text = addFolderText();
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

void FolderStatusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.data(AddButton).toBool()) {
        const_cast<QStyleOptionViewItem &>(option).showDecorationSelected = false;
    }

    QStyledItemDelegate::paint(painter, option, index);

    auto textAlign = Qt::AlignLeft;

    const auto aliasFont = makeAliasFont(option.font);
    const auto subFont = option.font;
    const auto errorFont = subFont;
    auto progressFont = subFont;

    progressFont.setPointSize(subFont.pointSize() - 2);

    QFontMetrics subFm(subFont);
    QFontMetrics aliasFm(aliasFont);

    const auto aliasMargin = aliasFm.height() / 2;
    const auto margin = subFm.height() / 4;

    if (index.data(AddButton).toBool()) {
        QStyleOptionButton opt;
        static_cast<QStyleOption &>(opt) = option;
        if (opt.state & QStyle::State_Enabled && opt.state & QStyle::State_MouseOver && index == _pressedIndex) {
            opt.state |= QStyle::State_Sunken;
        } else {
            opt.state |= QStyle::State_Raised;
        }
        opt.text = addFolderText();
        opt.rect = addButtonRect(option.rect, option.direction);
        painter->save();
        painter->setFont(qApp->font("QPushButton"));
        QApplication::style()->drawControl(QStyle::CE_PushButton, &opt, painter, option.widget);
        painter->restore();
        return;
    }

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

    auto overallPercent = qvariant_cast<int>(index.data(SyncProgressOverallPercent));
    auto overallString = qvariant_cast<QString>(index.data(SyncProgressOverallString));
    auto itemString = qvariant_cast<QString>(index.data(SyncProgressItemString));
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

    constexpr int statusIconSize = 24;
    constexpr int statusIconTopOffset = 4;
    iconRect.setSize(QSize(statusIconSize, statusIconSize));
    iconRect.moveTop(iconRect.top() + statusIconTopOffset);
    
    const auto nextToIcon = iconRect.right() + aliasMargin;
    aliasRect.setLeft(nextToIcon);
    localPathRect.setLeft(nextToIcon);
    remotePathRect.setLeft(nextToIcon);

    auto optionsButtonVisualRect = optionsButtonRect(option.rect, option.direction);

    statusIcon.paint(
        painter,
        QStyle::visualRect(option.direction, option.rect, iconRect),
        Qt::AlignCenter,
        syncEnabled ? QIcon::Normal : QIcon::Disabled
    );

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

    const auto elidedAlias = aliasFm.elidedText(aliasText, Qt::ElideRight, aliasRect.width());
    painter->setFont(aliasFont);
    painter->drawText(QStyle::visualRect(option.direction, option.rect, aliasRect), textAlign, elidedAlias);

    const auto showProgess = !overallString.isEmpty() || !itemString.isEmpty();
    if (!showProgess) {
        painter->setFont(subFont);
        const auto elidedRemotePathText = subFm.elidedText(syncText, Qt::ElideRight, remotePathRect.width());
        painter->drawText(QStyle::visualRect(option.direction, option.rect, remotePathRect), textAlign, elidedRemotePathText);

        const auto elidedPathText = subFm.elidedText(pathText, Qt::ElideMiddle, localPathRect.width());
        painter->drawText(QStyle::visualRect(option.direction, option.rect, localPathRect), textAlign, elidedPathText);
    }

    auto textBoxTop = iconRect.bottom() + margin;

    // paint an error overlay if there is an error string or conflict string
    auto drawTextBox = [&](const QStringList &texts, QColor color) {
        auto rect = localPathRect;
        rect.setLeft(iconRect.left());
        rect.setTop(textBoxTop);
        rect.setHeight(texts.count() * subFm.height() + 2 * margin);
        rect.setRight(option.rect.right() - margin);

        // save previous state to not mess up colours with the background (fixes issue: https://github.com/nextcloud/desktop/issues/1237)
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
            painter->drawText(QStyle::visualRect(option.direction, option.rect, textRect), textAlign, subFm.elidedText(eText, Qt::ElideRight, textRect.width()));
            textRect.translate(0, textRect.height());
        }
        // restore previous state
        painter->restore();

        textBoxTop = rect.bottom() + margin;
    };

    if (!conflictTexts.isEmpty()) {
        drawTextBox(conflictTexts, QColor(0xba, 0xba, 0x4d));
    }
    if (!errorTexts.isEmpty()) {
        drawTextBox(errorTexts, QColor(0xbb, 0x4d, 0x4d));
    }
    if (!infoTexts.isEmpty()) {
        drawTextBox(infoTexts, QColor(0x4d, 0x4d, 0xba));
    }

    // Sync File Progress Bar: Show it if syncFile is not empty.
    if (showProgess) {
        const auto fileNameTextHeight = subFm.boundingRect(tr("File")).height();
        constexpr auto barHeight = 7; // same height as quota bar
        const auto overallWidth = option.rect.right() - aliasMargin - optionsButtonVisualRect.width() - nextToIcon;

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

        // itemString is e.g. Syncing fileName1, filename2
        // syncText is Synchronizing files in local folders or Synchronizing virtual files in local folder
        const auto generalSyncStatus = !itemString.isEmpty() ? itemString : syncText;
        QRect generalSyncStatusRect;
        generalSyncStatusRect.setTop(progressBarRect.bottom() + margin);
        generalSyncStatusRect.setHeight(fileNameTextHeight);
        generalSyncStatusRect.setLeft(progressBarRect.left());
        generalSyncStatusRect.setWidth(progressBarRect.width());
        painter->setFont(progressFont);

        painter->drawText(QStyle::visualRect(option.direction, option.rect, generalSyncStatusRect), Qt::AlignLeft | Qt::AlignVCenter, generalSyncStatus);

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
        btnOpt.icon = _iconMore;
        const auto buttonSize = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
        btnOpt.iconSize = QSize(buttonSize, buttonSize);
        QApplication::style()->drawComplexControl(QStyle::CC_ToolButton, &btnOpt, painter);
    }
}

bool FolderStatusDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
        if (const auto *view = qobject_cast<const QAbstractItemView *>(option.widget)) {
            auto *me = dynamic_cast<QMouseEvent *>(event);
            QModelIndex index;
            if (me->buttons()) {
                index = view->indexAt(me->pos());
            }
            if (_pressedIndex != index) {
                _pressedIndex = index;
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
    QFont font = QFont();
    QFont aliasFont = makeAliasFont(font);
    QFontMetrics fm(font);
    QFontMetrics aliasFm(aliasFont);
    within.setHeight(FolderStatusDelegate::rootFolderHeightWithoutErrors(fm, aliasFm));

    QStyleOptionToolButton opt;
    int e = QApplication::style()->pixelMetric(QStyle::PM_ButtonIconSize);
    opt.rect.setSize(QSize(e,e));
    QSize size = QApplication::style()->sizeFromContents(QStyle::CT_ToolButton, &opt, opt.rect.size());

    int margin = QApplication::style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
    QRect r(QPoint(within.right() - size.width() - margin,
                within.top() + within.height() / 2 - size.height() / 2),
        size);
    return QStyle::visualRect(direction, within, r);
}

QRect FolderStatusDelegate::addButtonRect(QRect within, Qt::LayoutDirection direction)
{
    QFontMetrics fm(qApp->font("QPushButton"));
    QStyleOptionButton opt;
    opt.text = addFolderText();
    QSize size = QApplication::style()->sizeFromContents(QStyle::CT_PushButton, &opt, fm.size(Qt::TextSingleLine, opt.text));
    QRect r(QPoint(within.left(), within.top() + within.height() / 2 - size.height() / 2), size);
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

void FolderStatusDelegate::slotStyleChanged()
{
    customizeStyle();
}

void FolderStatusDelegate::customizeStyle()
{
    _iconMore = Theme::createColorAwareIcon(QLatin1String(":/client/theme/more.svg"));
}

} // namespace OCC
