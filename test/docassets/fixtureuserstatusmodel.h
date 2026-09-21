/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "userstatusselectormodel.h"
namespace OCC::DocAssets
{
// Only selects the existing production model's injectable constructor.
class FixtureUserStatusModel : public UserStatusSelectorModel
{
public:
    explicit FixtureUserStatusModel(QObject *parent = nullptr);
};
}
