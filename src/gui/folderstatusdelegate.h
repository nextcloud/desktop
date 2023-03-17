/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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
#include <QStyledItemDelegate>

namespace OCC {

/**
 * @brief The FolderStatusDelegate class
 * @ingroup gui
 */
class FolderStatusDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    FolderStatusDelegate(QObject *parent);

    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;


    /**
     * return the position of the option button within the item
     */
    QRectF optionsButtonRect(QRectF within, Qt::LayoutDirection direction) const;
    QRectF errorsListRect(QRectF within, const QModelIndex &) const;
    qreal rootFolderHeightWithoutErrors() const;

private:
    static QString addFolderText(bool useSapces);

    // a workaround for a design flaw of the class
    // we need to know the actual font for most computations
    // the font is only set in paint and sizeHint
    void updateFont(const QFont &font);

    QFont _aliasFont;
    QFont _font;
    qreal _margin = 0;
    qreal _aliasMargin = 0;
    bool _ready = false;
};

} // namespace OCC
