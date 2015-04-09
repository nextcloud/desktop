/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include "account.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT AccountManager : public QObject {
    Q_OBJECT
public:
    static AccountManager *instance();
    ~AccountManager() {}

    void setAccount(AccountPtr account);
    AccountPtr account() { return _account; }

    /**
     * Saves the account to a given settings file
     */
    void save();

    /**
     * Creates an account object from from a given settings file.
     * return true if the account was restored
     */
    bool restore();

Q_SIGNALS:
    void accountAdded(AccountPtr account);
    void accountRemoved(AccountPtr account);

private:
    AccountManager() {}
    AccountPtr _account;
};

}