#include "settingsdialogmac.h"

#include "macstandardicon.h"

#include "folderman.h"
#include "theme.h"
#include "generalsettings.h"
#include "networksettings.h"
#include "accountsettings.h"
#include "mirallconfigfile.h"
#include "progressdispatcher.h"
#include "owncloudgui.h"
#include "protocolwidget.h"

#include <QLabel>
#include <QStandardItemModel>
#include <QPushButton>
#include <QDebug>
#include <QSettings>

namespace Mirall {

SettingsDialogMac::SettingsDialogMac(ownCloudGui *gui, QWidget *parent)
    : MacPreferencesWindow(parent)
{
    // do not show minimize button. There is no use, and retoring the
    // dialog from minimize is broken in MacPreferencesWindow
    setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint |
                   Qt::WindowCloseButtonHint | Qt::WindowMaximizeButtonHint);


    // Emulate dialog behavior: Escape means close
    QAction *closeWindowAction = new QAction(this);
    closeWindowAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(closeWindowAction, SIGNAL(triggered()), SLOT(close()));
    addAction(closeWindowAction);

    setObjectName("SettingsMac"); // required as group for saveGeometry call

    setWindowTitle(tr("%1").arg(Theme::instance()->appNameGUI()));

    _accountSettings = new AccountSettings;
    QIcon icon = Theme::instance()->syncStateIcon(SyncResult::Undefined, true);
    _accountIdx = addPreferencesPanel(icon, tr("Account"), _accountSettings);

    QIcon protocolIcon(QLatin1String(":/mirall/resources/activity.png"));
    _protocolWidget = new ProtocolWidget;
    _protocolIdx = addPreferencesPanel(protocolIcon, tr("Activity"), _protocolWidget);

    QIcon generalIcon = MacStandardIcon::icon(MacStandardIcon::PreferencesGeneral);
    GeneralSettings *generalSettings = new GeneralSettings;
    addPreferencesPanel(generalIcon, tr("General"), generalSettings);

    QIcon networkIcon = MacStandardIcon::icon(MacStandardIcon::Network);
    NetworkSettings *networkSettings = new NetworkSettings;
    addPreferencesPanel(networkIcon, tr("Network"), networkSettings);

    FolderMan *folderMan = FolderMan::instance();
    connect( folderMan, SIGNAL(folderSyncStateChange(QString)),
             this, SLOT(slotSyncStateChange(QString)));

    connect( ProgressDispatcher::instance(), SIGNAL(progressInfo(QString, Progress::Info)),
             _accountSettings, SLOT(slotSetProgress(QString, Progress::Info)) );

    QAction *showLogWindow = new QAction(this);
    showLogWindow->setShortcut(QKeySequence("F12"));
    connect(showLogWindow, SIGNAL(triggered()), gui, SLOT(slotToggleLogBrowser()));
    addAction(showLogWindow);

    MirallConfigFile cfg;
    cfg.restoreGeometry(this);
}

void SettingsDialogMac::slotSyncStateChange(const QString& alias)
{
    FolderMan *folderMan = FolderMan::instance();
    SyncResult state = folderMan->accountStatus(folderMan->map().values());
    QIcon accountIcon = Theme::instance()->syncStateIcon(state.status());
    setPreferencesPanelIcon(_accountIdx, accountIcon);

    Folder *folder = folderMan->folder(alias);
    if( folder ) {
        _accountSettings->slotUpdateFolderState(folder);
    }
}

void SettingsDialogMac::setGeneralErrors(const QStringList &errors)
{
    if( _accountSettings ) {
        _accountSettings->setGeneralErrors(errors);
    }
}

void SettingsDialogMac::closeEvent(QCloseEvent *event)
{
    MirallConfigFile cfg;
    cfg.saveGeometry(this);
    MacPreferencesWindow::closeEvent(event);
}

void SettingsDialogMac::showActivityPage()
{
    setCurrentPanelIndex(_protocolIdx);
}

}
