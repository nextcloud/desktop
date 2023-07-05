/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QToolBar>
#include <QtTest>

#include "ui_settingsdialog.h"
#include "settingsdialog.h"

using namespace OCC;

class TestSettingsDialog: public QDialog
{
    Q_OBJECT

private slots:

};

QTEST_MAIN(TestSettingsDialog)
#include "testsettingsdialog.moc"
