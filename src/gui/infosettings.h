/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef INFOSETTINGS_H
#define INFOSETTINGS_H

#include "config.h"

#include <QStringList>
#include <QWidget>

namespace OCC {

namespace Ui {
    class InfoSettings;
}

class InfoSettings : public QWidget
{
    Q_OBJECT

public:
    explicit InfoSettings(QWidget *parent = nullptr);
    ~InfoSettings() override;
    [[nodiscard]] QSize sizeHint() const override;

public slots:
    void slotStyleChanged();
#if defined(BUILD_UPDATER)
    void loadUpdateChannelsList();
    [[nodiscard]] QString updateChannelToLocalized(const QString &channel) const;
    void setAndCheckNewUpdateChannel(const QString &newChannel);
    void restoreUpdateChannel();
#endif

private slots:
    void slotShowLegalNotice();
#if defined(BUILD_UPDATER)
    void slotUpdateInfo();
    void slotUpdateChannelChanged();
    void slotUpdateCheckNow();
    void slotToggleAutoUpdateCheck();
    void slotRestoreUpdateChannel();
#endif

private:
    void customizeStyle();

    Ui::InfoSettings *_ui;
    QStringList _currentUpdateChannelList;
};

} // namespace OCC

#endif // INFOSETTINGS_H
