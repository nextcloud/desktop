/*
 * SPDX-FileCopyrightText: 2013 ownCloud, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../../../src/libsync/utility.h"

#include <QDir>

int main(int argc, char* argv[])
{
   QString dir="/tmp/linktest/";
   QDir().mkpath(dir);
   OCC::Utility::setupFavLink(dir);
}
