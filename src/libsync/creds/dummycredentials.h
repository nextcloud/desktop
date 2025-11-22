/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_CREDS_DUMMY_CREDENTIALS_H
#define MIRALL_CREDS_DUMMY_CREDENTIALS_H

#include "creds/abstractcredentials.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT DummyCredentials : public AbstractCredentials
{
    Q_OBJECT

public:
    QString _user;
    QString _password;
    [[nodiscard]] QString authType() const override;
    [[nodiscard]] QString user() const override;
    [[nodiscard]] QString password() const override;
    [[nodiscard]] QNetworkAccessManager *createQNAM() const override;
    [[nodiscard]] bool ready() const override;
    bool stillValid(QNetworkReply *reply) override;
    void fetchFromKeychain(const QString &appName = {}) override;
    void askFromUser() override;
    void persist() override;
    void invalidateToken() override {}
    void forgetSensitiveData() override{};
};

} // namespace OCC

#endif
