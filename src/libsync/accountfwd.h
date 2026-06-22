/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SERVERFWD_H
#define SERVERFWD_H

#include <QSharedPointer>

namespace OCC {

class Account;
using AccountPtr = QSharedPointer<Account>;
class AccountState;
using AccountStatePtr = QExplicitlySharedDataPointer<AccountState>;

} // namespace OCC

#endif //SERVERFWD
