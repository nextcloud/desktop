#pragma once

#include "account.h"
#include "common/checksumalgorithms.h"
#include "folder.h"
#include "folderman.h"

#include <QJsonArray>
#include <QJsonObject>

namespace OCC {

namespace TestUtils {
    FolderMan *folderMan();
    FolderDefinition createDummyFolderDefinition(const AccountPtr &account, const QString &path);
    AccountPtr createDummyAccount();
    bool writeRandomFile(const QString &fname, int size = -1);


    const QVariantMap testCapabilities(CheckSums::Algorithm algo = CheckSums::Algorithm::ADLER32);
}
}
