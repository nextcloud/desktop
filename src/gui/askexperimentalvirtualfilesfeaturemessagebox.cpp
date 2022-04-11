#include "askexperimentalvirtualfilesfeaturemessagebox.h"

namespace OCC {

AskExperimentalVirtualFilesFeatureMessageBox::AskExperimentalVirtualFilesFeatureMessageBox(QWidget *parent)
    : QMessageBox(QMessageBox::Warning,
        tr("Enable experimental feature?"),
        tr("When the \"virtual files\" mode is enabled no files will be downloaded initially. "
           "Instead, a tiny file will be created for each file that exists on the server. "
           "The contents can be downloaded by running these files or by using their context menu."
           "\n\n"
           "The virtual files mode is mutually exclusive with selective sync. "
           "Currently unselected folders will be translated to online-only folders "
           "and your selective sync settings will be reset."
           "\n\n"
           "Switching to this mode will abort any currently running synchronization."
           "\n\n"
           "This is a new, experimental mode. If you decide to use it, please report any "
           "issues that come up."),
        QMessageBox::NoButton,
        parent)
{
    this->addButton(tr("Enable experimental placeholder mode"), QMessageBox::AcceptRole);
    this->addButton(tr("Stay safe"), QMessageBox::RejectRole);

    this->setAttribute(Qt::WA_DeleteOnClose);
}

}
