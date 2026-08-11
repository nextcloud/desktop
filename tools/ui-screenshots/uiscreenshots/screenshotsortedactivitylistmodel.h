/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SCREENSHOTSORTEDACTIVITYLISTMODEL_H
#define SCREENSHOTSORTEDACTIVITYLISTMODEL_H

#include <QIdentityProxyModel>

namespace OCC {

/** @brief Preserves deterministic fixture order behind the production proxy-model QML name. */
class ScreenshotSortedActivityListModel : public QIdentityProxyModel
{
    Q_OBJECT

public:
    using QIdentityProxyModel::QIdentityProxyModel;

private:
    Q_DISABLE_COPY_MOVE(ScreenshotSortedActivityListModel)
};

}

#endif // SCREENSHOTSORTEDACTIVITYLISTMODEL_H
