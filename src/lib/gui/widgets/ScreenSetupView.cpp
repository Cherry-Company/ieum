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

#include <QCoreApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QStyledItemDelegate>

namespace {
class ComputerCardDelegate final : public QStyledItemDelegate
{
public:
  explicit ComputerCardDelegate(QObject *parent) : QStyledItemDelegate(parent)
  {
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
  {
    const auto palette = QGuiApplication::palette();
    const auto card = option.rect.adjusted(6, 6, -6, -6);
    const auto name = index.data().toString();
    const auto border = palette.color(QPalette::Mid);
    painter->save();
    painter->fillRect(option.rect, palette.color(QPalette::Base));
    painter->setRenderHint(QPainter::Antialiasing);
    if (name.isEmpty()) {
      auto faint = border;
      faint.setAlpha(80);
      painter->setPen(QPen(faint, 1, Qt::DashLine));
      painter->setBrush(Qt::NoBrush);
      painter->drawRoundedRect(card, 8, 8);
      painter->restore();
      return;
    }
    const bool selected = option.state & QStyle::State_Selected;
    const bool host = index.data(Qt::UserRole).toBool();
    const auto accent = palette.color(QPalette::Highlight);
    painter->setPen(QPen(selected ? accent : border, selected ? 2 : 1));
    painter->setBrush(palette.color(QPalette::Base));
    painter->drawRoundedRect(card, 8, 8);
    const auto screen = QRectF(card.center().x() - 21, card.center().y() - 38, 42, 29);
    auto fill = accent;
    fill.setAlpha(host ? 50 : 18);
    painter->setBrush(fill);
    painter->setPen(QPen(accent, 2));
    painter->drawRoundedRect(screen, 3, 3);
    painter->drawLine(QPointF(screen.center().x(), screen.bottom()), QPointF(screen.center().x(), screen.bottom() + 7));
    painter->drawLine(
        QPointF(screen.center().x() - 10, screen.bottom() + 7), QPointF(screen.center().x() + 10, screen.bottom() + 7)
    );
    auto font = option.font;
    font.setBold(true);
    painter->setFont(font);
    painter->setPen(palette.color(QPalette::Text));
    painter->drawText(
        QRect(card.left() + 6, card.center().y() + 10, card.width() - 12, 22), Qt::AlignCenter,
        QFontMetrics(font).elidedText(name, Qt::ElideMiddle, card.width() - 12)
    );
    font.setBold(false);
    painter->setFont(font);
    painter->setPen(palette.color(QPalette::PlaceholderText));
    const auto subtitle = host ? QCoreApplication::translate("ScreenSetupView", "This host")
                               : QCoreApplication::translate("ScreenSetupView", "Other computer");
    painter->drawText(QRect(card.left() + 6, card.center().y() + 34, card.width() - 12, 20), Qt::AlignCenter, subtitle);
    painter->restore();
  }
};
} // namespace

ScreenSetupView::ScreenSetupView(QWidget *parent) : QTableView(parent)
{
  setItemDelegate(new ComputerCardDelegate(this));
  setShowGrid(false);
  setEditTriggers(NoEditTriggers);
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
  if (model() == nullptr || model()->columnCount() <= 0 || model()->rowCount() <= 0) {
    return;
  }
  for (int i = 0; i < model()->columnCount(); i++)
    setColumnWidth(i, viewport()->width() / model()->columnCount());

  for (int i = 0; i < model()->rowCount(); i++)
    setRowHeight(i, viewport()->height() / model()->rowCount());
}

void ScreenSetupView::resizeEvent(QResizeEvent *event)
{
  QTableView::resizeEvent(event);
  setTableSize();
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
  option->initFrom(this);

  option->decorationSize = QSize(96, 96);
  option->showDecorationSelected = true;
  option->decorationPosition = QStyleOptionViewItem::Top;
  option->decorationAlignment = Qt::AlignHCenter | Qt::AlignVCenter;
  option->displayAlignment = Qt::AlignTop | Qt::AlignHCenter;
  option->textElideMode = Qt::ElideMiddle;
}
