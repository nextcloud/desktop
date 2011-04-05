
#include "mirall/folderwizard.h"

namespace Mirall
{

FolderWizard::FolderWizard(QWidget *parent)
    : QWizard(parent)
{
    _ui.setupUi(this);
}

}

#include "folderwizard.moc"
