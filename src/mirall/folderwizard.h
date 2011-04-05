
#ifndef MIRALL_FOLDERWIZARD_H
#define MIRALL_FOLDERWIZARD_H

#include "ui_folderwizard.h"
#include <QWizard>

namespace Mirall {

class FolderWizard : public QWizard
{
    Q_OBJECT
public:
    FolderWizard(QWidget *parent);
private:
    Ui_FolderWizard _ui;
};


} // ns Mirall

#endif
