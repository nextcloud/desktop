/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NAVIGATIONPANEHELPER_H
#define NAVIGATIONPANEHELPER_H

#include <QObject>
#include <QTimer>

namespace OCC {

class FolderMan;

class NavigationPaneHelper : public QObject
{
    Q_OBJECT
public:
    NavigationPaneHelper(FolderMan *folderMan);

    [[nodiscard]] bool showInExplorerNavigationPane() const { return _showInExplorerNavigationPane; }
    void setShowInExplorerNavigationPane(bool show);

    void scheduleUpdateCloudStorageRegistry();

private:
    void updateCloudStorageRegistry();

    FolderMan *_folderMan;
    bool _showInExplorerNavigationPane;
    QTimer _updateCloudStorageRegistryTimer;
};

} // namespace OCC
#endif // NAVIGATIONPANEHELPER_H
