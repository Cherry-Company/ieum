/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "StyleUtilsTests.h"

#include "gui/StyleUtils.h"

#include <QEvent>
#include <QWidget>

namespace {

class PaletteReactiveWidget : public QWidget
{
public:
  int paletteChangeCount() const
  {
    return m_paletteChangeCount;
  }

protected:
  void changeEvent(QEvent *event) override
  {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
      ++m_paletteChangeCount;
      deskflow::gui::applyIeumMainWindowStyle(*this);
    }
  }

private:
  int m_paletteChangeCount = 0;
};

} // namespace

void StyleUtilsTests::applyMainWindowStyle_paletteChange_doesNotRecurse()
{
  PaletteReactiveWidget widget;

  deskflow::gui::applyIeumMainWindowStyle(widget);
  const auto styleSheet = widget.styleSheet();
  deskflow::gui::applyIeumMainWindowStyle(widget);

  QVERIFY(!styleSheet.isEmpty());
  QCOMPARE(widget.styleSheet(), styleSheet);
  QVERIFY(widget.paletteChangeCount() <= 1);
}

QTEST_MAIN(StyleUtilsTests)
