#include "accountconfiguredwizardpage.h"
#include <QMessageBox>

#include "gui/selectivesyncdialog.h"
#include "theme.h"
#include "ui_accountconfiguredwizardpage.h"

namespace OCC::Wizard {

AccountConfiguredWizardPage::AccountConfiguredWizardPage()
    : _ui(new ::Ui::AccountConfiguredWizardPage)
{
    _ui->setupUi(this);
}

AccountConfiguredWizardPage::~AccountConfiguredWizardPage() noexcept
{
    delete _ui;
}
}
