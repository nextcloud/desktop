/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QPointer>
#include <QWidget>

#include <functional>

namespace OCC {

class IgnoreListEditor;

namespace Ui {
    class AdvancedSettings;
}

class AdvancedSettings : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedSettings(QWidget *parent = nullptr);
    ~AdvancedSettings() override;
    [[nodiscard]] QSize sizeHint() const override;

public Q_SLOTS:
    void slotStyleChanged();

private Q_SLOTS:
    void saveMiscSettings();
    void slotShowInExplorerNavigationPane(bool checked);
    void slotIgnoreFilesEditor();
    void slotCreateDebugArchive();
    void loadMiscSettings();
    void slotRemotePollIntervalChanged(int seconds);
    void updatePollIntervalVisibility();

private:
    using DebugArchiveLocationPicker = std::function<void(std::function<void(const QString &)>)>;

    friend class AdvancedSettingsTestAccess;

    void customizeStyle();
    void pickDebugArchiveLocation(std::function<void(const QString &)> completion);

    Ui::AdvancedSettings *_ui;
    DebugArchiveLocationPicker _debugArchiveLocationPicker;
    QPointer<IgnoreListEditor> _ignoreEditor;
    bool _currentlyLoading = false;
};

} // namespace OCC

#endif // ADVANCEDSETTINGS_H
