#pragma once

#include <QWidget>

namespace OCC::Wizard {

class AbstractSetupWizardPage : public QWidget
{
    Q_OBJECT

public:
    ~AbstractSetupWizardPage() override;

Q_SIGNALS:
    /**
     * Emitted after a page has been displayed within the wizard.
     * Can be used to, e.g., set the focus on widgets in order to make navigation with the keyboard easier.
     */
    void pageDisplayed();
};

}
