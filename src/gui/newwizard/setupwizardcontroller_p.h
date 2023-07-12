#pragma once

#include <QtGlobal>

namespace OCC::Wizard::SetupWizardControllerPrivate {

Q_NAMESPACE

enum class ChangeReason {
    Default,
    EvaluationFailed,
};
Q_ENUM_NS(ChangeReason)

}
