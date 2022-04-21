#pragma once

#include <QObject>

namespace OCC::Wizard {
Q_NAMESPACE

enum class SyncMode {
    Invalid = 0,
    SyncEverything,
    ConfigureUsingFolderWizard,
    UseVfs,
};
Q_ENUM_NS(SyncMode)
}
