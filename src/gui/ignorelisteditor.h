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
    enum IgnoreListType {
        Global,
        Folder,
    };

    IgnoreListEditor(QWidget *parent = nullptr);
    IgnoreListEditor(const QString &ignoreFile, QWidget *parent = nullptr);

    ~IgnoreListEditor() override;

    [[nodiscard]] bool ignoreHiddenFiles() const;

private slots:
    void slotSaveIgnoreList();
    void slotRestoreDefaults(QAbstractButton *button);

private:
    Ui::IgnoreListEditor *ui;
    QString _ignoreFile;

    IgnoreListType _ignoreListType;

    void setupUi();
    void setupTableReadOnlyItems();
};

} // namespace OCC

#endif // IGNORELISTEDITOR_H
