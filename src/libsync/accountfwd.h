/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef SERVERFWD_H
#define SERVERFWD_H

#include <QSharedPointer>

namespace OCC {

class Account;
typedef QSharedPointer<Account> AccountPtr;
class AccountState;
typedef QExplicitlySharedDataPointer<AccountState> AccountStatePtr;

} // namespace OCC

#endif //SERVERFWD
