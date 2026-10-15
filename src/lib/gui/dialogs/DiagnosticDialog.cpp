/*
 * Ieum -- IME-native keyboard and mouse sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DiagnosticDialog.h"
#include "gui/StyleUtils.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QTextEdit>
#include <QVBoxLayout>

DiagnosticDialog::DiagnosticDialog(
    const deskflow::gui::diagnostic::Report &report, QWidget *parent, const QString &explanation,
    deskflow::gui::ExternalUrlOpener opener
)
    : QDialog(parent)
{
  setObjectName(QStringLiteral("diagnosticReport"));
  setWindowTitle(tr("Review and report a problem"));
  resize(760, 620);
  setMinimumSize(600, 440);
  deskflow::gui::applyIeumDialogStyle(*this);
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(22, 22, 22, 18);
  root->setSpacing(12);
  auto *intro = new QLabel(
      (explanation.isEmpty() ? QString() : explanation + QStringLiteral("\n\n")) +
          tr("1. Review the diagnostic below.\n2. Copy it and open the GitHub issue page.\n3. Paste it, describe what "
             "happened, and submit. GitHub requires an account.\nNothing is uploaded automatically."),
      this
  );
  intro->setTextFormat(Qt::PlainText);
  intro->setWordWrap(true);
  root->addWidget(intro);
  auto *preview = new QTextEdit(this);
  preview->setObjectName(QStringLiteral("diagnosticPreview"));
  preview->setAccessibleName(tr("Diagnostic preview"));
  preview->setReadOnly(true);
  preview->setPlainText(report.markdown);
  root->addWidget(preview, 1);
  auto *location = new QLabel(
      report.saved ? tr("Local copy: %1").arg(QDir::toNativeSeparators(report.filePath))
                   : tr("The local copy could not be saved. Use Save a copy below."),
      this
  );
  location->setTextFormat(Qt::PlainText);
  location->setWordWrap(true);
  location->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
  root->addWidget(location);
  auto *address = new QLineEdit(report.issueUrl.toString(QUrl::FullyEncoded), this);
  address->setObjectName(QStringLiteral("diagnosticIssueAddress"));
  address->setAccessibleName(tr("GitHub issue address. Copy this if your browser does not open."));
  address->setReadOnly(true);
  root->addWidget(address);
  auto *status = new QLabel(this);
  status->setObjectName(QStringLiteral("diagnosticActionStatus"));
  status->setWordWrap(true);
  root->addWidget(status);
  auto *actions = new QDialogButtonBox(QDialogButtonBox::Close, this);
  auto *copy = actions->addButton(tr("Copy diagnostic"), QDialogButtonBox::ActionRole);
  auto *open = actions->addButton(tr("Copy and open GitHub"), QDialogButtonBox::ActionRole);
  auto *save = actions->addButton(tr("Save a copy…"), QDialogButtonBox::ActionRole);
  copy->setObjectName(QStringLiteral("copyDiagnostic"));
  open->setObjectName(QStringLiteral("openDiagnosticIssue"));
  save->setObjectName(QStringLiteral("saveDiagnostic"));
#ifdef Q_OS_MACOS
  auto *nativeReports = actions->addButton(tr("Open macOS crash reports"), QDialogButtonBox::ActionRole);
  nativeReports->setAutoDefault(false);
  connect(nativeReports, &QPushButton::clicked, this, [status] {
    status->setText(
        deskflow::gui::diagnostic::openNativeCrashReports()
            ? tr("Crash report folder opened. Look for Ieum reports from the time of the crash.")
            : tr("The crash report folder could not be opened.")
    );
  });
#endif
  for (auto *button : {copy, open, save}) {
    button->setAutoDefault(false);
  }
  connect(copy, &QPushButton::clicked, this, [report, status] {
    deskflow::gui::diagnostic::copyToClipboard(report);
    status->setText(tr("Diagnostic copied. Paste it into the issue before submitting."));
  });
  connect(open, &QPushButton::clicked, this, [report, status, opener] {
    deskflow::gui::diagnostic::copyToClipboard(report);
    const auto result = deskflow::gui::launchExternalUrl(report.issueUrl, opener);
    status->setText(
        result.opened ? tr("Browser opened. Paste the diagnostic and submit the issue there.")
                      : tr("The browser could not be opened. The diagnostic is copied; copy the address above and open "
                           "it manually.")
    );
  });
  connect(save, &QPushButton::clicked, this, [this, report, status] {
    const auto path = QFileDialog::getSaveFileName(
        this, tr("Save diagnostic"), QStringLiteral("ieum-diagnostic-%1.md").arg(report.id.toLower()),
        tr("Markdown files (*.md)")
    );
    if (path.isEmpty()) {
      return;
    }
    QSaveFile file(path);
    const auto contents = report.markdown.toUtf8();
    if (file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit()) {
      status->setText(tr("Copy saved: %1").arg(QDir::toNativeSeparators(path)));
    } else {
      status->setText(tr("Could not save that file. Choose another location or copy the diagnostic."));
    }
  });
  connect(actions, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(actions);
}
