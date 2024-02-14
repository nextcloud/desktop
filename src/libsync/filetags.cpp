/*
 * Copyright (C) 2024 by Uwe Runtemund <uwe.runtemund@jameo.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __linux__
#include <sys/xattr.h>
#endif

#include <QStringList>
#include <QXmlStreamReader>

#include "common/syncjournalfilerecord.h"
#include "discoveryphase.h"
#include "filetags.h"

namespace OCC{

Q_LOGGING_CATEGORY(lcFTM, "nextcloud.sync.filetags", QtInfoMsg)

FileTagManager* gFileTagManger=NULL;
	
FileTagManager::FileTagManager():QObject(NULL)
{}

FileTagManager* FileTagManager::GetInstance()
{
    if(gFileTagManger==NULL)gFileTagManger=new FileTagManager();
    return gFileTagManger;
}

void FileTagManager::fromPropertiesToTagList(QByteArray &list,const QString &properties)
{
    if(properties==NULL || properties.isEmpty()) return;

    QStringList tags;
    QString token;
    QXmlStreamReader reader("<system-tags>"%properties%"</system-tags>");
    bool insideTag = false;

    while (!reader.atEnd()){
        QXmlStreamReader::TokenType type = reader.readNext();
        QString name = reader.name().toString();

        // Start elements with DAV:
        if (type == QXmlStreamReader::StartElement && (name=="system-tag" || name=="tag")){
            insideTag=true;
        }
        else if(type== QXmlStreamReader::Characters && insideTag){
            token.append(reader.text());
        }
        else if (type == QXmlStreamReader::EndElement && (name == "system-tag" || name=="tag")){
            if(token.size()>0)tags << token;
            token.clear();
            insideTag=false;
        }
    }

    if(tags.size()>0){
        if(list.size()>0)tags.append(QString(list).split(QChar(0x000A)));
        tags.removeDuplicates();
        tags.sort(Qt::CaseInsensitive);
        list=tags.join(QChar(0x000A)).toUtf8();
    }
}

QByteArray FileTagManager::readTagListFromLocalFile(const QString &path)
{
#ifdef __APPLE__	
    // Create necessary system related objects
    CFStringRef cfstr = path.toCFString();
    CFURLRef urlref = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                    cfstr,
                                                    kCFURLPOSIXPathStyle,
                                                    false);

    // Query tags
    CFArrayRef labels=NULL;
    Boolean result = CFURLCopyResourcePropertyForKey(urlref,
                                                     kCFURLTagNamesKey,
                                                     &labels,
                                                     NULL);

    QStringList tagarray;

    if(result==true && labels != NULL){
        // Extract the labels to our array
        int count = (int) CFArrayGetCount(labels);

        if(count>0){
            for(int index=0;index<count;index++){
                CFStringRef str = (CFStringRef)CFArrayGetValueAtIndex(labels,index);
                if(CFStringGetLength(str)>0)tagarray << QString::fromCFString(str);
            }
        }
    }

    tagarray.sort(Qt::CaseInsensitive);

    // Clean up
    CFRelease(cfstr);
    CFRelease(urlref);
    if(labels!=NULL)CFRelease(labels);

    return tagarray.join(QChar(0x000A)).toUtf8();
#endif
#ifdef __linux__
    const int sz=4096;
    char buffer[sz];
    int result = getxattr(path.toUtf8().constData(),"user.xdg.tags",buffer,sz);

    if(result>0){
        // Data is stored usually as comma(,) separated list.
        // So we just need to replace ',' by '\n'.
        QByteArray tagList = QByteArray(buffer,result);
        tagList.replace(',','\n');
        return tagList;
    }
    else return QByteArray();
#endif
}
	
bool FileTagManager::writeTagListToLocalFile(const QString &localpath,const QByteArray &taglist)
{
	qCInfo(lcFTM) << "Write file tag list to file system: " << localpath;

#ifdef __APPLE__

    QStringList strlist=QString(taglist).split(QChar(0x000A));

    //Create new array and append tag
    int newsize=strlist.size();
    CFStringRef* strs = new CFStringRef[newsize];

    for(int index=0;index<newsize;index++){
        strs[index]=strlist[index].toCFString();
    }

    CFArrayRef newtags = CFArrayCreate(NULL,(const void**)strs, newsize, &kCFTypeArrayCallBacks);

    // Create necessary system related objects
    CFStringRef cfstr = localpath.toCFString();
    CFURLRef urlref = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                    cfstr,
                                                    kCFURLPOSIXPathStyle,
                                                    false/* TODO: Is directory*/);

    // Query tags
    Boolean result = CFURLSetResourcePropertyForKey(urlref,
                                                    kCFURLTagNamesKey,
                                                    newtags,
                                                    NULL);

    // Clean unnecessary stuff
    for(int index=0;index<newsize;index++){
        CFRelease(strs[index]);
    }

    CFRelease(cfstr);
    CFRelease(urlref);
    CFRelease(newtags);

    return result;

