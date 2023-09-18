/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QPainter>

#include "folderstatusmodel.h"
#define private public
#include "folderstatusdelegate.h"
#undef private

using namespace OCC;

class TestFolderStatusDelegate: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestFolderStatusDelegate)
#include "testfolderstatusdelegate.moc"
