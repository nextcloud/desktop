/*
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIRALL_CREDS_CREDENTIALS_FACTORY_H
#define MIRALL_CREDS_CREDENTIALS_FACTORY_H

#include "owncloudlib.h"

class QString;

namespace OCC {
class AbstractCredentials;


/**
 * @brief The HttpCredentialsGui namespace
 * @ingroup gui
 */
namespace CredentialsFactory {

    AbstractCredentials *create(const QString &type);

} // ns CredentialsFactory

} // namespace OCC

#endif
