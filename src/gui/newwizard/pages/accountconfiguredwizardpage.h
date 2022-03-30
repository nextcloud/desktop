#pragma once

#include <QSharedPointer>

#include "abstractsetupwizardpage.h"
#include "setupwizardcontroller.h"

namespace Ui {
class AccountConfiguredWizardPage;
}

namespace OCC::Wizard {

class AccountConfiguredWizardPage : public AbstractSetupWizardPage
{
    Q_OBJECT

public:
    explicit AccountConfiguredWizardPage(const QString &defaultSyncTargetDir, bool vfsIsAvailable, bool enableVfsByDefault, bool vfsModeIsExperimental);
    ~AccountConfiguredWizardPage() noexcept override;

    QString syncTargetDir() const;

    SyncMode syncMode() const;

private:
    ::Ui::AccountConfiguredWizardPage *_ui;
};

}
