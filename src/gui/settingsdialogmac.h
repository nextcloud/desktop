#ifndef SETTINGSDIALOGMAC_H
#define SETTINGSDIALOGMAC_H

#include "progressdispatcher.h"
#include "macpreferenceswindow.h"

class QStandardItemModel;
class QListWidgetItem;

namespace Mirall {

class AccountSettings;
class ProtocolWidget;
class Application;
class FolderMan;
class ownCloudGui;

class SettingsDialogMac : public MacPreferencesWindow
{
    Q_OBJECT

public:
    explicit SettingsDialogMac(ownCloudGui *gui, QWidget *parent = 0);

    void setGeneralErrors( const QStringList& errors );

public slots:
    void slotSyncStateChange(const QString& alias);
    void showActivityPage();

private:
    void closeEvent(QCloseEvent *event);

    AccountSettings *_accountSettings;
    QListWidgetItem *_accountItem;
    ProtocolWidget  *_protocolWidget;

    int _accountIdx;
    int _protocolIdx;
};

}

#endif // SETTINGSDIALOGMAC_H
