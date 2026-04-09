#pragma once

#include <QWizardPage>

#include "wizard/owncloudwizardcommon.h"

namespace OCC {

class OwncloudWizard;
    
namespace Ui {
    class DataProtectionSettingsPage;
}

class DataProtectionSettingsPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit DataProtectionSettingsPage(OwncloudWizard *ocWizard);
    ~DataProtectionSettingsPage() override;
    [[nodiscard]] int nextId() const override;
    void initializePage() override;

private:
    void setupUi();
    void customizeStyle();
    void setupPage();

    QScopedPointer<Ui::DataProtectionSettingsPage> _ui;

    OwncloudWizard *_ocWizard;

    WizardCommon::Pages _nextPage = WizardCommon::Page_AdvancedSetup;
};

}