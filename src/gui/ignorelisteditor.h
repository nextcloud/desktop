/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

namespace OCC {

namespace Ui {
class IgnoreListEditor;
}

class IgnoreListEditor : public QDialog
{
    Q_OBJECT

public:
    explicit IgnoreListEditor(QWidget *parent = 0);
    ~IgnoreListEditor();

private slots:
    void slotItemSelectionChanged();
    void slotRemoveCurrentItem();
    void slotUpdateLocalIgnoreList();
    void slotAddPattern();
    void slotEditPattern(QListWidgetItem*);

private:
    void readIgnoreFile(const QString& file, bool readOnly);
    Ui::IgnoreListEditor *ui;
};

} // namespace OCC

#endif // IGNORELISTEDITOR_H
