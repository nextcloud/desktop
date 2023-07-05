/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QQuickView>
#include <QQmlEngine>
#include <QQuickItem>

#include "theme.h"

using namespace OCC;

class TestWelcome: public QObject
{
    Q_OBJECT

public slots:

};

QTEST_MAIN(TestWelcome)
#include "testwelcome.moc"
