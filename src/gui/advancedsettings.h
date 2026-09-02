/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ADVANCEDSETTINGS_H
#define ADVANCEDSETTINGS_H

#include <QPointer>
#include <QWidget>

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
    void slotIgnoreFilesEditor();

private Q_SLOTS:
    void saveMiscSettings();
    void slotShowInExplorerNavigationPane(bool checked);
    void slotCreateDebugArchive();
    void loadMiscSettings();
    void slotRemotePollIntervalChanged(int seconds);
    void updatePollIntervalVisibility();

private:
    void customizeStyle();

    Ui::AdvancedSettings *_ui;
    QPointer<IgnoreListEditor> _ignoreEditor;
    bool _currentlyLoading = false;
};

} // namespace OCC

#endif // ADVANCEDSETTINGS_H
