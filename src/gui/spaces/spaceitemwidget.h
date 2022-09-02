/*
 * Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "account.h"
#include "space.h"

#include <QWidget>


QT_BEGIN_NAMESPACE
namespace Ui {
class SpaceItemWidget;
}
QT_END_NAMESPACE

namespace OCC::Spaces {

class SpaceItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpaceItemWidget(const AccountPtr &accountPtr, const Space &space, QWidget *parent = nullptr);
    ~SpaceItemWidget() override;

    [[nodiscard]] const Space &space() const;

    void setRadioButtonChecked(bool checked);

Q_SIGNALS:
    void radioButtonClicked();

private:
    Ui::SpaceItemWidget *_ui;
    Space _space;
};

} // OCC::Spaces
