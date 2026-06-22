/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_NETWORKSETTINGS_H
#define MIRALL_NETWORKSETTINGS_H

#include <QWidget>

#include "libsync/accountfwd.h"

namespace OCC {
class Account;

namespace Ui {
    class NetworkSettings;
}

/**
 * @brief The NetworkSettings class
 * @ingroup gui
 */
class NetworkSettings : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkSettings(const AccountPtr &account = {}, QWidget *parent = nullptr);
    ~NetworkSettings() override;
    [[nodiscard]] QSize sizeHint() const override;

private slots:
    void saveProxySettings();
    void saveBWLimitSettings();

    /// Red marking of host field if empty and enabled
    void checkEmptyProxyHost();

    void checkAccountLocalhost();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void loadProxySettings();
    void loadBWLimitSettings();

    Ui::NetworkSettings *_ui;
    AccountPtr _account;
};


} // namespace OCC
#endif // MIRALL_NETWORKSETTINGS_H
