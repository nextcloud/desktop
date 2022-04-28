#pragma once

#include <QWidget>

namespace OCC::Wizard {

class AbstractSetupWizardPage : public QWidget
{
    Q_OBJECT

public:
    ~AbstractSetupWizardPage() override;

    /**
     * Check whether the user input appears valid, so that the user can be allowed to proceed through the wizard.
     * A minimal check for instance is to check whether all required line edits have got data.
     * In case a page does not have user input to be validated, this function may always return true.
     */
    virtual bool validateInput() = 0;

Q_SIGNALS:
    /**
     * Emitted after a page has been displayed within the wizard.
     * Can be used to, e.g., set the focus on widgets in order to make navigation with the keyboard easier.
     */
    void pageDisplayed();
};

}
