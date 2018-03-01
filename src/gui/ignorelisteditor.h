/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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
    explicit IgnoreListEditor(QWidget *parent = 0);
    ~IgnoreListEditor();

    bool ignoreHiddenFiles();

private slots:
    void slotItemSelectionChanged();
    void slotRemoveCurrentItem();
    void slotUpdateLocalIgnoreList();
    void slotAddPattern();
    void slotRestoreDefaults(QAbstractButton *button);
    void slotRemoveAllItems();

private:
    void readIgnoreFile(const QString &file, bool readOnly);
    void setupTableReadOnlyItems();
    int addPattern(const QString &pattern, bool deletable, bool readOnly);
    QString readOnlyTooltip;
    Ui::IgnoreListEditor *ui;
};

} // namespace OCC

#endif // IGNORELISTEDITOR_H
