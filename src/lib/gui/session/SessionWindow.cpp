// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionWindow.h"
#include "SessionController.h"
#include "gui/StyleUtils.h"
#include "gui/dialogs/PlansDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWindow>

namespace deskflow::gui::session {
namespace {
class ScreenView final : public QWidget
{
public:
  using QWidget::QWidget;
  QImage image;

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    if (image.isNull())
      return;
    const auto size = image.size().scaled(this->size(), Qt::KeepAspectRatio);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRect(QPoint((width() - size.width()) / 2, (height() - size.height()) / 2), size), image);
  }
};
} // namespace
SessionWindow::SessionWindow(SessionController *controller, QWidget *parent) : QDialog(parent), m_controller(controller)
{
  setWindowTitle(tr("Share your workspace"));
  setObjectName("workspaceSharingPanel");
  resize(660, 690);
  setMinimumWidth(460);
  applyIeumDialogStyle(*this);
  const auto palette = QGuiApplication::palette();
  const auto window = palette.color(QPalette::Window), text = palette.color(QPalette::WindowText);
  const auto border = blendedColor(window, text, isDarkMode() ? 0.20 : 0.14);
  setStyleSheet(
      styleSheet() + QStringLiteral(R"(
QDialog#workspaceSharingPanel { background: %1; }
QLabel#sharingTitle { font-size: 24px; font-weight: 600; }
QLabel#sharingDescription, QLabel#sharingHint { color: %6; }
QComboBox#sharingComputer { background: %2; border: 1px solid %3; border-radius: 10px; padding: 8px 12px; min-height: 24px; }
QPushButton { background: %2; border: 1px solid %3; border-radius: 10px; padding: 7px 12px; }
QPushButton:hover { background: %4; }
QPushButton:focus { border-color: %5; }
QPushButton[sharingFeature="true"] { text-align: left; padding: 10px 16px; min-height: 32px; }
QPushButton#stopAllSharing { color: %5; }
QListWidget { background: %2; border: 1px solid %3; border-radius: 12px; padding: 10px; }
QListWidget::item { padding: 9px 8px; border-radius: 6px; }
QListWidget::item:selected { background: %4; color: %7; }
)")
                         .arg(
                             cssColor(window), cssColor(palette.color(QPalette::Base), 190), cssColor(border),
                             cssColor(blendedColor(window, text, 0.07)), cssColor(palette.color(QPalette::Highlight)),
                             cssColor(blendedColor(window, text, 0.72)), cssColor(text)
                         )
  );
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 24, 24, 24);
  layout->setSpacing(16);
  auto *title = new QLabel(tr("One workspace. Every computer."), this);
  title->setObjectName("sharingTitle");
  title->setWordWrap(true);
  auto font = title->font();
  font.setPointSize(font.pointSize() + 5);
  font.setWeight(QFont::DemiBold);
  title->setFont(font);
  layout->addWidget(title);
  auto *description =
      new QLabel(tr("Choose a connected computer. Sharing starts after that computer allows it."), this);
  description->setObjectName("sharingDescription");
  description->setWordWrap(true);
  layout->addWidget(description);
  m_peers = new QComboBox(this);
  m_peers->setObjectName("sharingComputer");
  m_peers->setAccessibleName(tr("Computer"));
  layout->addWidget(m_peers);
  auto *features = new QGridLayout;
  features->setSpacing(10);
  const QList<QPair<QString, QString>> actions = {
      {"remote-viewing", tr("View screen")},
      {"audio-sharing", tr("Listen to audio")},
      {"bidirectional-control", tr("Control selected computer")},
      {"simultaneous-input", tr("Add to simultaneous input")},
      {"coordinated-lock-wake", tr("Link lock and wake")}
  };
  int index = 0;
  for (const auto &[feature, label] : actions) {
    auto *button = new QPushButton(label, this);
    button->setMinimumHeight(54);
    button->setObjectName("sharing-" + feature);
    button->setProperty("sharingFeature", true);
    button->setAutoDefault(false);
    button->setToolTip(feature == "audio-sharing" ? tr("Professional or higher") : tr("Professional Max"));
    features->addWidget(button, index / 2, index % 2);
    ++index;
    connect(button, &QPushButton::clicked, this, [this, feature] { request(feature); });
  }
  auto *wake = new QPushButton(tr("Wake computer"), this);
  wake->setProperty("sharingFeature", true);
  wake->setAutoDefault(false);
  wake->setMinimumHeight(54);
  features->addWidget(wake, 2, 1);
  connect(wake, &QPushButton::clicked, this, [this] { (void)m_controller->wake(m_peers->currentData().toString()); });
  layout->addLayout(features);
  auto *hint = new QLabel(
      tr("Control uses your physical keyboard and mouse. Ctrl+Alt+Esc returns control. Simultaneous input also stays "
         "active on this computer."),
      this
  );
  hint->setWordWrap(true);
  hint->setObjectName("sharingHint");
  layout->addWidget(hint);
  m_sessions = new QListWidget(this);
  m_sessions->setAccessibleName(tr("Active sharing sessions"));
  layout->addWidget(m_sessions, 1);
  auto *controls = new QHBoxLayout;
  auto *stop = new QPushButton(tr("Stop selected"), this);
  auto *stopAll = new QPushButton(tr("Stop all sharing"), this);
  stopAll->setObjectName("stopAllSharing");
  auto *lock = new QPushButton(tr("Lock together"), this);
  controls->addWidget(stop);
  controls->addWidget(stopAll);
  controls->addStretch();
  controls->addWidget(lock);
  layout->addLayout(controls);
  connect(stop, &QPushButton::clicked, this, [this] {
    if (const auto *item = m_sessions->currentItem())
      m_controller->stop(item->data(Qt::UserRole).toString());
  });
  connect(stopAll, &QPushButton::clicked, m_controller, &SessionController::stopAll);
  connect(lock, &QPushButton::clicked, m_controller, &SessionController::lockTogether);
  m_status = new QLabel(tr("No sharing session is active."), this);
  m_status->setWordWrap(true);
  m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(m_status);
  auto *done = new QDialogButtonBox(QDialogButtonBox::Close, this);
  done->button(QDialogButtonBox::Close)->setText(tr("Close"));
  done->button(QDialogButtonBox::Close)->setIcon(QIcon());
  connect(done, &QDialogButtonBox::rejected, this, &QDialog::hide);
  layout->addWidget(done);
  auto *revoke = done->addButton(tr("Revoke saved permissions"), QDialogButtonBox::ActionRole);
  connect(revoke, &QPushButton::clicked, m_controller, &SessionController::revokePermissions);
  auto *plans = done->addButton(tr("Plans"), QDialogButtonBox::ActionRole);
  connect(plans, &QPushButton::clicked, this, [this] {
    auto *dialog = new PlansDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
  });
  connect(controller, &SessionController::status, m_status, &QLabel::setText);
  connect(controller, &SessionController::peersChanged, this, [this] { refreshPeers(); });
  connect(controller, &SessionController::consentRequested, this, &SessionWindow::askConsent);
  connect(controller, &SessionController::frameReceived, this, &SessionWindow::showFrame);
  connect(
      controller, &SessionController::sessionChanged, this,
      [this](const QString &id, const QString &peer, const QString &feature, bool active) {
        for (int i = m_sessions->count() - 1; i >= 0; --i)
          if (m_sessions->item(i)->data(Qt::UserRole).toString() == id)
            delete m_sessions->takeItem(i);
        if (active) {
          auto *item = new QListWidgetItem(tr("%1 · %2").arg(peer, featureName(feature)), m_sessions);
          item->setData(Qt::UserRole, id);
          m_status->setText(tr("Sharing is active. Closing this panel keeps it running; Stop all sharing ends it."));
        } else {
          if (auto view = m_viewers.take(id))
            view->close();
          if (m_sessions->count() == 0)
            m_status->setText(tr("No sharing session is active."));
        }
      }
  );
  refreshPeers();
}
QString SessionWindow::featureName(const QString &feature) const
{
  if (feature == "audio-sharing")
    return tr("System audio");
  if (feature == "remote-viewing")
    return tr("Screen viewing");
  if (feature == "bidirectional-control")
    return tr("Keyboard and mouse control");
  if (feature == "simultaneous-input")
    return tr("Simultaneous keyboard and mouse");
  if (feature == "coordinated-lock-wake")
    return tr("Lock and wake");
  return {};
}
void SessionWindow::refreshPeers()
{
  const auto current = m_peers->currentData().toString();
  m_peers->clear();
  for (const auto &peer : m_controller->peers())
    m_peers->addItem(peer, peer);
  for (const auto &peer : m_controller->rememberedPeers())
    if (!m_controller->peers().contains(peer))
      m_peers->addItem(tr("%1 · offline").arg(peer), peer);
  const auto index = m_peers->findData(current);
  if (index >= 0)
    m_peers->setCurrentIndex(index);
  if (m_peers->count() == 0)
    m_status->setText(tr("Start sharing on both computers to see available devices."));
}
void SessionWindow::request(const QString &feature)
{
  (void)m_controller->request(m_peers->currentData().toString(), feature);
}
void SessionWindow::askConsent(const QString &id, const QString &peer, const QString &feature)
{
  auto *box = new QMessageBox(
      QMessageBox::Question, tr("Allow sharing?"),
      tr("%1 wants to use %2 on this computer.").arg(peer, featureName(feature)), QMessageBox::Yes | QMessageBox::No,
      parentWidget()
  );
  box->setDefaultButton(QMessageBox::No);
  box->setAttribute(Qt::WA_DeleteOnClose);
  auto *remember = new QCheckBox(tr("Allow this feature while this computer stays connected"), box);
  box->setCheckBox(remember);
  if (feature == "coordinated-lock-wake")
    box->setInformativeText(
        tr("This allows coordinated locking and waking the display. It never unlocks your login screen.")
    );
  else if (feature == "audio-sharing")
    box->setInformativeText(
        tr("Your computer's audio will be sent to this trusted computer. Microphone audio is not recorded.")
    );
  else if (feature == "remote-viewing")
    box->setInformativeText(tr("Your primary screen will be visible to this trusted computer until you stop sharing."));
  else
    box->setInformativeText(
        tr("The other computer can type and click here. Local input or Ctrl+Alt+Esc takes control back.")
    );
  connect(box, &QMessageBox::finished, this, [this, box, remember, id](int result) {
    m_controller->consent(id, result == QMessageBox::Yes, remember->isChecked());
  });
  connect(
      m_controller, &SessionController::sessionChanged, box,
      [box, id](const QString &session, const QString &, const QString &, bool active) {
        if (session == id && !active)
          box->reject();
      }
  );
  box->open();
}
void SessionWindow::showFrame(const QString &id, const QString &peer, const QImage &image)
{
  auto *dialog = m_viewers.value(id).data();
  if (!dialog) {
    dialog = new QDialog(this, Qt::Window);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("%1 — Screen").arg(peer));
    dialog->resize(1000, 650);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *view = new ScreenView(dialog);
    view->setObjectName("remoteScreenImage");
    layout->addWidget(view, 1);
    auto *close = new QPushButton(tr("Stop viewing"), dialog);
    layout->addWidget(close);
    connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    connect(dialog, &QDialog::finished, this, [this, id] {
      m_viewers.remove(id);
      m_controller->stop(id);
    });
    m_viewers.insert(id, dialog);
    dialog->show();
  }
  auto *view = static_cast<ScreenView *>(dialog->findChild<QWidget *>("remoteScreenImage"));
  view->image = image;
  view->update();
}
} // namespace deskflow::gui::session
