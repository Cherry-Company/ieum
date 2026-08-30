/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSetupModel.h"

#include <QDataStream>
#include <QIODevice>
#include <QIcon>
#include <QMimeData>

#include "gui/config/Screen.h"

const QString ScreenSetupModel::m_MimeType = "application/x-deskflow-screen";

ScreenSetupModel::ScreenSetupModel(ScreenList &screens, int numColumns, int numRows)
    : QAbstractTableModel(nullptr),
      m_Screens(screens),
      m_NumColumns(numColumns),
      m_NumRows(numRows)
{

  // bound rows and columns to prevent multiply overflow.
  // this is unlikely to happen, as the grid size is only 3x9.
  if (m_NumColumns > 100 || m_NumRows > 100) {
    qFatal("grid size out of bounds: %d columns x %d rows", m_NumColumns, m_NumRows);
    return;
  }

  const long span = static_cast<long>(m_NumColumns) * m_NumRows;
  if (span > screens.size()) {
    qFatal("scrren list (%lld) too small for %d columns x %d rows", screens.size(), m_NumColumns, m_NumRows);
  }
}

QVariant ScreenSetupModel::data(const QModelIndex &index, int role) const
{
  const auto *selectedScreen = screenAt(index);
  if (selectedScreen == nullptr || selectedScreen->isNull())
    return QVariant();

  switch (role) {
  case Qt::DecorationRole:
    return selectedScreen->pixmap();

  case Qt::ToolTipRole:
    return QString(tr("<center>Screen: <b>%1</b></center>"
                      "<br>Double click to edit settings"
                      "<br>Drag screen to the trashcan to remove it"))
        .arg(selectedScreen->name());

  case Qt::DisplayRole:
    return selectedScreen->name();

  default:
    break;
  }
  return QVariant();
}

Qt::ItemFlags ScreenSetupModel::flags(const QModelIndex &index) const
{
  const auto *selectedScreen = screenAt(index);
  if (selectedScreen == nullptr)
    return Qt::NoItemFlags;

  if (!selectedScreen->isNull())
    return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;

  return Qt::ItemIsDropEnabled;
}

Qt::DropActions ScreenSetupModel::supportedDropActions() const
{
  return Qt::MoveAction | Qt::CopyAction;
}

QStringList ScreenSetupModel::mimeTypes() const
{
  return QStringList() << m_MimeType;
}

QMimeData *ScreenSetupModel::mimeData(const QModelIndexList &indexes) const
{
  if (indexes.size() != 1)
    return nullptr;

  const auto &index = indexes.constFirst();
  const auto *selectedScreen = screenAt(index);
  if (selectedScreen == nullptr || selectedScreen->isNull())
    return nullptr;

  const auto encodedData = encodeScreenDrag(index.column(), index.row(), *selectedScreen);
  if (encodedData.isEmpty())
    return nullptr;

  auto *pMimeData = new QMimeData();
  pMimeData->setData(m_MimeType, encodedData);

  return pMimeData;
}

QByteArray ScreenSetupModel::encodeScreenDrag(int sourceColumn, int sourceRow, const Screen &screen)
{
  QByteArray encodedData;
  QDataStream stream(&encodedData, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_6_0);
  stream << m_MimeMagic << m_MimeVersion << m_MimeFieldCount << static_cast<qint32>(sourceColumn)
         << static_cast<qint32>(sourceRow) << screen;
  return stream.status() == QDataStream::Ok ? encodedData : QByteArray();
}

bool ScreenSetupModel::isInBounds(int column, int row) const
{
  return column >= 0 && column < m_NumColumns && row >= 0 && row < m_NumRows;
}

const Screen *ScreenSetupModel::screenAt(int column, int row) const
{
  return isInBounds(column, row) ? &m_Screens[row * m_NumColumns + column] : nullptr;
}

Screen *ScreenSetupModel::screenAt(int column, int row)
{
  return isInBounds(column, row) ? &m_Screens[row * m_NumColumns + column] : nullptr;
}

bool ScreenSetupModel::decodeScreenDrag(
    const QByteArray &encodedData, int &sourceColumn, int &sourceRow, Screen &screen
) const
{
  auto data = encodedData;
  QDataStream stream(&data, QIODevice::ReadOnly);
  stream.setVersion(QDataStream::Qt_6_0);

  quint32 magic = 0;
  quint16 version = 0;
  quint16 fieldCount = 0;
  qint32 decodedColumn = -1;
  qint32 decodedRow = -1;
  stream >> magic >> version >> fieldCount >> decodedColumn >> decodedRow >> screen;

  if (stream.status() != QDataStream::Ok || !stream.atEnd() || magic != m_MimeMagic || version != m_MimeVersion ||
      fieldCount != m_MimeFieldCount || screen.isNull())
    return false;

  const auto isExternal = decodedColumn == -1 && decodedRow == -1;
  if (!isExternal && !isInBounds(decodedColumn, decodedRow))
    return false;

  sourceColumn = decodedColumn;
  sourceRow = decodedRow;
  return true;
}

bool ScreenSetupModel::dropMimeData(
    const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent
)
{
  if (action == Qt::IgnoreAction)
    return true;

  if (data == nullptr || !data->hasFormat(m_MimeType))
    return false;

  if (!parent.isValid() || parent.model() != this || row != -1 || column != -1)
    return false;

  int sourceColumn = -1;
  int sourceRow = -1;
  Screen droppedScreen;
  if (!decodeScreenDrag(data->data(m_MimeType), sourceColumn, sourceRow, droppedScreen))
    return false;

  auto *destinationScreen = screenAt(parent);
  if (destinationScreen == nullptr)
    return false;

  // don't drop screen onto itself
  if (sourceColumn == parent.column() && sourceRow == parent.row())
    return false;

  const auto isExternal = sourceColumn == -1 && sourceRow == -1;
  auto *sourceScreen = isExternal ? nullptr : screenAt(sourceColumn, sourceRow);
  if ((!isExternal && (sourceScreen == nullptr || sourceScreen->isNull())) ||
      (isExternal && !destinationScreen->isNull()))
    return false;

  if (auto oldScreen = Screen(*destinationScreen); !oldScreen.isNull() && sourceScreen != nullptr) {
    // mark the screen so it isn't deleted after the dragndrop succeeded
    // see ScreenSetupView::startDrag()
    oldScreen.setSwapped(true);
    *sourceScreen = oldScreen;
  }

  *destinationScreen = droppedScreen;

  Q_EMIT screensChanged();

  return true;
}

void ScreenSetupModel::addScreen(const Screen &newScreen)
{
  m_Screens.addScreenByPriority(newScreen);
  Q_EMIT screensChanged();
}

bool ScreenSetupModel::isFull() const
{
  auto emptyScreen = std::ranges::find_if(m_Screens, [](const Screen &item) { return item.isNull(); });
  return (static_cast<QList<Screen>::const_iterator>(emptyScreen) == m_Screens.cend());
}
