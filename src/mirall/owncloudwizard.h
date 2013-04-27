/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef MIRALL_OWNCLOUDWIZARD_H
#define MIRALL_OWNCLOUDWIZARD_H

#include <QWizard>

#include "ui_owncloudsetuppage_ng.h"
#include "ui_owncloudwizardresultpage.h"

class QLabel;
class QVariant;
class QProgressIndicator;

namespace Mirall {

class OwncloudSetupPage;
class OwncloudWizardResultPage;

class OwncloudSetupPage: public QWizardPage
{
    Q_OBJECT
public:
  OwncloudSetupPage();
  ~OwncloudSetupPage();

  enum SyncMode {
      SelectiveMode,
      BoxMode
  };

  virtual bool isComplete() const;
  virtual void initializePage();
  virtual int nextId() const;
  void setOCUrl( const QString& );
  void setOCUser( const QString& );
  void setAllowPasswordStorage( bool );
  bool validatePage();
  QString url() const;
  void setConnected(bool complete);
  QString selectedLocalFolder() const;
  void setFolderNames( const QString&, const QString& remoteFolder = QString::null);

  SyncMode syncMode();

public slots:
  void setErrorString( const QString&  );
  void stopSpinner();

protected slots:
  void handleNewOcUrl(const QString& ocUrl);
  void setupCustomization();
  void slotToggleAdvanced(int state);
  void slotChangedSelective(QAbstractButton*);
  void slotSelectFolder();

signals:
  void connectToOCUrl( const QString& );

private:
  Ui_OwncloudSetupPage _ui;
  QString _oCUrl;
  bool    _connected;
  bool    _checking;
  QProgressIndicator *_progressIndi;
  QButtonGroup       *_selectiveSyncButtons;
  QString _remoteFolder;
};

class OwncloudWizard: public QWizard
{
    Q_OBJECT
public:

    enum {
      Page_oCSetup,
      Page_Result
    };

    enum LogType {
      LogPlain,
      LogParagraph
    };

    OwncloudWizard(QWidget *parent = 0L);

    void setOCUrl( const QString& );
    void setOCUser( const QString& );

    void setupCustomMedia( QVariant, QLabel* );
    QString ocUrl() const;

    void enableFinishOnResultWidget(bool enable);

    void displayError( const QString& );
    OwncloudSetupPage::SyncMode syncMode();

public slots:
    void setFolderNames( const QString&, const QString& );
    void appendToConfigurationLog( const QString& msg, LogType type = LogParagraph );
    void slotCurrentPageChanged( int );

    void showConnectInfo( const QString& );
    void successfullyConnected(bool);

signals:
    void clearPendingRequests();
    void connectToOCUrl( const QString& );

private:
    OwncloudSetupPage *_setupPage;
    OwncloudWizardResultPage *_resultPage;

    QString _configFile;
    QString _oCUrl;
    QString _oCUser;
    QStringList _setupLog;
};


/**
 * page to ask for the type of Owncloud to connect to
 */

/**
 * page to display the install result
 */
class OwncloudWizardResultPage : public QWizardPage
{
  Q_OBJECT
public:
  OwncloudWizardResultPage();
  ~OwncloudWizardResultPage();

  bool isComplete() const;

public slots:
  void setComplete(bool complete);
  void setOwncloudUrl( const QString& );
  void setFolderNames( const QString&, const QString& );

protected:
  void setupCustomization();

private:
  QString _url;
  bool _complete;

  Ui_OwncloudWizardResultPage _ui;
};

} // ns Mirall

#endif
