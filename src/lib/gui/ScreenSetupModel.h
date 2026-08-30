/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QAbstractTableModel>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "gui/config/ScreenList.h"

class ScreenSetupView;
class ServerConfigDialog;

class ScreenSetupModel : public QAbstractTableModel
{
  Q_OBJECT

  friend class ScreenSetupView;
  friend class ServerConfigDialog;

public:
  ScreenSetupModel(ScreenList &screens, int numColumns, int numRows);

  static const QString &mimeType()
  {
    return m_MimeType;
  }
  static QByteArray encodeScreenDrag(int sourceColumn, int sourceRow, const Screen &screen);
  [[nodiscard]] bool isInBounds(int column, int row) const;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  int rowCount() const
  {
    return m_NumRows;
  }
  int columnCount() const
  {
    return m_NumColumns;
  }
  int rowCount(const QModelIndex &) const override
  {
    return rowCount();
  }
  int columnCount(const QModelIndex &) const override
  {
    return columnCount();
  }
  Qt::DropActions supportedDropActions() const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool isFull() const;

Q_SIGNALS:
  void screensChanged();

protected:
  bool
  dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
  const Screen *screenAt(const QModelIndex &index) const
  {
    return index.isValid() && index.model() == this ? screenAt(index.column(), index.row()) : nullptr;
  }
  Screen *screenAt(const QModelIndex &index)
  {
    return index.isValid() && index.model() == this ? screenAt(index.column(), index.row()) : nullptr;
  }
  const Screen *screenAt(int column, int row) const;
  Screen *screenAt(int column, int row);
  void addScreen(const Screen &newScreen);

private:
  bool decodeScreenDrag(const QByteArray &encodedData, int &sourceColumn, int &sourceRow, Screen &screen) const;

  ScreenList &m_Screens;
  const int m_NumColumns;
  const int m_NumRows;

  static constexpr quint32 m_MimeMagic = 0x49554d53;
  static constexpr quint16 m_MimeVersion = 1;
  static constexpr quint16 m_MimeFieldCount = 3;
  static const QString m_MimeType;
};
