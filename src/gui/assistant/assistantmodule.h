/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountfwd.h"

class QQmlEngine;
class QQuickWindow;

namespace OCC::Assistant
{

/** @brief Makes the Assistant QML resources available to the application. */
void initializeResources();

/**
 * @brief Creates an Assistant window for an account.
 * @param engine QML engine used to create the window.
 * @param accountState Account whose Assistant state is presented.
 * @return The created window, or nullptr when creation fails.
 */
[[nodiscard]] QQuickWindow *createWindow(QQmlEngine *engine, const AccountStatePtr &accountState);

}
