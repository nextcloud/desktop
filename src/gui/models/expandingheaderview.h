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
#pragma once

#include <QHeaderView>

namespace OCC {

class ExpandingHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    ExpandingHeaderView(const QString &objectName, QWidget *parent = nullptr);
    ~ExpandingHeaderView();

    int expandingColumn() const;
    void setExpandingColumn(int newExpandingColumn);

    void resizeColumns(bool reset = false);
    void addResetActionToMenu(QMenu *menu);

    bool resizeToContent() const;
    void setResizeToContent(bool newResizeToContent);

protected:
    void resizeEvent(QResizeEvent *event) override;


private:
    bool _requiresReset = false;
    bool _resizeToContent = false;
    int _expandingColumn = 0;
};

}
