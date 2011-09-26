#ifndef STATUSDIALOG_H
#define STATUSDIALOG_H

#include <QDialog>

#include "ui_statusdialog.h"
#include "application.h"

namespace Mirall {

class StatusDialog : public QDialog, public Ui::statusDialog
{
    Q_OBJECT
public:
    explicit StatusDialog(QWidget *parent = 0);
    void setFolderList( QHash<QString, Folder*> );

signals:

public slots:
    void slotRemoveFolder();
};
};

#endif // STATUSDIALOG_H
