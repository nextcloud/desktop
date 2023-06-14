/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "gui/nextcloudCore_autogen/include/ui_folderwizardsourcepage.h"
#include "gui/nextcloudCore_autogen/include/ui_folderwizardtargetpage.h"
#include "creds/abstractcredentials.h"
#include "folderwizard.h"
#include "accountstate.h"
#include "testhelper.h"
#include <QWizardPage>
#include "folderman.h"
#include "account.h"
#include <QtTest>

using namespace OCC;

class TestFolderWizard: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestFolderWizard)
#include "testfolderwizard.moc"
