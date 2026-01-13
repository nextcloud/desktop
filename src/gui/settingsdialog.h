/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QStyledItemDelegate>

#include "progressdispatcher.h"
#include "owncloudgui.h"

class QAction;
class QActionGroup;
class QToolBar;
class QStandardItemModel;
class QResizeEvent;

namespace OCC {

namespace Ui {
    class SettingsDialog;
}

class AccountState;
class AccountSettings;
class Application;
class FolderMan;
class ownCloudGui;

/**
 * @brief The SettingsDialog class
 * @ingroup gui
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QWidget* currentPage READ currentPage NOTIFY currentPageChanged)

public:
    explicit SettingsDialog(ownCloudGui *gui, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    QWidget* currentPage();

public slots:
    void showFirstPage();
    void showIssuesList(OCC::AccountState *account);
    void slotSwitchPage(QAction *action);
    void slotAccountAvatarChanged();
    void slotAccountDisplayNameChanged();

signals:
    void styleChanged();
    void onActivate();
    void currentPageChanged();

protected:
    void reject() override;
    void accept() override;
    void changeEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void accountAdded(OCC::AccountState *);
    void accountRemoved(OCC::AccountState *);

private:
    void customizeStyle();
    void requestStyleUpdate();
    void updateAccountAvatar(const Account *account);

    QAction *createColorAwareAction(const QString &iconName, const QString &fileName);
    QAction *createActionWithIcon(const QIcon &icon, const QString &text, const QString &iconPath = QString());

    Ui::SettingsDialog *const _ui;

    QActionGroup *_actionGroup;
    // Maps the actions from the action group to the corresponding widgets
    QHash<QAction *, QWidget *> _actionGroupWidgets;

    // Maps the action in the dialog to their according account. Needed in
    // case the account avatar changes
    QHash<const Account *, QAction *> _actionForAccount;

    QToolBar *_toolBar;

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    QWidget *_windowDragHandle = nullptr;
#endif

    ownCloudGui *_gui;
    bool _styleUpdatePending = false;
    bool _updatingStyle = false;
};
}

#endif // SETTINGSDIALOG_H
