/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenSetupView.h"

#include "ScreenSetupModel.h"
#include "dialogs/ScreenSettingsDialog.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
#include <QResizeEvent>

ScreenSetupView::ScreenSetupView(QWidget *parent) : QTableView(parent)
{
  setDropIndicatorShown(true);
  setDragDropMode(DragDrop);
  setSelectionMode(SingleSelection);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  setIconSize(QSize(96, 96));
  horizontalHeader()->hide();
  verticalHeader()->hide();
}

void ScreenSetupView::setModel(QAbstractItemModel *model)
{
  QTableView::setModel(model);
  setTableSize();
}

ScreenSetupModel *ScreenSetupView::model() const
{
  return qobject_cast<ScreenSetupModel *>(QTableView::model());
}

void ScreenSetupView::showScreenConfig(int col, int row)
{
  auto *selectedScreen = model() == nullptr ? nullptr : model()->screenAt(col, row);
  if (selectedScreen == nullptr || selectedScreen->isNull())
    return;

  ScreenSettingsDialog dlg(this, selectedScreen, &model()->m_Screens);
  dlg.exec();
  Q_EMIT model()->screensChanged();
}

void ScreenSetupView::setTableSize()
{
  for (int i = 0; i < model()->columnCount(); i++)
    setColumnWidth(i, width() / model()->columnCount());

  for (int i = 0; i < model()->rowCount(); i++)
    setRowHeight(i, height() / model()->rowCount());
}

void ScreenSetupView::resizeEvent(QResizeEvent *event)
{
  setTableSize();
  event->ignore();
}

void ScreenSetupView::mouseDoubleClickEvent(QMouseEvent *event)
{
  event->ignore();
  if (model() == nullptr || event->button() != Qt::LeftButton)
    return;

  const auto col = columnAt(event->pos().x());
  const auto row = rowAt(event->pos().y());
  const auto *selectedScreen = model()->screenAt(col, row);
  if (selectedScreen == nullptr || selectedScreen->isNull())
    return;

  event->accept();
  showScreenConfig(col, row);
}

void ScreenSetupView::dragEnterEvent(QDragEnterEvent *event)
{
  // we accept anything that enters us by a drag as long as the
  // mime type is okay. anything else is dealt with in dragMoveEvent()
  if (event->mimeData()->hasFormat(ScreenSetupModel::mimeType()))
    event->accept();
  else
    event->ignore();
}

void ScreenSetupView::dragMoveEvent(QDragMoveEvent *event)
{
  event->ignore();
  if (model() == nullptr || !event->mimeData()->hasFormat(ScreenSetupModel::mimeType()))
    return;

  const auto point = event->position().toPoint();
  const auto col = columnAt(point.x());
  const auto row = rowAt(point.y());
  const auto *destinationScreen = model()->screenAt(col, row);
  if (destinationScreen == nullptr)
    return;

  // where does the event come from? myself or someone else?
  if (event->source() == this) {
    // myself is ok, but then it must be a move action, never a copy
    event->setDropAction(Qt::MoveAction);
    event->accept();
  } else if (destinationScreen->isNull()) {
    event->acceptProposedAction();
  }
}

// this is reimplemented from QAbstractItemView::startDrag()
void ScreenSetupView::startDrag(Qt::DropActions)
{
  QModelIndexList indexes = selectedIndexes();

  if (indexes.count() != 1)
    return;

  QMimeData *pData = model()->mimeData(indexes);
  if (pData == nullptr)
    return;

  auto *draggedScreen = model()->screenAt(indexes[0]);
  if (draggedScreen == nullptr) {
    delete pData;
    return;
  }

  const QPixmap &pixmap = draggedScreen->pixmap();
  auto *pDrag = new QDrag(this);
  pDrag->setPixmap(pixmap);
  pDrag->setMimeData(pData);
  pDrag->setHotSpot(QPoint(iconSize().width() / 2, iconSize().height() / 2));

  if (pDrag->exec(Qt::MoveAction, Qt::MoveAction) == Qt::MoveAction) {
    selectionModel()->clear();

    auto *sourceScreen = model()->screenAt(indexes[0]);
    if (sourceScreen == nullptr)
      return;

    // make sure to only delete the drag source if screens weren't swapped
    // see ScreenSetupModel::dropMimeData
    if (!sourceScreen->swapped())
      *sourceScreen = Screen();
    else
      sourceScreen->setSwapped(false);

    Q_EMIT model()->screensChanged();
  }
}

void ScreenSetupView::initViewItemOption(QStyleOptionViewItem *option) const
{
  // HACK make a basic widget and init from it
  auto w = new QWidget();
  option->initFrom(w);
  w->deleteLater();
  delete w;

  option->decorationSize = QSize(96, 96);
  option->showDecorationSelected = true;
  option->decorationPosition = QStyleOptionViewItem::Top;
  option->decorationAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
  option->displayAlignment = Qt::AlignTop | Qt::AlignHCenter;
  option->textElideMode = Qt::ElideMiddle;
}
