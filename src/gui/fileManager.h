#ifndef FILEMANAGER_H
#define FILEMANAGER_H
#include <QtCore>

class FileManager
{
public:
    static const QString FMFileType;
    static const QString FMFileTypeDirectory;
    static const QString FMFileTypeRegular;
    static const QString FMFileTypeSymbolicLink;
    static const QString FMFileTypeSocket;
    static const QString FMFileTypeCharacterSpecial;
    static const QString FMFileTypeBlockSpecial;
    static const QString FMFileTypeUnknown;
    static const QString FMFileSize;
    static const QString FMFileModificationDate;
    static const QString FMFileReferenceCount;
    static const QString FMFileDeviceIdentifier;
    static const QString FMFileOwnerAccountName;
    static const QString FMFileGroupOwnerAccountName;
    static const QString FMFilePosixPermissions;
    static const QString FMFileSystemNumber;
    static const QString FMFileSystemFileNumber;
    static const QString FMFileExtensionHidden;
    static const QString FMFileHFSCreatorCode;
    static const QString FMFileHFSTypeCode;
    static const QString FMFileImmutable;
    static const QString FMFileAppendOnly;
    static const QString FMFileCreationDate;
    static const QString FMFileOwnerAccountID;
    static const QString FMFileGroupOwnerAccountID;
    static const QString FMFileBusy;
    static const QString FMFileProtectionKey;
    static const QString FMFileProtectionNone;
    static const QString FMFileProtectionComplete;
    static const QString FMFileProtectionCompleteUnlessOpen;
    static const QString FMFileProtectionCompleteUntilFirstUserAuthentication;
    
    static const QString FMFileSystemSize;
    static const QString FMFileSystemFreeSize;
    static const QString FMFileSystemNodes;
    static const QString FMFileSystemFreeNodes;
    
    static const QString FMPOSIXErrorDomain;
    
    FileManager(){}
    QVariantMap* attributesOfItemAtPath(QString path, QVariantMap &error);
    bool createDirectory(QString path, QVariantMap attributes, QVariantMap &errorcode);
    bool createFileAtPath(QString path, QVariantMap attributes, int flags, QVariant &userData, QVariantMap &error);
    bool createFileAtPath(QString path, QVariantMap attributes, QVariant &userData, QVariantMap &error);
    bool removeItemAtPath(QString path, QVariantMap &error);
    bool createSymbolicLinkAtPath(QString path, QString otherPath, QVariantMap &error);
    QString destinationOfSymbolicLinkAtPath(QString path, QVariantMap &error);
    QVariantMap attributesOfFileSystemForPath(QString path, QVariantMap &error);
    bool setAttributes(QVariantMap attributes, QString path, QVariantMap &error);
    QStringList contentsOfDirectoryAtPath (QString path, QVariantMap &error);
    
};

#endif