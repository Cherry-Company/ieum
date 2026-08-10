/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "gui/ProductIdentity.h"
#include "gui/StyleUtils.h"

#include <QClipboard>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()}
{
  ui->setupUi(this);
  deskflow::gui::applyIeumDialogStyle(*this);
  setWindowTitle(tr("About %1").arg(deskflow::gui::productDisplayName()));

  const int px = (fontMetrics().height() * 6);
  const QSize pixmapSize(px, px);
  ui->lblIcon->setFixedSize(pixmapSize);

  ui->lblIcon->setPixmap(QPixmap(QIcon::fromTheme(kRevFqdnName).pixmap(QSize().scaled(pixmapSize, Qt::KeepAspectRatio)))
  );

  ui->btnCopyVersion->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy));
  connect(ui->btnCopyVersion, &QPushButton::clicked, this, &AboutDialog::copyVersionText);

  ui->lblVersion->setText(kDisplayVersion);
  ui->lblName->setText(deskflow::gui::productDisplayName());
  ui->lblDescription->setText(deskflow::gui::productTagline());
  ui->lblCopyright->setText(kCopyright);
  ui->groupBox->setTitle(tr("Credits"));

  // Use non-breaking space in each awesome dev name so names are not split across lines.
  QStringList devsNbsp;
  for (const auto &dev : s_awesomeDevs) {
    QString withNbsp = dev;
    devsNbsp.append(withNbsp.replace(" ", QStringLiteral("&nbsp;")));
  }

  ui->lblImportantDevs->setTextFormat(Qt::RichText);
  ui->lblImportantDevs->setText(QStringLiteral("<b>%1</b> %2<br><b>%3</b> %4<br><br><b>%5</b><br>%6")
                                    .arg(
                                        tr("Project contributors:"), QStringLiteral("Ieum&nbsp;contributors"),
                                        tr("Maintained by:"), QStringLiteral("Cherry&nbsp;Company"),
                                        tr("Upstream contributors:"), devsNbsp.join(QStringLiteral(", "))
                                    ));

  ui->btnOk->setDefault(true);
  connect(ui->btnOk, &QPushButton::clicked, this, &AboutDialog::close);
}

void AboutDialog::copyVersionText() const
{
  QString infoString =
      QStringLiteral(
          "%1: %2 (%3)\nProject contributors: Ieum contributors\nMaintained by: Cherry Company\nQt: %4\nSystem: %5"
      )
          .arg(
              deskflow::gui::productDisplayName(), kVersion, kVersionGitSha, qVersion(), QSysInfo::prettyProductName()
          );
  if (Settings::isPortableMode()) {
    infoString.append(QStringLiteral("\nPortable Mode"));
  }
#ifdef Q_OS_LINUX
  infoString.append(QStringLiteral("\nSession: %1 (%2)")
                        .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_TYPE")));
#endif
  QGuiApplication::clipboard()->setText(infoString);
}

AboutDialog::~AboutDialog() = default;
