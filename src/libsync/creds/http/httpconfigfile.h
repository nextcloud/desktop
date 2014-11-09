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

#ifndef MIRALL_CREDS_HTTP_CONFIG_FILE_H
#define MIRALL_CREDS_HTTP_CONFIG_FILE_H

#include "configfile.h"

namespace OCC
{

class HttpConfigFile : public ConfigFile
{
public:
  QString user() const;
  void setUser(const QString& user);

  QString password() const;
  void setPassword(const QString& password);
  bool passwordExists() const;
  void removePassword();
  void fixupOldPassword();

private:
  void removeOldPassword();
};

} // namespace OCC

#endif
