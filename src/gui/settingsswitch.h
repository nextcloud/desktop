/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSSWITCH_H
#define SETTINGSSWITCH_H

#include <QCheckBox>
#include <QSize>

class QPaintEvent;

namespace OCC {

class SettingsSwitch : public QCheckBox
{
    Q_OBJECT

public:
    explicit SettingsSwitch(QWidget *parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

} // namespace OCC

#endif // SETTINGSSWITCH_H
