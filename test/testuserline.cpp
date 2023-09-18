/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>

#include "theme.h"

using namespace OCC;

class TestUserLine: public QObject
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestUserLine)
#include "testuserline.moc"
