#include "qexample.h"
#include <QSslConfiguration>
#include <QMessageBox>
#include <QSslKey>
#include <QSslCertificate>
#include <QList>

#include <windows.h>

namespace OCC {

	QExample::QExample(QObject *parent, QString m_host, QString m_user, QString m_pass, QString m_relativeDir) :
		QObject(parent)
	{

		if(!_hasBeenData)
			{
			S_host = m_host;  S_user = m_user; S_pass = m_pass;
			_hasBeenData = 1;
			}
		else
			{
			 m_host = S_host;  m_user = S_user;  m_pass = S_pass;
			}

		FILE * fp;
		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::QExample host: --%s--", std::string(m_host.toLocal8Bit().constData()).c_str() );
		fprintf(fp, "\nF.................QExample::QExample user: --%s--", std::string(m_user.toLocal8Bit().constData()).c_str());
		fprintf(fp, "\nF.................QExample::QExample pass: --%s--", std::string(m_pass.toLocal8Bit().constData()).c_str());
		fprintf(fp, "\nF.................QExample::QExample dir: --%s--", std::string(m_relativeDir.toLocal8Bit().constData()).c_str());

		fclose(fp);

		_relativeDir = m_relativeDir;

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::QExample %s", "1a");
		fclose(fp);

		std::string J_host = m_host.toLocal8Bit().constData();

		char UrlType[10];
		char __host[50];
		int kk = 0;
		char *pc = (char*)J_host.c_str();
		while (*pc != ':')
		{
			UrlType[kk++] = *pc;
			pc++;
		}

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::QExample %s", "1b");
		fclose(fp);

		UrlType[kk] = 0;
		kk = 0;
		pc += 3;
		while (*pc != '/')
		{
			__host[kk++] = *pc;
			pc++;
		}
		__host[kk] = 0;



		//FILE * fp;
		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::QExample %s","2");
		fclose(fp);

		_relativeDir = m_relativeDir;

		QString wUrlType(UrlType);
		QString w__host(__host);

		if (wUrlType.compare(QString("https")) == 0)
			w.setConnectionSettings(QWebdav::HTTPS, w__host, "/remote.php/dav/files", m_user, m_pass, 443);
		else if (wUrlType.compare(QString("http")) == 0)
			w.setConnectionSettings(QWebdav::HTTP, w__host, "/remote.php/dav/files", m_user, m_pass, 80);

		//w.setConnectionSettings(QWebdav::HTTPS, "i0000.clarodrive.com", "/remote.php/dav/files", m_user, m_pass, 443);


		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::QExample %s", "3");
		fclose(fp);

		connect(&p, SIGNAL(finished()), this, SLOT(printList()));
		connect(&p, SIGNAL(errorChanged(QString)), this, SLOT(printError(QString)));
		connect(&w, SIGNAL(errorChanged(QString)), this, SLOT(printError(QString)));

		m_path = "/" + m_user + _relativeDir;
	
		//m_path = "/" + m_user + "/d1/";

		//FILE *
		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::Qexample() llmando p.listDirectory %s", "INIT");
		fclose(fp);

		///////////p.listDirectory(&w, m_path);

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::Qexample() llmando p.listDirectory %s", "END");
		fclose(fp);

		//Sleep(1000000000); 

		/*
		QMessageBox msgBox;
		msgBox.setText("Wait....: 2");
		msgBox.exec();
		*/
		

	}

	void QExample::replyFinished(QNetworkReply* reply)
	{
		FILE * fp;
		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n----------> %s", " QExample::replyFinished\n");
		fclose(fp);

		QString answer = reply->readAll();

		std::string m_answer = answer.toLocal8Bit().constData();



		QString err = reply->errorString();
		std::string m_error = answer.toLocal8Bit().constData();


	}
	void QExample::printList()
	{

		FILE * fp;
		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n----------> %s", "QExample::printList() INIT\n");
		fclose(fp);

		QList<QWebdavItem> list = p.getList();

		QWebdavItem item;
		foreach(item, list) {
			qDebug() << item.name();

			std::string m_alias = item.name().toLocal8Bit().constData();
			//char mP[200] = "X:/";
			//char mP[200] = "C:\\Users\\poncianoj\\dirUser_clientLaboratory\\";
			char mP[300] = "C:/Users/poncianoj/dirUser_clientLaboratory";
				std::string M_relativeDir = _relativeDir.toLocal8Bit().constData();
				char *qc = (char*)M_relativeDir.c_str();

			strcat(mP, qc);


			strcat(mP, m_alias.c_str());

			if (!item.isDir())
			{

				fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
				fprintf(fp, "\nPINTANDO----------f: --%s--", mP);
				fclose(fp);

				//FILE *fp = NULL;
				fp = fopen(mP, "w");
				if (fp)fclose(fp);
			}
			else
			{
				wchar_t qw[200];
				//QString strVariable1("X:/");
				//QString strVariable1("C:\\Users\\poncianoj\\dirUser_clientLaboratory\\");
				QString strVariable1("C:/Users/poncianoj/dirUser_clientLaboratory");

				strVariable1.append(_relativeDir);

				strVariable1.append(item.name());

				QDir dir = QDir::root();
				dir.mkdir(strVariable1);
			}





			QNetworkReply *reply = w.get(item.path());
			connect(reply, SIGNAL(readyRead()), this, SLOT(replySkipRead()));
			m_replyList.append(reply);
		}

		fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n----------> %s", "QExample::printList() END");
		fclose(fp);
		emit ya_termine();
	}

	void QExample::printError(QString errorMsg)
	{

		std::string m_alias = errorMsg.toLocal8Bit().constData();
	
		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\n----------> QExample::printError %s", m_alias.c_str());
		fclose(fp);
		qDebug() << "QWebdav::printErrors()  errorMsg == " << errorMsg;

	}

	void QExample::replySkipRead()
	{


		QNetworkReply* reply = qobject_cast<QNetworkReply*>(QObject::sender());
		if (reply == 0)
			return;

		QByteArray ba = reply->readAll();

		qDebug() << "QWebdav::replySkipRead()   skipped " << ba.size() << " reply->url() == " << reply->url().toString(QUrl::RemoveUserInfo);
	}

	void QExample::start()
	{

		FILE *fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::start() %s", "INIT");
		fclose(fp);


		p.listDirectory(&w, m_path);

		///////FILE *
			fp = fopen("C:\\Users\\poncianoj\\bb.txt", "a+");
		fprintf(fp, "\nF.................QExample::start() %s", "END");
		fclose(fp);



	}

}