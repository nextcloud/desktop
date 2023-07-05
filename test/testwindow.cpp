/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>

#include "tray/usermodel.h"
#include "systray.h"
#include "theme.h"

using namespace OCC;

class TestWindow: public QObject
{
    Q_OBJECT

public:

};

QTEST_MAIN(TestWindow)
#include "testwindow.moc"
