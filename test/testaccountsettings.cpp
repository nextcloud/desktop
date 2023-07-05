/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>

#include "ui_accountsettings.h"
#include "accountsettings.h"

using namespace OCC;

class TestAccountSettings: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestAccountSettings)
#include "testaccountsettings.moc"
