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
  void setServerUrl( const QString& );
  void setOCUser( const QString& );
  void setAllowPasswordStorage( bool );
  bool validatePage();
  QString url() const;
  QString localFolder() const;
  void setConnected(bool complete);
  void setRemoteFolder( const QString& remoteFolder);

  SyncMode syncMode();

public slots:
  void setErrorString( const QString&  );
  void setConfigExists(  bool );
  void stopSpinner();

protected slots:
  void slotUrlChanged(const QString&);
  void slotUserChanged(const QString&);

  void setupCustomization();
  void slotToggleAdvanced(int state);
  void slotChangedSelective(QAbstractButton*);
  void slotSelectFolder();

signals:
  void connectToOCUrl( const QString& );

protected:
    void updateFoldersInfo();

private slots:
    void slotHandleUserInput();

private:
    bool urlHasChanged();

  Ui_OwncloudSetupPage _ui;
  QString _oCUrl;
  QString _ocUser;
  bool    _connected;
  bool    _checking;
  bool    _configExists;

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
    QString localFolder() const;

    void enableFinishOnResultWidget(bool enable);

    void displayError( const QString& );
    OwncloudSetupPage::SyncMode syncMode();
    void setConfigExists( bool );
    bool configExists();

public slots:
    void setRemoteFolder( const QString& );
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
    QString _oCUser;
    QStringList _setupLog;
    bool _configExists;
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
  void initializePage();
  void setRemoteFolder( const QString& remoteFolder);

public slots:
  void setComplete(bool complete);

protected slots:
  void slotOpenLocal();
  void slotOpenServer();

protected:
  void setupCustomization();

private:
  QString _localFolder;
  QString _remoteFolder;
  bool _complete;

  Ui_OwncloudWizardResultPage _ui;
};

} // ns Mirall

#endif
