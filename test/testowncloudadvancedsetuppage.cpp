/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "gui/nextcloudCore_autogen/include/ui_owncloudadvancedsetuppage.h"
#include "gui/wizard/owncloudadvancedsetuppage.h"
#include "gui/wizard/owncloudwizard.h"

using namespace OCC;

class TestOwncloudAdvancedSetupPage: public QWidget
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestOwncloudAdvancedSetupPage)
#include "testowncloudadvancedsetuppage.moc"
