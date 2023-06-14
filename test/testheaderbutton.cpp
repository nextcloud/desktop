/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <qqmlengine.h>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>
#include "theme.h"

using namespace OCC;

class TestHeaderButton: public QObject
{
    Q_OBJECT

public:

private slots:

};

QTEST_MAIN(TestHeaderButton)
#include "testheaderbutton.moc"
