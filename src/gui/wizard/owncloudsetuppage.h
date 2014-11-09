/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_OWNCLOUD_SETUP_PAGE_H
#define MIRALL_OWNCLOUD_SETUP_PAGE_H

#include <QWizard>

#include "wizard/owncloudwizardcommon.h"
#include "ui_owncloudsetupnocredspage.h"

class QLabel;
class QVariant;
class QProgressIndicator;

namespace OCC {

class OwncloudSetupPage: public QWizardPage
{
    Q_OBJECT
public:
  OwncloudSetupPage();

  virtual bool isComplete() const Q_DECL_OVERRIDE;
  virtual void initializePage() Q_DECL_OVERRIDE;
  virtual int nextId() const Q_DECL_OVERRIDE;
  void setServerUrl( const QString& );
  void setAllowPasswordStorage( bool );
  bool validatePage() Q_DECL_OVERRIDE;
  QString url() const;
  QString localFolder() const;
  void setRemoteFolder( const QString& remoteFolder);
  void setMultipleFoldersExist( bool exist );
  void setAuthType(WizardCommon::AuthType type);

public slots:
  void setErrorString( const QString&  );
  void setConfigExists(  bool );
  void startSpinner();
  void stopSpinner();

protected slots:
  void slotUrlChanged(const QString&);
  void slotUrlEditFinished();

  void setupCustomization();

signals:
  void determineAuthType(const QString&);

private:
  bool urlHasChanged();

  Ui_OwncloudSetupPage _ui;
  QString _oCUrl;
  QString _ocUser;
  bool    _authTypeKnown;
  bool    _checking;
  WizardCommon::AuthType _authType;

  QProgressIndicator* _progressIndi;
  QString _remoteFolder;
};

} // namespace OCC

#endif
