
#ifndef MIRALL_TEST_UNISONFOLDER_H
#define MIRALL_TEST_UNISONFOLDER_H

#include <QtTest/QtTest>

class TestUnisonFolder : public QObject
{
    Q_OBJECT
public:

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testSyncFiles();

private:
};

#endif
