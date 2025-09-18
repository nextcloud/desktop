/*
 * SPDX-FileCopyrightText: 2015 ownCloud, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QCoreApplication>

#include "httpserver.h"

int main(int argc, char* argv[])
{
  QCoreApplication app(argc, argv);
  HttpServer server;
  return app.exec();
}