#endif
#ifdef __linux__

    QByteArray lcTagList=taglist;
    lcTagList.replace('\n',',');

    int result = setxattr(localpath.toUtf8().constData(),
    "user.xdg.tags",
    lcTagList.constData(),
    lcTagList.size(),
    0);

    if(result==0)return true;
    else return false;

#endif
}

bool FileTagManager::restoreLocalFileTags(SyncJournalDb* journal,
                                          const QString &localdir,
                                          const QString &file)
{
    qCInfo(lcFTM) << "Restore file tag list in file system: " << file;
    return writeTagListToLocalFile(localdir % file,journal->tagList(file));
}

void FileTagManager::syncTags(AccountPtr account,
                              SyncJournalDb* journal,
                              const QString &localDir,
                              const QString &filePath,
                              const LocalInfo &localEntry,
                              const RemoteInfo &serverEntry,
                              const SyncJournalFileRecord &dbEntry)
{
    QString fullpath=localDir%filePath;

    if(localEntry.isValid()
       && dbEntry.isValid()
       && serverEntry.isValid()){

        if(localEntry.tagList!= dbEntry._tagList
          || localEntry.tagList!=serverEntry.tagList
          || dbEntry._tagList!=serverEntry.tagList){

            // For syncing tags, the following matrix applies:
            //
            // LC: Local, DB: SyncDatabase, RM: Remote
            // X: Tag is present, -: Tag is not present
            //-----------------------------------------
            //  LC DB RM  What happend   ACTION
            //-----------------------------------------
            // 1 -  -  -  no tags        NOP
            // 2 X  X  X  tag present    NOP
            // 3 X  -  X  lc/rm added    ADD  (DB)
            // 4 X  -  -  lc added       ADD  (RM+DB)
            // 5 -  -  X  rm added       ADD  (LC+DB)
            // 6 -  X  -  lc/rm deleted  DEL  (DB)
            // 7 -  X  X  lc deleted     DEL  (RM+DB)
            // 8 X  X  -  rm deleted     DEL  (LC+DB)
            //------------------------------------------

            // Note: The lists from the entries are always alphabetically sorted.
            QStringList lcTags;
            QStringList dbTags;
            QStringList rmTags;
            QStringList syncedTagList;

            // Avoid adding empty entries to the list
            if(localEntry.tagList.size()>0)lcTags=QString(localEntry.tagList).split(QChar(0x000A));
            if(serverEntry.tagList.size()>0)rmTags=QString(serverEntry.tagList).split(QChar(0x000A));
            if(dbEntry._tagList.size()>0)dbTags=QString(dbEntry._tagList).split(QChar(0x000A));

            // The "new" synced list ist a joined list of all present tags, and then deleting
            // Tags according to row 6-8 of above table
            syncedTagList << lcTags;
            syncedTagList << rmTags;
            syncedTagList << dbTags;
            syncedTagList.removeDuplicates();
            syncedTagList.sort(Qt::CaseInsensitive);

            for(int index=0;index<syncedTagList.size();index++){
                const QString& tag=syncedTagList[index];
                bool lc =lcTags.contains(tag,Qt::CaseInsensitive);
                bool db =dbTags.contains(tag,Qt::CaseInsensitive);
                bool rm =rmTags.contains(tag,Qt::CaseInsensitive);

                // row 6-8
                if(db==true && (lc==false || rm==false)){
                    syncedTagList.removeAt(index);
                    index--;
                }
            }

            QByteArray newTagList = syncedTagList.join(QChar(0x000A)).toUtf8();

            bool syncRemote = newTagList != serverEntry.tagList;
            bool syncLocal  = newTagList != localEntry.tagList;
            bool syncDb     = newTagList != dbEntry._tagList;

            qCInfo(lcFTM) << "Full sync: "
                          << fullpath.toUtf8().data()
                          << '('<<syncedTagList.join(QChar((int)',')).toUtf8().data()<<')'
                          << (syncLocal?'L':'-')
                          << (syncRemote?'R':'-')
                          << (syncDb?'D':'-');

            // We sync in that order:
            // 1. Remote
            // 2. Local
            // 3. Database (only if local is successfull)

            bool remoteSuccess=true;
            if(syncRemote){
                GetInstance()->syncTagsStep1(account,
                                             journal,
                                             fullpath,
                                             syncedTagList,
                                             dbEntry,
                                             syncLocal,
                                             syncDb);
            }
            else{
                GetInstance()->syncTagsStep2(journal,
                                             fullpath,
                                             newTagList,
                                             dbEntry,
                                             syncLocal,
                                             syncDb);
            }
        }
    }
    else{
        localSync(account,
                  journal,
                  fullpath,
                  filePath,
                  localEntry,
                  dbEntry);
    }
}

