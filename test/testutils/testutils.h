#pragma once

#include "account.h"
#include "common/checksumalgorithms.h"
#include "folder.h"
#include "folderman.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>

namespace OCC {

namespace TestUtils {
    namespace TestUtilsPrivate {
        void accountStateDeleter(OCC::AccountState *acc);

        using AccountStateRaii = std::unique_ptr<AccountState, decltype(&TestUtilsPrivate::accountStateDeleter)>;
    }

    FolderMan *folderMan();
    FolderDefinition createDummyFolderDefinition(const AccountPtr &account, const QString &path);
    TestUtilsPrivate::AccountStateRaii createDummyAccount();
    bool writeRandomFile(const QString &fname, int size = -1);

    /***
     * Create a QTemporaryDir with a test specific name pattern
     * ownCloud-unit-test-{TestName}-XXXXXX
     * This allow to clean up after failed tests
     */
    QTemporaryDir createTempDir();

    const QVariantMap testCapabilities(CheckSums::Algorithm algo = CheckSums::Algorithm::DUMMY_FOR_TESTS);
}
}
