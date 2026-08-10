/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include <QTest>

class ServerTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void SwitchToScreenInfo_alloc_screen();
  void KeyboardBroadcastInfo_alloc_stateAndSceens();
  void cursorTransform_clampsOutOfBounds();
  void cursorTransform_mapsBetweenDifferentResolutions();
  void cursorTransform_roundTripsMixedOrigins();
  void cursorTransform_mapsPhysicalEdgeDisplays();
  void cursorTransform_mapsStackedEdgeDisplaysContinuously();
  void cursorTransform_collapsesPhysicalEdgeGaps();
  void cursorTransform_preservesSubpixelMotion();
  void fullscreenGeometry_distinguishesFullscreenFromMaximized();
  void fullscreenGeometry_detectsPointerCapture();
};
