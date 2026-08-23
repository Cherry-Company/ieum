/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/Log.h"

#include <QObject>

class ServerProxyTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void initTestCase();
  void handleKeepAliveAlarm_timeout_queuesDisconnectRequest();
  void handleData_incompleteMessage_queuesDisconnectRequest();
  void handleData_largeBurst_queuesBoundedContinuation();
  void parseHandshakeMessage_protocolError_queuesRefusalRequest();
  void handleData_truncatedHandshakePayload_queuesDisconnectRequest();
  void parseMessage_truncatedPayload_data();
  void parseMessage_truncatedPayload();
  void parseHandshakeMessage_truncatedLanguagePayload_throws();
  void parseMessage_allocationFailure_throwsBeforeSideEffects();
  void clientProxy_truncatedInitialInfo_disconnects();
  void clientProxy_allocationFailure_disconnects();
  void clientProxy_truncatedRuntimePayload_disconnects_data();
  void clientProxy_truncatedRuntimePayload_disconnects();
  void clientProxy_largeBurst_yieldsToTimerAndSecondClient();

private:
  Log m_log;
};
