/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "localdiscoverytracker.h"
#include "syncrunfilelog.h"
#include "syncengine.h"
#include <QtTest>

#include "account.h"
#define private public
#include "accountstate.h"
#include "folder.h"
#include "gui/tray/UserModel.h"
#undef private

using namespace OCC;

class TestUserModel: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestUserModel)
#include "testusermodel.moc"
