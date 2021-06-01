/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "expandingheaderview.h"
#include "models.h"

#include "configfile.h"

#include <QScopedValueRollback>
#include <QApplication>
#include <QDebug>

using namespace OCC;

ExpandingHeaderView::ExpandingHeaderView(const QString &objectName, QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setSectionsClickable(true);
    setHighlightSections(true);

    connect(this, &QHeaderView::sectionCountChanged, this, &ExpandingHeaderView::resizeColumns);
    connect(this, &QHeaderView::sectionResized, this, [this](int index, int oldSize, int newSize) {
        if (_isResizing) {
            return;
        }
        QScopedValueRollback<bool> guard(_isResizing, true);
        if (index != _expandingColumn) {
            // give/take space from _expandingColumn column
            resizeSection(_expandingColumn, sectionSize(_expandingColumn) - (newSize - oldSize));
        } else {
            // distribute space across all columns
            // use actual width as oldSize/newSize isn't reliable here
            auto visibleSections = Models::range(count());
            visibleSections.erase(std::remove_if(visibleSections.begin(), visibleSections.end(), [this](int i) {
                return isSectionHidden(i);
            }),
                visibleSections.end());
            if (visibleSections.empty()) {
                return;
            }
            int availableWidth = width();
            for (auto &i : visibleSections) {
                availableWidth -= sectionSize(i);
            }

            const int diffPerSection = availableWidth / static_cast<int>(visibleSections.size());
            const int extraDiff = availableWidth % visibleSections.size();
            const auto secondLast = visibleSections.cend()[-2];
            for (auto &i : visibleSections) {
                if (_expandingColumn == i) {
                    continue;
                }
                auto newSize = sectionSize(i) + diffPerSection;
                if (i == secondLast) {
                    newSize += extraDiff;
                }
                resizeSection(i, newSize);
            }
        }
    });

    setObjectName(objectName);
    ConfigFile cfg;
    cfg.restoreGeometryHeader(this);

    connect(qApp, &QApplication::aboutToQuit, this, [this] {
        ConfigFile cfg;
        cfg.saveGeometryHeader(this);
    });
}

int ExpandingHeaderView::expandingColumn() const
{
    return _expandingColumn;
}

void ExpandingHeaderView::setExpandingColumn(int newExpandingColumn)
{
    _expandingColumn = newExpandingColumn;
}

void ExpandingHeaderView::resizeEvent(QResizeEvent *event)
{
    QHeaderView::resizeEvent(event);
    resizeColumns();
}

void ExpandingHeaderView::resizeColumns(bool reset)
{
    QScopedValueRollback<bool> guard(_isResizing, true);
    int availableWidth = width();
    const auto defaultSize = defaultSectionSize();
    for (int i = 0; i < count(); ++i) {
        if (i == _expandingColumn || isSectionHidden(i)) {
            continue;
        }
        if (reset) {
            resizeSection(i, defaultSize);
        }
        availableWidth -= sectionSize(i);
    }
    resizeSection(_expandingColumn, availableWidth);
}
