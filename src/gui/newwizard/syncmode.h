#pragma once

#include <QObject>

namespace OCC::Wizard {
Q_NAMESPACE

enum class SyncMode {
    Invalid = 0,
    SyncEverything,
    SelectiveSync,
    UseVfs,
};
Q_ENUM_NS(SyncMode)
}