void FileTagManager::syncTagsStep1(AccountPtr account,
                                   SyncJournalDb* journal,
                                   const QString &fullpath,
                                   QStringList list,
                                   const SyncJournalFileRecord &dbEntry,
                                   bool syncLocal,bool syncDb)
{
    // SYNCING REMOTE CAN ONLY SET oc:tags, not nc:system-tags
    // oc:tags can double nc:system-tags, but we try to prevent that.
    // We do following:
    // 1. propfind oc:tags list and od:system-tags of file
    // 3. remove system-tags from list (we cannot delete them acually)
    // 3. deside wich tag to set or remove
    // 4. proppatch oc:tags
    // TODO: Update this procedure, as soon as Nextcloud Server supports PROPPATCH of nc:system-tags
    // Check: https://github.com/nextcloud/server/blob/master/apps/dav/lib/SystemTag/SystemTagPlugin.php

    qCInfo(lcFTM) << "Start remote sync: "
                  << fullpath <<' '
                  << (syncLocal?'L':'-')
                  << (syncDb?'D':'-');

    // Note: We must not delete tags explicitly. The server removes non present tags automatically.
    QString tags;
    for(int index=0;index<list.size();index++){
        tags.append("<tag xmlns=\"http://owncloud.org/ns\">");
        tags.append(list[index]);
        tags.append("</tag>");
    }

    QMap<QByteArray, QByteArray> setproperties;
    setproperties.insert(QString("http://owncloud.org/ns:tags").toUtf8(),tags.toUtf8());

    ProppatchJob* job = new ProppatchJob(account,dbEntry._path);
    if(!setproperties.isEmpty())job->setProperties(setproperties);

    QByteArray newTagList = list.join(QChar(0x000A)).toUtf8();

    QObject::connect(job, &ProppatchJob::success, GetInstance(), [=](){
        qCInfo(lcFTM) << "Remote sync successfull: " << fullpath;
        syncTagsStep2(journal,fullpath,newTagList,dbEntry,syncLocal,syncDb);
        });

    QObject::connect(job, &ProppatchJob::finishedWithError, GetInstance(),[=]() {
        // Do nothing, ignore.
        qCInfo(lcFTM) << "Remote sync with errors: " << fullpath;
        });

    job->start();
}

void FileTagManager::syncTagsStep2(SyncJournalDb* journal,
                                   const QString &fullpath,
                                   const QByteArray &newTagList,
                                   const SyncJournalFileRecord &dbEntry,
                                   bool syncLocal,bool syncDb)
{
    bool localSuccess=true;
    if(syncLocal)
    {
        localSuccess=writeTagListToLocalFile(fullpath,newTagList);
    }

    if(localSuccess && syncDb)
    {
        journal->updateMetadataTagList(dbEntry._path,&newTagList);
    }
};

