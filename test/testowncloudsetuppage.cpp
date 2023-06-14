/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "gui/nextcloudCore_autogen/include/ui_owncloudsetupnocredspage.h"
#include "gui/wizard/owncloudsetuppage.h"

using namespace OCC;

class TestOwncloudSetupPage: public QWidget
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestOwncloudSetupPage)
#include "testowncloudsetuppage.moc"
