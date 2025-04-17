/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QPaintDevice>
#include <QTest>

class FakePaintDevice : public QPaintDevice
{
public:
    FakePaintDevice();

    [[nodiscard]] QPaintEngine *paintEngine() const override;

    void setHidpi(bool value);

protected:
    [[nodiscard]] int metric(QPaintDevice::PaintDeviceMetric metric) const override;

private:
    bool _hidpi = false;
};
