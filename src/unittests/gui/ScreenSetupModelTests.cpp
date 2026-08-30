/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "gui/ScreenSetupModel.h"
#include "gui/widgets/ScreenSetupView.h"

#include <QDragMoveEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTest>

#include <memory>

namespace {

class TestScreenSetupModel final : public ScreenSetupModel
{
public:
  using ScreenSetupModel::ScreenSetupModel;

  QModelIndex uncheckedIndex(const int row, const int column) const
  {
    return createIndex(row, column);
  }

  const Screen *screenAtForTest(const int column, const int row) const
  {
    return screenAt(column, row);
  }

  bool
  drop(const QMimeData *data, const Qt::DropAction action, const int row, const int column, const QModelIndex &parent)
  {
    return dropMimeData(data, action, row, column, parent);
  }
};

class TestScreenSetupView final : public ScreenSetupView
{
public:
  TestScreenSetupView() : ScreenSetupView(nullptr)
  {
  }

  void dispatchDoubleClick(QMouseEvent *event)
  {
    mouseDoubleClickEvent(event);
  }

  void dispatchDragMove(QDragMoveEvent *event)
  {
    dragMoveEvent(event);
  }
};

ScreenList createScreens()
{
  ScreenList screens(2);
  screens.resize(4);
  screens[0] = Screen(QStringLiteral("source"));
  screens[1] = Screen(QStringLiteral("occupied"));
  return screens;
}

} // namespace

class ScreenSetupModelTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void boundsRejectNegativeAndCountCoordinates()
  {
    auto screens = createScreens();
    TestScreenSetupModel model(screens, 2, 2);

    QVERIFY(model.isInBounds(0, 0));
    QVERIFY(model.isInBounds(1, 1));
    QVERIFY(!model.isInBounds(-1, 0));
    QVERIFY(!model.isInBounds(0, -1));
    QVERIFY(!model.isInBounds(model.columnCount(), 0));
    QVERIFY(!model.isInBounds(0, model.rowCount()));
    QVERIFY(model.screenAtForTest(-1, 0) == nullptr);
    QVERIFY(model.screenAtForTest(0, model.rowCount()) == nullptr);

    const auto pastLastRow = model.uncheckedIndex(model.rowCount(), 0);
    const auto pastLastColumn = model.uncheckedIndex(0, model.columnCount());
    QVERIFY(pastLastRow.isValid());
    QVERIFY(pastLastColumn.isValid());
    QVERIFY(!model.data(pastLastRow).isValid());
    QVERIFY(!model.data(pastLastColumn).isValid());
    QCOMPARE(model.flags(pastLastRow), Qt::NoItemFlags);
    QCOMPARE(model.flags(pastLastColumn), Qt::NoItemFlags);
  }

  void mimeDataRequiresExactlyOneInBoundsScreen()
  {
    auto screens = createScreens();
    TestScreenSetupModel model(screens, 2, 2);

    std::unique_ptr<QMimeData> valid(model.mimeData({model.index(0, 0)}));
    QVERIFY(valid != nullptr);
    QVERIFY(valid->hasFormat(ScreenSetupModel::mimeType()));

    std::unique_ptr<QMimeData> empty(model.mimeData({}));
    QVERIFY(empty == nullptr);

    std::unique_ptr<QMimeData> multiple(model.mimeData({model.index(0, 0), model.index(0, 1)}));
    QVERIFY(multiple == nullptr);

    std::unique_ptr<QMimeData> outOfBounds(model.mimeData({model.uncheckedIndex(model.rowCount(), 0)}));
    QVERIFY(outOfBounds == nullptr);
  }

  void malformedDragPayloadIsRejected_data()
  {
    QTest::addColumn<QByteArray>("payload");

    const auto validPayload = ScreenSetupModel::encodeScreenDrag(-1, -1, Screen(QStringLiteral("new-screen")));
    auto truncated = validPayload;
    truncated.chop(1);
    auto trailing = validPayload;
    trailing.append('x');
    auto wrongFieldCount = validPayload;
    QVERIFY(wrongFieldCount.size() > 7);
    wrongFieldCount[7] = static_cast<char>(4);

    QTest::newRow("arbitrary") << QByteArrayLiteral("not-a-screen");
    QTest::newRow("truncated") << truncated;
    QTest::newRow("trailing-field") << trailing;
    QTest::newRow("wrong-field-count") << wrongFieldCount;
    QTest::newRow("source-column-out-of-bounds")
        << ScreenSetupModel::encodeScreenDrag(2, 0, Screen(QStringLiteral("new-screen")));
    QTest::newRow("source-row-out-of-bounds")
        << ScreenSetupModel::encodeScreenDrag(0, 2, Screen(QStringLiteral("new-screen")));
    QTest::newRow("partial-external-sentinel")
        << ScreenSetupModel::encodeScreenDrag(-1, 0, Screen(QStringLiteral("new-screen")));
    QTest::newRow("empty-screen") << ScreenSetupModel::encodeScreenDrag(-1, -1, Screen());
  }

  void malformedDragPayloadIsRejected()
  {
    QFETCH(QByteArray, payload);

    auto screens = createScreens();
    const auto originalScreens = screens;
    TestScreenSetupModel model(screens, 2, 2);
    QSignalSpy changedSpy(&model, &ScreenSetupModel::screensChanged);
    QMimeData mimeData;
    mimeData.setData(ScreenSetupModel::mimeType(), payload);

    QVERIFY(!model.drop(&mimeData, Qt::CopyAction, -1, -1, model.index(1, 1)));
    QVERIFY(screens == originalScreens);
    QCOMPARE(changedSpy.count(), 0);
  }

  void outOfBoundsDestinationIsRejected()
  {
    auto screens = createScreens();
    const auto originalScreens = screens;
    TestScreenSetupModel model(screens, 2, 2);
    QMimeData mimeData;
    mimeData.setData(
        ScreenSetupModel::mimeType(), ScreenSetupModel::encodeScreenDrag(-1, -1, Screen(QStringLiteral("new-screen")))
    );

    QVERIFY(!model.drop(&mimeData, Qt::CopyAction, -1, -1, model.uncheckedIndex(model.rowCount(), 0)));
    QVERIFY(screens == originalScreens);
  }

  void validExternalDropChangesOnlyTheDestination()
  {
    auto screens = createScreens();
    TestScreenSetupModel model(screens, 2, 2);
    QSignalSpy changedSpy(&model, &ScreenSetupModel::screensChanged);
    QMimeData mimeData;
    mimeData.setData(
        ScreenSetupModel::mimeType(), ScreenSetupModel::encodeScreenDrag(-1, -1, Screen(QStringLiteral("new-screen")))
    );

    QVERIFY(model.drop(&mimeData, Qt::CopyAction, -1, -1, model.index(1, 1)));
    QCOMPARE(screens[0].name(), QStringLiteral("source"));
    QCOMPARE(screens[1].name(), QStringLiteral("occupied"));
    QCOMPARE(screens[3].name(), QStringLiteral("new-screen"));
    QCOMPARE(changedSpy.count(), 1);
  }

  void viewIgnoresPointerEventsOutsideTheGrid()
  {
    auto screens = createScreens();
    TestScreenSetupModel model(screens, 2, 2);
    TestScreenSetupView view;
    view.resize(400, 300);
    view.setModel(&model);

    const QPointF outside(-1, -1);
    QMouseEvent doubleClick(
        QEvent::MouseButtonDblClick, outside, outside, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier
    );
    doubleClick.accept();
    view.dispatchDoubleClick(&doubleClick);
    QVERIFY(!doubleClick.isAccepted());

    QMimeData mimeData;
    mimeData.setData(
        ScreenSetupModel::mimeType(), ScreenSetupModel::encodeScreenDrag(-1, -1, Screen(QStringLiteral("new-screen")))
    );
    QDragMoveEvent dragMove(QPoint(-1, -1), Qt::CopyAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
    dragMove.accept();
    view.dispatchDragMove(&dragMove);
    QVERIFY(!dragMove.isAccepted());
  }
};

QTEST_MAIN(ScreenSetupModelTests)

#include "ScreenSetupModelTests.moc"
