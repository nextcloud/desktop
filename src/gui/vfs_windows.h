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
#include <QString>

namespace OCC {

class Vfs_windows : public QObject
{
    Q_OBJECT
public:
	QLinHeaderList _syncFiles;
	static Vfs_windows* instance();

	Vfs_windows(AccountState *);
	~Vfs_windows();
	void upDrive(QString, QString);
	void downDrive(WCHAR DriveLetter);
	bool removeDir(const QString &);

	void DsetTotalNumberOfBytes(unsigned long long n);
	unsigned long long DgetTotalNumberOfBytes();

	void DsetTotalNumberOfFreeBytes(unsigned long long n);
	unsigned long long DgetTotalNumberOfFreeBytes();

	QStringList* contentsOfDirectoryAtPath(QString path, QVariantMap &error);

	void createFileAtPath(QString path, QVariantMap &error);
	void moveFileAtPath(QString path, QString npath,QVariantMap &error);
	void createDirectoryAtPath(QString path, QVariantMap &error);
	void moveDirectoryAtPath(QString path, QString npath, QVariantMap &error);

	void openFileAtPath(QString path, QVariantMap &error);
	void writeFileAtPath(QString path, QVariantMap &error);
	void deleteFileAtPath(QString path, QVariantMap &error);
	void startDeleteDirectoryAtPath(QString path, QVariantMap &error);
	void endDeleteDirectoryAtPath(QString path, QVariantMap &error);

private:
	static Vfs_windows *_instance;
	QMap<QString, OCC::DiscoveryDirectoryResult*> _fileListMap;
	QPointer<OCC::DiscoveryFolderFileList> _remotefileListJob;
	QString rootPath_;

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
public slots:
	void folderFileListFinish(OCC::DiscoveryDirectoryResult *dr);
};

} // namespace OCC

#endif // VFS_WINDOWS_H

