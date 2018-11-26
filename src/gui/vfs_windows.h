/*
* Copyright(C) 2018 by AMCO
* Copyright(C) 2018 by Jonathan Ponciano <jponcianovera@ciencias.unam.mx>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
*(at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE.See the GNU General Public License
* for more details.
*/

#ifndef VFS_WINDOWS_H
#define VFS_WINDOWS_H

#include "../../dokanLib/dokan.h"
#include "../../dokanLib/fileinfo.h"

#include "discoveryphase.h"
#include "accountstate.h"
#include "configfile.h"

#include <QMutex>
#include <QWaitCondition>
#include "syncwrapper.h"

namespace OCC {

class Vfs_windows : public QObject
{
    Q_OBJECT
public:
	static Vfs_windows* instance();

	Vfs_windows(AccountState *);
	~Vfs_windows();
	void upDrive(QString, QString);
	void downDrive(WCHAR DriveLetter);

	void DsetTotalNumberOfBytes(unsigned long long n);// { m_TotalNumberOfBytes = n; }
	unsigned long long DgetTotalNumberOfBytes();// { return m_TotalNumberOfBytes; }

	void DsetTotalNumberOfFreeBytes(unsigned long long n);// { m_TotalNumberOfFreeBytes = n; }
	unsigned long long DgetTotalNumberOfFreeBytes();// { return m_TotalNumberOfFreeBytes; }

	QStringList* contentsOfDirectoryAtPath(QString path, QVariantMap &error);
    void openFileAtPath(QString path, QVariantMap &error);
    void deleteFileAtPath(QString path, QVariantMap &error);
    void startDeleteDirectoryAtPath(QString path, QVariantMap &error);
    void endDeleteDirectoryAtPath(QString path, QVariantMap &error);

private:
	static Vfs_windows *_instance;
	QMap<QString, OCC::DiscoveryDirectoryResult*> _fileListMap;
	QPointer<OCC::DiscoveryFolderFileList> _remotefileListJob;
	QString rootPath_;

	// To sync
	OCC::SyncWrapper *_syncWrapper;
    QMutex _mutex;
    QWaitCondition _syncCondition;
    QWaitCondition _dirCondition;

signals:
	void startRemoteFileListJob(QString path);

    void getOperationCreateFile(QString, QString, QString);
	void getOperationCleanup(QString, QString, QString);
	void getOperationCloseFile(QString, QString, QString);
	void getOperationReadFile(QString, QString, QString);
	void getOperationWriteFile(QString, QString, QString);
	void getOperationFlushFileBuffers(QString, QString, QString);
	void getOperationGetFileInformation(QString, QString, QString);
	void getOperationFindFiles(QString, QString, QString);
	void getOperationFindFilesWithPattern(QString, QString, QString);
	void getOperationSetFileAttributes(QString, QString, QString);
	void getOperationSetFileTime(QString, QString, QString);
	void getOperationDeleteFile(QString, QString, QString);
	void getOperationDeleteDirectory(QString, QString, QString);
	void getOperationMoveFile(QString, QString, QString);
	void getOperationSetEndOfFile(QString, QString, QString);
	void getOperationSetAllocationSize(QString, QString, QString);
	void getOperationLockFile(QString, QString, QString);
	void getOperationUnlockFile(QString, QString, QString);
	void getOperationGetFileSecurity(QString, QString, QString);
	void getOperationSetFileSecurity(QString, QString, QString);
	void getOperationGetDiskFreeSpace(QString, QString, QString);
	void getOperationGetVolumeInformation(QString, QString, QString);
	void getOperationFindStreams(QString, QString, QString);
		//< void getOperationUmounted(QString, QString, QString);
		//< void getOperationMounted(QString, QString, QString);

	// To sync: propagate FUSE operations to the sync engine
    void openFile(const QString path);
    void releaseFile(const QString path);
    void writeFile(const QString path);
    void deleteFile(const QString path);
    void addToFileTree(const QString path);

private slots:

//< Examples catch signals ...
	void slotCatchOperationCreateFile(QString, QString, QString);
	void slotCatchOperationCleanup(QString, QString, QString);
	void slotCatchOperationCloseFile(QString, QString, QString);
	void slotCatchOperationReadFile(QString, QString, QString);
	void slotCatchOperationWriteFile(QString, QString, QString);
	void slotCatchOperationFlushFileBuffers(QString, QString, QString);
	void slotCatchOperationGetFileInformation(QString, QString, QString);
	void slotCatchOperationFindFiles(QString, QString, QString);
	void slotCatchOperationFindFilesWithPattern(QString, QString, QString);
	void slotCatchOperationSetFileAttributes(QString, QString, QString);
	void slotCatchOperationSetFileTime(QString, QString, QString);
	void slotCatchOperationDeleteFile(QString, QString, QString);
	void slotCatchOperationDeleteDirectory(QString, QString, QString);
	void slotCatchOperationMoveFile(QString, QString, QString);
	void slotCatchOperationSetEndOfFile(QString, QString, QString);
	void slotCatchOperationSetAllocationSize(QString, QString, QString);
	void slotCatchOperationLockFile(QString, QString, QString);
	void slotCatchOperationUnlockFile(QString, QString, QString);
	void slotCatchOperationGetFileSecurity(QString, QString, QString);
	void slotCatchOperationSetFileSecurity(QString, QString, QString);
	void slotCatchOperationGetDiskFreeSpace(QString, QString, QString);
	void slotCatchOperationGetVolumeInformation(QString, QString, QString);
	void slotCatchOperationFindStreams(QString, QString, QString);

public slots:
	void folderFileListFinish(OCC::DiscoveryDirectoryResult *dr);

	// To sync: notify syncing is done
    void slotSyncFinish(const QString &path, bool status);
};

} // namespace OCC

#endif // VFS_WINDOWS_H

