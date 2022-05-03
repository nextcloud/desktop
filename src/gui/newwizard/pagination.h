/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#pragma once

#include <QHBoxLayout>
#include <QRadioButton>
#include <QString>
#include <QWidget>

namespace OCC::Wizard {

using PageIndex = QStringList::size_type;

/**
 * Renders pagination entries as radio buttons in a horizontal layout.
 */
class Pagination : public QWidget
{
    Q_OBJECT

public:
    explicit Pagination(QWidget *parent = nullptr);

    ~Pagination() noexcept override;

    void setEntries(const QStringList &newEntries);

    [[nodiscard]] PageIndex entriesCount() const;

    PageIndex activePageIndex();

Q_SIGNALS:
    void paginationEntryClicked(PageIndex clickedPageIndex);

public Q_SLOTS:
    void setActivePageIndex(PageIndex activePageIndex);

private:
    void removeAllItems();
    void enableOrDisableButtons();

    QList<QRadioButton *> _entries;
    PageIndex _activePageIndex;
    bool _enabled;
};

}
