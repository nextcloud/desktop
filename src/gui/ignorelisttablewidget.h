/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWidget>

class QAbstractButton;

namespace OCC {

namespace Ui {
    class IgnoreListTableWidget;
}

class IgnoreListTableWidget : public QWidget
{
    Q_OBJECT

public:
    IgnoreListTableWidget(QWidget *parent = nullptr);
    ~IgnoreListTableWidget() override;

    void readIgnoreFile(const QString &file, bool readOnly = false);
    int addPattern(const QString &pattern, bool deletable, bool readOnly);

public slots:
    void slotRemoveAllItems();
    void slotWriteIgnoreFile(const QString &file);

private slots:
    void slotItemSelectionChanged();
    void slotRemoveCurrentItem();
    void slotAddPattern();

private:
    void setupTableReadOnlyItems();
    Ui::IgnoreListTableWidget *ui;
};
} // namespace OCC
