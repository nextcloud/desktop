/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSSWITCH_H
#define SETTINGSSWITCH_H

#include <QAbstractButton>
#include <QSize>

class QPaintEvent;

namespace OCC {

class SettingsSwitch : public QAbstractButton
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
