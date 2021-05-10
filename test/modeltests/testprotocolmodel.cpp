
/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include "gui/models/protocolitemmodel.h"

#include <QTest>
#include <QAbstractItemModelTester>

namespace OCC {

class TestProtocolModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInsertAndRemove()
    {
        auto model = new ProtocolItemModel(this, false);

        new QAbstractItemModelTester(model, this);

        // populate with dummy data
        // -1 to test the ring buffer window roll over
        const auto size = model->rawData().capacity() - 1;
        auto item = SyncFileItemPtr::create();
        std::vector<ProtocolItem> tmp;
        tmp.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            tmp.emplace_back(QStringLiteral("foo") + QString::number(i), item);
        }
        model->reset(std::move(tmp));

        // test some inserts
        for (int i = 0; i < 5; ++i) {
            model->addProtocolItem(ProtocolItem { QStringLiteral("bar") + QString::number(i), item });
        }

        const auto oldSize = model->rowCount();
        QCOMPARE(oldSize, model->rawData().capacity());
        // pick one from the middle
        const auto toBeRemoved = {
            model->protocolItem(model->index(0, 0)),
            model->protocolItem(model->index(model->rawData().capacity() / 2, 0)),
            model->protocolItem(model->index(model->rawData().capacity() / 3, 0))
        };

        std::vector<ProtocolItem> copy;
        copy.reserve(model->rowCount());
        for (int i = 0; i < model->rowCount(); ++i) {
            copy.push_back(model->protocolItem(model->index(i, 0)));
        }

        int matches = 0;
        const auto filter = [&toBeRemoved, &matches](const ProtocolItem &pi) {
            for (const auto &tb : toBeRemoved) {
                if (pi.folderName() == tb.folderName()) {
                    matches++;
                    return true;
                }
            }
            return false;
        };
        copy.erase(std::remove_if(copy.begin(), copy.end(), filter), copy.cend());
        QCOMPARE(matches, toBeRemoved.size());
        matches = 0;
        model->remove_if(filter);
        QCOMPARE(matches, toBeRemoved.size());
        QCOMPARE(oldSize - 3, model->rowCount());

        // ensure we kept the original order
        for (int i = 0; i < model->rowCount(); ++i) {
            QCOMPARE(model->protocolItem(model->index(i, 0)).folderName(), copy[i].folderName());
        }
    }
};
}

QTEST_GUILESS_MAIN(OCC::TestProtocolModel)
#include "testprotocolmodel.moc"
