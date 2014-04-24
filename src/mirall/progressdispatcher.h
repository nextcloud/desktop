/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef PROGRESSDISPATCHER_H
#define PROGRESSDISPATCHER_H

#include <QObject>
#include <QHash>
#include <QTime>
#include <QQueue>
#include <QElapsedTimer>
#include "syncfileitem.h"

namespace Mirall {

/**
 * @brief The FolderScheduler class schedules folders for sync
 */
namespace Progress
{
    /** Return true is the size need to be taken in account in the total amount of time */
    inline bool isSizeDependent(csync_instructions_e instruction) {
        return instruction == CSYNC_INSTRUCTION_CONFLICT || instruction == CSYNC_INSTRUCTION_SYNC
            || instruction == CSYNC_INSTRUCTION_NEW;
    }


    struct Info {
		Info() : _totalFileCount(0), _totalSize(0), _completedFileCount(0), _completedSize(0), _etaEstimate()  {}

        quint64 _totalFileCount;
        quint64 _totalSize;
        quint64 _completedFileCount;
        quint64 _completedSize;
		struct EtaEstimate {
			EtaEstimate() :  _startedTime(QDateTime::currentMSecsSinceEpoch()), _agvEtaMSecs(0),_effectiveBandwidth(0) {}
			
			static const int AVG_DIVIDER=10;
			
			quint64	_startedTime ;
			quint64	_agvEtaMSecs;
			quint64 _effectiveBandwidth;
			
			/**
			 * update the estimated eta time with more current data.
			 * @param quint64 completed the amount the was completed.
			 * @param quint64 total the total amout that should be completed.
			 */
			void updateTime(quint64 completed, quint64 total) {
				if(total != 0) {
					quint64 elapsedTime = QDateTime::currentMSecsSinceEpoch() -  this->_startedTime ;
					// (elapsedTime-1) to avoid float "rounding" issue (ie. 0.99999999999999999999....)
					_agvEtaMSecs = _agvEtaMSecs - (_agvEtaMSecs / AVG_DIVIDER) + (elapsedTime * ((float) total / completed ) - (elapsedTime-1) ); 
				}
			}
			
	       quint64 getEtaEstimate() const {
			   return _agvEtaMSecs / AVG_DIVIDER;
	       } 
		};
		EtaEstimate _etaEstimate;

        struct ProgressItem {
            ProgressItem() : _completedSize(0) {}
            SyncFileItem _item;
            quint64 _completedSize;
        };
        QHash<QString, ProgressItem> _currentItems;
        SyncFileItem _lastCompletedItem;

        void setProgressComplete(const SyncFileItem &item) {
            _currentItems.remove(item._file);
            if (Progress::isSizeDependent(item._instruction)) {
                _completedSize += item._size;
            }
            _completedFileCount++;
            _lastCompletedItem = item;
        }
        void setProgressItem(const SyncFileItem &item, quint64 size) {
            _currentItems[item._file]._item = item;
            _currentItems[item._file]._completedSize = size;
            _lastCompletedItem = SyncFileItem();
			_etaEstimate.updateTime(this->completedSize(),this->_totalSize);
        }

        quint64 completedSize() const {
            quint64 r = _completedSize;
            foreach(const ProgressItem &i, _currentItems) {
                r += i._completedSize;
            }
            return r;
        }
        
        /**
		 * Get the eta estimate in milliseconds 
		 * @return quint64 the estimate amount of milliseconds to end the process.
		 */
        quint64 etaEstimate() const {
			return _etaEstimate.getEtaEstimate();
        }
        
        /**
		 * Get the estimated average bandwidth usage.
		 * @return quint64 the estimated bandwidth usage in bytes.
		 */
        quint64 getEstimatedBandwidth() const {
			return ( this->_totalSize - this->completedSize() ) / (1+_etaEstimate.getEtaEstimate()/1000) ;
        }
    };

    QString asActionString( const SyncFileItem& item );
    QString asResultString(  const SyncFileItem& item );

    bool isWarningKind( SyncFileItem::Status );

}

/**
 * @file progressdispatcher.h
 * @brief A singleton class to provide sync progress information to other gui classes.
 *
 * How to use the ProgressDispatcher:
 * Just connect to the two signals either to progress for every individual file
 * or the overall sync progress.
 *
 */
class ProgressDispatcher : public QObject
{
    Q_OBJECT

    friend class Folder; // only allow Folder class to access the setting slots.
public:
    static ProgressDispatcher* instance();
    ~ProgressDispatcher();

signals:
    /**
      @brief Signals the progress of data transmission.

      @param[out]  folder The folder which is being processed
      @param[out]  progress   A struct with all progress info.

     */
    void progressInfo( const QString& folder, const Progress::Info& progress );

protected:
    void setProgressInfo(const QString& folder, const Progress::Info& progress);

private:
    ProgressDispatcher(QObject* parent = 0);

    QElapsedTimer _timer;
    static ProgressDispatcher* _instance;
};

}
#endif // PROGRESSDISPATCHER_H
