/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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

#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTextStream>
#include <QTextCodec>

#include "mirall/sitecopyfolder.h"


namespace Mirall {

    SiteCopyFolder::SiteCopyFolder( const QString &alias,
                                    const QString &path,
                                    const QString &secondPath,
                                    QObject *parent)
      : Folder(alias, path, parent),
        _SiteCopy(new QProcess(this)),
        _syncCount(0),
        _remotePath( secondPath )
{
    QObject::connect(_SiteCopy, SIGNAL(readyReadStandardOutput()),
                     SLOT(slotReadyReadStandardOutput()));

    QObject::connect(_SiteCopy, SIGNAL(readyReadStandardError()),
                     SLOT(slotReadyReadStandardError()));

    QObject::connect(_SiteCopy, SIGNAL(stateChanged(QProcess::ProcessState)),
                     SLOT(slotStateChanged(QProcess::ProcessState)));

    QObject::connect(_SiteCopy, SIGNAL(error(QProcess::ProcessError)),
                     SLOT(slotError(QProcess::ProcessError)));

    QObject::connect(_SiteCopy, SIGNAL(started()),
                     SLOT(slotStarted()));

    QObject::connect(_SiteCopy, SIGNAL(finished(int, QProcess::ExitStatus)),
                     SLOT(slotFinished(int, QProcess::ExitStatus)));
}

SiteCopyFolder::~SiteCopyFolder()
{
}

bool SiteCopyFolder::isBusy() const
{
    return (_SiteCopy->state() != QProcess::NotRunning);
}

QString SiteCopyFolder::remotePath() const
{
  return _remotePath;
}

void SiteCopyFolder::startSync(const QStringList &pathList)
{
  QMutexLocker locker(&_syncMutex);

  emit syncStarted();
  qDebug() << "PATHLIST: " << pathList;

  _pathListEmpty = (pathList.count() == 0);

  startSiteCopy( "--fetch", FlatList );
}

void SiteCopyFolder::fetchFromOC()
{
  QMutexLocker locker( &_syncMutex );
  qDebug() << "starting to sync from ownCloud";

  startSiteCopy( "--fetch", Sync );
}

void SiteCopyFolder::startSiteCopy( const QString& command, SiteCopyState nextState )
{
  if( _SiteCopy->state() == QProcess::Running ) {
    qDebug() << "Process currently running - come back later.";
    return;
  }

  if( alias().isEmpty() ) {
      qDebug() << "Site name not set, can not perform commands!";
      return;
  }

  QString programm = "/usr/bin/sitecopy";
  QStringList args;
  args << command << alias();
  qDebug() << "** starting command " << args;
  _NextStep  = nextState;
  _lastOutput.clear();

  _SiteCopy->start( programm, args );
}

void SiteCopyFolder::slotStarted()
{
    qDebug() << "    * SiteCopy process started ( PID " << _SiteCopy->pid() << ")";
    _syncCount++;

    //qDebug() << _SiteCopy->readAllStandardOutput();;
}

void SiteCopyFolder::slotFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "    * SiteCopy process finished with status" << exitCode;

    if( exitCode == -1 ) {
        qDebug() << "Configuration Error, stop processing.";
        SyncResult res( SyncResult::Error );
        res.setErrorString( tr("Sitecopy configuration problem, check $HOME/.sitecopyrc"));
        incrementErrorCount();
        emit( syncFinished( res ));
    }

    if( exitCode > 1 ) {
      // error has happened.
      QString out( _lastOutput );
      SyncResult res( SyncResult::Error );
      res.setErrorString( out );
      qDebug() << "An error happened: " << out;
      incrementErrorCount();
      emit syncFinished( res );
      return;
    }


