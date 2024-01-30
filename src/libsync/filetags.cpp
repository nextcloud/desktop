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
#include "filetags.h"

namespace OCC{
	
	FileTagManager* gFileTagManger=NULL;
	
	FileTagManager::FileTagManager(AccountPtr account,SyncJournalDb* journal):QObject(NULL)
	{
		_account=account;
		_journal=journal;
	}
	
	void FileTagManager::Init(AccountPtr account,SyncJournalDb* journal)
	{
		ASSERT(gFileTagManger==NULL);
		gFileTagManger=new FileTagManager(account,journal);
	}
	
	FileTagManager* FileTagManager::GetInstance()
	{
		ASSERT(gFileTagManger!=NULL);
		return gFileTagManger;
	}
	
	
	QByteArray FileTagManager::fromPropertiesToTagList(const QString &properties)
	{
		if(properties==NULL || properties.isEmpty()) return QByteArray();
		
		QStringList tags;
		QString token;
		QXmlStreamReader reader("<system-tags>"%properties%"</system-tags>");
		bool insideTag = false;
		
		while (!reader.atEnd())
		{
			QXmlStreamReader::TokenType type = reader.readNext();
			QString name = reader.name().toString();
			
				// Start elements with DAV:
			if (type == QXmlStreamReader::StartElement && (name=="system-tag" || name=="tag"))
			{
				insideTag=true;
			}
			else if(type== QXmlStreamReader::Characters && insideTag)
			{
				token.append(reader.text());
			}
			else if (type == QXmlStreamReader::EndElement && (name == "system-tag" || name=="tag"))
			{
				if(token.size()>0)tags << token;
				token.clear();
				insideTag=false;
			}
		}
		
		tags.sort(Qt::CaseInsensitive);
		printf("RMD TAG %s\n",tags.join(QChar(';')).toUtf8().data());
		return tags.join(QChar(0x000A)).toUtf8();
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
	
	if(result>0)
	{
		// Data is stored usually as comma(,) separated list.
		// So we just need to replace ',' by '\n'.
		QByteArray tagList = QByteArray(buffer,result);
		tagList.replace(',','\n');
		printf("%s\n",buffer);
		return tagList;
	}
    	else return QByteArray();
#endif
}
	
bool FileTagManager::writeTagListToLocalFile(const QString &localpath,const QByteArray &taglist)
{
#ifdef __APPLE__	
		printf("RMD SYNC LOCAL FS\n");
		QStringList strlist=QString(taglist).split(QChar(0x000A));
		
			//Create new array and append tag
		int newsize=strlist.size();
		CFStringRef* strs = new CFStringRef[newsize];
		for(int index=0;index<newsize;index++)
		{
			strs[index]=strlist[index].toCFString();
		}
		
		CFArrayRef newtags = CFArrayCreate(NULL,(const void**)strs, newsize, &kCFTypeArrayCallBacks);
		
			//
			//
			//
		
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
		
		for(int index=0;index<newsize;index++)
		{
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
	
	
	bool FileTagManager::restoreLocalFileTags(const QString &localdir,const QString &file)
	{
		printf("RMD RESTORE LOCAL TAGS %s\n",file.toUtf8().data());
		
		writeTagListToLocalFile(localdir % file,
										_journal->tagList(file));
	}
	
	void FileTagManager::pushTags(const QString &fullpath,
											const LocalInfo &localEntry,
											const RemoteInfo &serverEntry,
											const SyncJournalFileRecord &dbEntry)
	{
		if(localEntry.isValid()
			&& dbEntry.isValid()
			&& serverEntry.isValid())
		{
			
			if(localEntry.tagList!= dbEntry._tagList
				|| localEntry.tagList!=serverEntry.tagList
				|| dbEntry._tagList!=serverEntry.tagList)
			{
				printf("RMD TAG PUSH %s\n",fullpath.toUtf8().data());
				
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
				
				for(int index=0;index<syncedTagList.size();index++)
				{
						//TODO: REMOVE LOCAL DELETED TAGS
				}
				
				QByteArray newTagList = syncedTagList.join(QChar(0x000A)).toUtf8();
				
				bool syncRemote = newTagList != serverEntry.tagList;
				bool syncLocal  = newTagList != localEntry.tagList;
				bool syncDb     = newTagList != dbEntry._tagList;
				
				printf("RMD push sync tags %s (%s) %i,%i,%i\n",
						 fullpath.toUtf8().data(),
						 syncedTagList.join(QChar((int)' ')).toUtf8().data(),
						 syncLocal,
						 syncRemote,
						 syncDb);
				
					// We sync in that order:
					// 1. Remote
					// 2. Local
					// 3. Database (only if local is successfull)
				
				bool remoteSuccess=true;
				if(syncRemote)
				{
					pushTagsStep1(fullpath,syncedTagList,dbEntry,syncLocal,syncDb);
				}
			}
		}
	}
	
	void FileTagManager::pushTagsStep1(const QString &fullpath,
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
		
		QString tags;
			//TODO: REMOVE LOCAL DELETED TAGS
		for(int index=0;index<list.size();index++)
		{
			tags.append("<tag xmlns=\"http://owncloud.org/ns\">");
			tags.append(list[index]);
			tags.append("</tag>");
		}
		
		QByteArray newTagList = list.join(QChar(0x000A)).toUtf8();
		
		QMap<QByteArray, QByteArray> properties;
		properties.insert(QString("http://owncloud.org/ns:tags").toUtf8(),tags.toUtf8());
		
		ProppatchJob* job = new ProppatchJob(_account,dbEntry._path);
		job->setProperties(properties);
		
		QObject::connect(job, &ProppatchJob::success, this, [=](){
			pushTagsStep2(fullpath,newTagList,dbEntry,syncLocal,syncDb);
		});
		
		QObject::connect(job, &ProppatchJob::finishedWithError, this,[=]() {
				// Do nothing, ignore.
			printf("RMD PROPPATCH failure\n");
		});
		
		job->start();
	}
	
	void FileTagManager::pushTagsStep2(const QString &fullpath,const QByteArray &newTagList,const SyncJournalFileRecord &dbEntry,bool syncLocal,bool syncDb)
	{
		printf("RMD PUSH STEP 2 %i,%i\n",syncLocal,syncDb);
		bool localSuccess=true;
		if(syncLocal)
		{
			localSuccess=writeTagListToLocalFile(fullpath,newTagList);
			printf("RMD LOCAL WRITE %i\n",localSuccess);
		}
		
		if(localSuccess && syncDb)
		{
			bool success=_journal->updateMetadataTagList(dbEntry._path,&newTagList);
			printf("RMD DB UPDATE %i\n",success);
		}
	};
	
	
	void FileTagManager::pullTags(const QString &fullpath,
											const LocalInfo &localEntry,
											const SyncJournalFileRecord &dbEntry)
	{
		if(localEntry.isValid()
			&& dbEntry.isValid())
		{
			
			if(localEntry.tagList!= dbEntry._tagList)
			{
				printf("RMD TAG PULL %s\n",fullpath.toUtf8().data());
				
					// For syncing tags, the following matrix applies:
					//
					//  LC DB  What happened   Action
					// 1 -  -  no tags         NOP
					// 2 X  X  tag present     NOP
					// 3 -  X  rm added        ADD (LC)
					// 4 X  -  rm added        ADD (RM)
					//
					// If db taglist is empty, we assume an initial sync for that file and push the tags to the
					// server, even if we
				
					// Note: The lists from the entries are always alphabetically sorted.
				QStringList lcTags;
				QStringList dbTags;
				QStringList syncedTagList;
				bool initialSync=false;
				
					// Avoid addint empty entries to the list
				if(localEntry.tagList.size()>0)lcTags=QString(localEntry.tagList).split(QChar(0x000A));
				if(dbEntry._tagList.size()>0)dbTags=QString(dbEntry._tagList).split(QChar(0x000A));
				if(dbEntry._tagList.size()==0)initialSync=true;
				
					// The "new" synced list ist just a joined list of all present tags, because we do not
					// delete here any tags.
				syncedTagList << lcTags;
				syncedTagList << dbTags;
				syncedTagList.removeDuplicates();
				syncedTagList.sort(Qt::CaseInsensitive);
				
				QByteArray newTagList = syncedTagList.join(QChar(0x000A)).toUtf8();
				
				bool syncLocal  = newTagList != localEntry.tagList;
				bool syncDb     = newTagList != dbEntry._tagList;
				
				printf("RMD pull sync tags %s (%s) %i,%i\n",
						 fullpath.toUtf8().data(),
						 syncedTagList.join(QChar((int)' ')).toUtf8().data(),
						 syncLocal,
						 syncDb);
				
					// We sync in that order:
					// 1. Local
					// 2. Database (only if local is successfull)
				
				if(initialSync)
				{
					printf("RMD INITIAL!\n");
				}
				
				bool localSuccess=true;
				if(syncLocal)
				{
					localSuccess=writeTagListToLocalFile(fullpath,newTagList);
					printf("RMD LOCAL WRITE %i\n",localSuccess);
				}
				
				if(localSuccess && syncDb)
				{
					bool success=_journal->updateMetadataTagList(dbEntry._path,&newTagList);
					printf("RMD DB UPDATE %i\n",success);
				}
				
			}
		}
	}
	
}
