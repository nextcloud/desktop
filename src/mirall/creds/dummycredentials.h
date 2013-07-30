/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_DUMMY_CREDENTIALS_H
#define MIRALL_CREDS_DUMMY_CREDENTIALS_H

#include "mirall/creds/abstractcredentials.h"

namespace Mirall
{

class DummyCredentials : public AbstractCredentials
{
  Q_OBJECT

public:
  void syncContextPreInit(CSYNC* ctx);
  void syncContextPreStart(CSYNC* ctx);
  bool changed(AbstractCredentials* credentials) const;
  QString authType() const;
  QNetworkAccessManager* getQNAM() const;
  bool ready() const;
  void fetch();
  void persistForUrl(const QString& url);
};

} // ns Mirall

#endif
