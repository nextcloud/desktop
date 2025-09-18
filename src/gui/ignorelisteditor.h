/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IGNORELISTEDITOR_H
#define IGNORELISTEDITOR_H

#include <QDialog>

class QListWidgetItem;
class QAbstractButton;

namespace OCC {

namespace Ui {
    class IgnoreListEditor;
}

/**
 * @brief The IgnoreListEditor class
 * @ingroup gui
 */
class IgnoreListEditor : public QDialog
{
    Q_OBJECT

public:
    IgnoreListEditor(QWidget *parent = nullptr);
    ~IgnoreListEditor() override;

    bool ignoreHiddenFiles();

private slots:
    void slotRestoreDefaults(QAbstractButton *button);

private:
    void setupTableReadOnlyItems();
    QString readOnlyTooltip;
    Ui::IgnoreListEditor *ui;
};

} // namespace OCC

#endif // IGNORELISTEDITOR_H