void FileTagManager::localSync(AccountPtr account,
                              SyncJournalDb* journal,
                              const QString &localDir,
                              const QString &filePath,
                              const LocalInfo &localEntry,
                              const SyncJournalFileRecord &dbEntry)
{
    QString fullpath=localDir%filePath;

    if(localEntry.isValid() && dbEntry.isValid()){

        if(localEntry.tagList!= dbEntry._tagList){

            // For syncing tags, the following matrix applies:
            //
            //  LC DB  What happened    Action
            // 1 -  -  no tags          NOP
            // 2 X  X  tag present      NOP
            // 3 -  X  rm add or lc del FULL SYNC
            // 4 X  -  lc add or rm del FULL SYNC
            //
            // Note: The lists from the entries are always alphabetically sorted.
            QStringList lcTags;
            QStringList dbTags;
            QStringList syncedTagList;

            // Avoid adding empty entries to the list
            if(localEntry.tagList.size()>0)lcTags=QString(localEntry.tagList).split(QChar(0x000A));
            if(dbEntry._tagList.size()>0)dbTags=QString(dbEntry._tagList).split(QChar(0x000A));

            // The "new" synced list ist just a joined list of all present tags.
            syncedTagList << lcTags;
            syncedTagList << dbTags;
            syncedTagList.removeDuplicates();
            syncedTagList.sort(Qt::CaseInsensitive);

            QByteArray newTagList = syncedTagList.join(QChar(0x000A)).toUtf8();

            bool fullSync  = newTagList != localEntry.tagList || newTagList != dbEntry._tagList;

            qCInfo(lcFTM) << "Local sync: "
                          << filePath
                          << '('<< syncedTagList.join(QChar((int)',')).toUtf8().data() << ')'
                          << (fullSync?'F':'N');

            if(fullSync){
                const auto propfindJob = new PropfindJob(account, filePath, GetInstance());
                propfindJob->setProperties({ QByteArrayLiteral("http://owncloud.org/ns:tags"),
                                             QByteArrayLiteral("http://nextcloud.org/ns:system-tags") });

                connect(propfindJob, &PropfindJob::result, GetInstance(), [=](const QVariantMap &result){
                    qCInfo(lcFTM) << "Fetching remote info ok: " << fullpath;

                    RemoteInfo remoteInfo;
                    int slash = filePath.lastIndexOf('/');
                    remoteInfo.name = filePath.mid(slash + 1);

                    //TODO: SNIPPTE TAKEN FROM fieldtagmodel.h, but I quess doubling it is not good,
                    // also I prefer to remove the "special" code from PropfindJob and handle it
                    const auto normalTags = result.value(QStringLiteral("tags")).toStringList();
                    const auto systemTags = result.value(QStringLiteral("system-tags")).toList();

                    QStringList systemTagStringList = QStringList();
                    for (const auto &systemTagMapVariant : systemTags) {
                        const auto systemTagMap = systemTagMapVariant.toMap();
                        const auto systemTag = systemTagMap.value(QStringLiteral("tag")).toString();
                        systemTagStringList << systemTag;
                    }

                    QStringList tags=QStringList();
                    tags << normalTags << systemTagStringList;
                    tags.removeDuplicates();
                    tags.removeAll(QString(""));
                    tags.sort(Qt::CaseInsensitive);

                    remoteInfo.tagList=tags.join(QChar(0x000A)).toUtf8().constData();

                    QByteArray rl =remoteInfo.tagList;
                    rl.replace('\n',',');

                    syncTags(account,
                             journal,
                             localDir,
                             filePath,
                             localEntry,
                             remoteInfo,
                             dbEntry);
                });

                connect(propfindJob, &PropfindJob::finishedWithError, GetInstance(), [=](QNetworkReply*){
                    //Do nothing here.
                    qCInfo(lcFTM) << "Fetching remote info with errors: " << fullpath;
                });

                propfindJob->start();
            }
        }
    }
}

}
