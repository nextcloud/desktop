
/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "gui/protocolitemmodel.h"

#include <QTest>
#include <QAbstractItemModelTester>

namespace OCC {

class TestProtocolModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInsertAndRemove()
    {
        // no need to test with 20000 lines
        const auto TestBacklogSize = 111;
        auto model = new ProtocolItemModel(this, false, TestBacklogSize);

        new QAbstractItemModelTester(model, this);

        // populate with dummy data
        auto item = SyncFileItemPtr::create();
        for (int i = 0; i < TestBacklogSize * 1.1; ++i) {
            model->addProtocolItem({ ProtocolItem(QStringLiteral("foo") + QString::number(i), item) });
        }

        const auto oldSize = model->rowCount();
        QCOMPARE(oldSize, TestBacklogSize);
        // pick one from the middle
        const auto toBeRemoved = model->protocolItem(model->index(TestBacklogSize / 2, 0));
        model->remove([&toBeRemoved](const ProtocolItem &pi) {
            return pi.folderName() == toBeRemoved.folderName();
        });
        QCOMPARE(oldSize - 1, model->rowCount());
    }
};
}

QTEST_GUILESS_MAIN(OCC::TestProtocolModel)
#include "testprotocolmodel.moc"
