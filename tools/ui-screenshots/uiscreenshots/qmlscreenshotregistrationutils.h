/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QMLSCREENSHOTREGISTRATIONUTILS_H
#define QMLSCREENSHOTREGISTRATIONUTILS_H

class QQmlApplicationEngine;

namespace OCC {
class ScreenshotSystray;
class ScreenshotUserModel;

namespace UiScreenshots {

/**
 * @brief Registers the minimal production and deterministic QML type surface.
 * @param userModel Screenshot singleton registered under the production `UserModel` name.
 * @param systray Screenshot singleton registered under the production `Systray` name.
 */
void registerQmlTypes(ScreenshotUserModel &userModel, ScreenshotSystray &systray);

/**
 * @brief Configures production import paths and image providers on @p engine.
 * @param engine The single engine used for all screenshot jobs.
 */
void configureQmlEngine(QQmlApplicationEngine &engine);

}
}

#endif // QMLSCREENSHOTREGISTRATIONUTILS_H
