#include "syncdirvalidation.h"
#include <QDir>
#include <QStandardPaths>
#include "logger.h"

#ifdef Q_OS_WIN
bool SyncDirValidator::isValidDir() {
      QString appDataPath = SyncDirValidator::appDataPath().replace("/", QDir::separator());
      QStringList pathComponents = _path.replace("/", QDir::separator()).split(QDir::separator(), QString::SkipEmptyParts);
      QStringList appDataPathComponents = appDataPath.split(QDir::separator(), QString::SkipEmptyParts);
      /*
        If path is shorter than appDataPath and one path component is different, then path cannot be a real subset and is sowith valid
        If appDataPath is shorter than path, we need to check, if the last appDataPath component is different from the related path component, then path is valid.
        Otherwise path is a subpath from appDataPath and invalid
      */
      for(int i = 0; i < qMin(pathComponents.size(), appDataPathComponents.size()); i++) {
          if(pathComponents[i] != appDataPathComponents[i]) {
              return true;
          }
      }
      return false;

}

QString SyncDirValidator::message() {
    return QObject::tr("The directory %1 cannot be part of your sync directory. Please choose another folder.").arg(_path.replace("/", QDir::separator()));
}

QString SyncDirValidator::appDataPath() {
  //Path: AppData/Roaming/<ApplicationName>
  QString appDataRoamingApplicationNamePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir appDataRoamingApplicationNameDir(appDataRoamingApplicationNamePath);
  appDataRoamingApplicationNameDir.cdUp();
  appDataRoamingApplicationNameDir.cdUp();
  QString appDataPath = appDataRoamingApplicationNameDir.absolutePath();
  return appDataPath;
}

#else

bool SyncDirValidator::isValidDir() {
      return true;
}

QString SyncDirValidator::message() {
    return "";
}

QString SyncDirValidator::appDataPath() {
  return "";
}

#endif