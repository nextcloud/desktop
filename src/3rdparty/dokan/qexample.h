#ifndef QEXAMPLE_H
#define QEXAMPLE_H

#include <QObject>

#include "qwebdav.h"
#include "qwebdavdirparser.h"
#include "qwebdavitem.h"

#include "owncloudlib.h"

/*#include <qwebdav.h>
#include <qwebdavdirparser.h>
#include <qwebdavitem.h>
*/

namespace OCC {

	static int _hasBeenData = 0;
	static QString S_user, S_pass, S_host;

	class OWNCLOUDSYNC_EXPORT QExample : public QObject
	{
		Q_OBJECT

			QWebdav w;
		QWebdavDirParser p;
		QString m_path;
		QList<QNetworkReply *> m_replyList;

	public:
		QExample(QObject* parent, QString m_host, QString m_user, QString m_pass, QString m_relativeDir);
		
	signals:
		void ya_termine();

		public slots :
			void printList();
			
		void printError(QString errorMsg);
		void replySkipRead();
		void replyFinished(QNetworkReply*);
		
	public:
		void start();

	private:
			QString _relativeDir;

	};

}
#endif // QEXAMPLE_H
