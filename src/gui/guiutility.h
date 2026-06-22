/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GUIUTILITY_H
#define GUIUTILITY_H

#include <QString>
#include <QUrl>
#include <QWidget>

#include <functional>

#include "common/pinstate.h"

class QLabel;

namespace OCC {
namespace Utility {

    /** Open an url in the browser.
     *
     * If launching the browser fails, display a message.
     */
    bool openBrowser(const QUrl &url, QWidget *errorWidgetParent = nullptr);

    /** Start composing a new email message.
     *
     * If launching the email program fails, display a message.
     */
    bool openEmailComposer(const QString &subject, const QString &body,
        QWidget *errorWidgetParent);

    /** Returns a translated string indicating the current availability.
     *
     * This will be used in context menus to describe the current state.
     */
    QString vfsCurrentAvailabilityText(VfsItemAvailability availability);

    /** Translated text for "making items always available locally" */
    QString vfsPinActionText();

    /** Translated text for "free up local space" (and unpinning the item) */
    QString vfsFreeSpaceActionText();

    /** Ask before enabling an experimental virtual files mode. */
    void askExperimentalVirtualFilesFeature(QWidget *receiver, const std::function<void(bool enable)> &callback);

    /** Apply the standard inline error label styling used by setup dialogs. */
    void initErrorLabel(QLabel *errorLabel);

    /** Dim a hint label while preserving its current palette. */
    void customizeHintLabel(QLabel *label);

} // namespace Utility
} // namespace OCC

#endif
