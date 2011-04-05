#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>
#include <QHash>

class QAction;
class QSystemTrayIcon;
class QNetworkConfigurationManager;

namespace Mirall {

class Folder;
class FolderWizard;

class Application : public QApplication
{
    Q_OBJECT
public:
    explicit Application(int argc, char **argv);
    ~Application();
signals:

protected slots:

    void slotAddFolder();

    void slotFolderSyncStarted();
    void slotFolderSyncFinished();

protected:

    QString folderConfigPath() const;

    void setupActions();
    void setupSystemTray();
    void setupContextMenu();

    // finds all folder configuration files
    // and create the folders
    void setupKnownFolders();

    // creates a folder for a specific
    // configuration
    void setupFolderFromConfigFile(const QString &filename);

private:
    // configuration file -> folder
    QHash<QString, Folder *> _folderMap;
    QSystemTrayIcon *_tray;
    QAction *_actionQuit;
    QAction *_actionAddFolder;
    QNetworkConfigurationManager *_networkMgr;
    QString _folderConfigPath;

    // counter tracking number of folders doing a sync
    int _folderSyncCount;

    FolderWizard *_folderWizard;
};

} // namespace Mirall

#endif // APPLICATION_H
