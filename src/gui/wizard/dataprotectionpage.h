#pragma once

#include <QWizardPage>

#include "wizard/owncloudwizardcommon.h"

namespace OCC {

class OwncloudWizard;
    
namespace Ui {
    class DataProtectionPage;
}

class DataProtectionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit DataProtectionPage(OwncloudWizard *ocWizard);
    ~DataProtectionPage() override;
    [[nodiscard]] int nextId() const override;
    void initializePage() override;

private:
    void setupUi();
    void customizeStyle();

    QScopedPointer<Ui::DataProtectionPage> _ui;

    OwncloudWizard *_ocWizard;

    WizardCommon::Pages _nextPage = WizardCommon::Page_AdvancedSetup;
};

}