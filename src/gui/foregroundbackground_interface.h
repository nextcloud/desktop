/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/**
* @brief ForegroundBackgroundInterface allows ForegroundBackground to be implemented differently per platform
* @ingroup gui
*/

#ifndef FOREGROUNDBACKGROUND_INTERFACE_H
#define FOREGROUNDBACKGROUND_INTERFACE_H

#include <QObject>
#include <QEvent>

namespace OCC {
namespace Ui {

class ForegroundBackground;

}
}

class ForegroundBackground : public QObject
{
    Q_OBJECT

public:

   ForegroundBackground() = default;
   ~ForegroundBackground() = default;

    /**
    * @brief EventFilter catches events that should trigger ForegroundBackground
    * @ingroup gui
    */
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    /**
    * @brief ToForeground() enables the macOS menubar and dock icon, which are necessary for a maximized window to be able to exit full screen.
    * @ingroup gui
    */
    void ToForeground();

    /**
    * @brief ToBackground() disables the macOS menubar and dock icon, so that the application will only be present as a menubar icon.
    * @ingroup gui
    */
    void ToBackground();
};

#endif