/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "localdiscoverytracker.h"
#include "folderstatusmodel.h"
#include "syncrunfilelog.h"
#include "syncengine.h"
#include "account.h"
#include <QtTest>

#define private public
#include "accountstate.h"
#include "syncresult.h"
#include "folder.h"
#include "theme.h"
#undef private

using namespace OCC;

class TestFolderStatusModel: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_GUILESS_MAIN(TestFolderStatusModel)
#include "testfolderstatusmodel.moc"
