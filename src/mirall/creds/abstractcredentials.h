/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_CREDS_ABSTRACT_CREDENTIALS_H
#define MIRALL_CREDS_ABSTRACT_CREDENTIALS_H

#include <QObject>

#include <csync.h>

class QNetworkAccessManager;

namespace Mirall
{

class AbstractCredentials : public QObject
{
  Q_OBJECT

public:
  // No need for virtual destructor - QObject already has one.
  virtual void syncContextPreInit(CSYNC* ctx) = 0;
  virtual void syncContextPreStart(CSYNC* ctx) = 0;
  virtual bool changed(AbstractCredentials* credentials) const = 0;
  virtual QString authType() const = 0;
  virtual QNetworkAccessManager* getQNAM() const = 0;
  virtual bool ready() const = 0;
  virtual void fetch() = 0;
  virtual void persistForUrl(const QString& url) = 0;

Q_SIGNALS:
  void fetched();
};

} // ns Mirall

#endif
