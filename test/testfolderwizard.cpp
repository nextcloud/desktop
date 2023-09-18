/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QWizardPage>
#include <QtTest>

#include "creds/abstractcredentials.h"
#include "folderwizard.h"
#include "accountstate.h"
#include "testhelper.h"
#include "folderman.h"
#include "account.h"

using namespace OCC;

class TestFolderWizard: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestFolderWizard)
#include "testfolderwizard.moc"