    if( _NextStep == Sync ) {
      startSiteCopy( "--sync", Finish ); // sync local with cloud data.
    } else if( _NextStep == Update ) {
      startSiteCopy( "--update", Finish ); // update from owncloud
    } else if( _NextStep == Finish ) {
      qDebug() << "Finished!";
      emit syncFinished( SyncResult(SyncResult::Success) );
      // mLocalChangesSeen = false;
    } else if( _NextStep == Status ) {
      startSiteCopy( "--fetch", FlatList );
    } else if( _NextStep == FlatList ) {
      startSiteCopy( "--flatlist", ExecuteStatus );
    } else if( _NextStep == ExecuteStatus ) {
        if( exitCode == 1 ) {
            qDebug() << "Exit-Code: Sync Needed!";
            analyzeStatus();
            if( _ChangesHash.contains("deleted") && _pathListEmpty ) {
              // problem: Files were added on the cloud. If we say update,
              // the new files are going to be deleted. Lets warn the user.
              qDebug() << "!!!! Better not call update, changes happened on the cloud!";
              SyncResult res( SyncResult::Disabled );
              res.setErrorString( tr("The ownCloud changed. Disabling syncing for security reasons."));
              setSyncEnabled( false );
              emit syncFinished( res );
            } else {
              startSiteCopy( "--update", Status );
            }
        } else if( exitCode == 0 ) {
            qDebug() << "No update needed, remote is in sync.";
            // No update needed
            emit syncFinished( SyncResult( SyncResult::Success ) );
        } else {
            qDebug() << "Got an unknown exit code " << exitCode;
            emit syncFinished( SyncResult( SyncResult::Error ) );
        }
    }

    _lastOutput.clear();
}

void SiteCopyFolder::analyzeStatus()
{
  QString out( _lastOutput );
  qDebug() << "Output: " << out;

  _ChangesHash.clear();

  QStringList items;
  QString action;

  QStringList li = out.split(QChar('\n'));
  foreach( QString l, li ) {
    if( l.startsWith( "sectstart|") ) {
      action = l.mid(10);
      qDebug() << "starting to parse " << action;
    }
    if( l.startsWith( "sectend|")) {
      action = l.mid(8);
      _ChangesHash.insert( action, items );
      items.clear();
    }
    if( l.startsWith( "item|" )) {
      QString item = l.mid(5);
      items << item;
    }

    if( l.startsWith("siteend") ) {
#if 0
      if( l.endsWith("unchanged") ) {
        // we are synced and don't do anything
        // emit statusChange( Unchanged );
          // FIXME: Bug handling
          emit syncFinished( SyncResult(SyncResult::Success) );
      } else if( l.endsWith("changed")) {
          startSiteCopy( "--update", Status );

        if( mLocalChangesSeen ) {
          // emit statusChange( SyncToNeeded );
        } else {
          // emit statusChange( SyncFromNeeded );
        }

      }
#endif
    }
  }
}

void SiteCopyFolder::slotReadyReadStandardOutput()
{
    QByteArray arr = _SiteCopy->readAllStandardOutput();

    if( _NextStep == Finish ) {
        QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        // render the output to status line
        QString string = codec->toUnicode( arr );
        int pos = string.indexOf( QChar('\n'));
        if( pos > -1 ) {
            QString newLine = string.mid( 1+pos );
            _StatusString = newLine;
        } else {
            // no newline, append to the status string
            _StatusString.append( string );
        }
        emit statusString( _StatusString );

    } else { /* if( _NextStep == DisplayStatus ) { */
        _lastOutput += arr;
    }

//    QTextStream stream(&_lastOutput);
//    stream << _SiteCopy->readAllStandardOutput();;
}

void SiteCopyFolder::slotReadyReadStandardError()
{
    QTextStream stream(&_lastOutput);
    stream << _SiteCopy->readAllStandardError();
}

void SiteCopyFolder::slotStateChanged(QProcess::ProcessState state)
{
    //qDebug() << "changed: " << state;
}

void SiteCopyFolder::slotError(QProcess::ProcessError error)
{
    //qDebug() << "error: " << error;
}

} // ns

#include "sitecopyfolder.moc"
