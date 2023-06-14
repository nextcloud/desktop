/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QQuickView>
#include <qqmlengine.h>
#include <QQuickItem>
#include <QtTest>
#include "theme.h"

using namespace OCC;

class TestWelcome: public QObject
{
    Q_OBJECT

public slots:

};

QTEST_MAIN(TestWelcome)
#include "testwelcome.moc"
