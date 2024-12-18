#include "syncdirvalidation.h"
#include <QDir>
#include <QStandardPaths>
#include "logger.h"

#ifdef Q_OS_WIN
bool SyncDirValidator::isValidDir() {
      QString appDataPath = SyncDirValidator::appDataPath();
      return !QDir::fromNativeSeparators(_path).startsWith(appDataPath) && !appDataPath.startsWith(QDir::fromNativeSeparators(_path));

}

QString SyncDirValidator::message() {
    return QObject::tr("The directory %1 cannot be part of your sync directory. Please choose another folder.").arg(_path);
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