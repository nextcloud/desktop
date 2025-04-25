/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud, Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTcpServer>

class HttpServer : public QTcpServer
 {
     Q_OBJECT
 public:
    HttpServer(qint16 port, QObject* parent = nullptr);
    void incomingConnection(int socket);

 private slots:
     void readClient();
     void discardClient();
 };
