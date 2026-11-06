// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#pragma once
#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <memory>
#include <optional>

namespace deskflow::gui::session {
struct Input
{
  enum class Kind
  {
    Key,
    Button,
    Move,
    Wheel,
    Escape
  };
  Kind kind = Kind::Move;
  int code = 0;
  bool down = false;
  int x = 0;
  int y = 0;
  QJsonObject toJson() const;
  static std::optional<Input> fromJson(const QJsonObject &object);
};

class SessionPlatform : public QObject
{
  Q_OBJECT
public:
  enum class InputMode
  {
    Observe,
    Control,
    Broadcast
  };
  explicit SessionPlatform(QObject *parent = nullptr) : QObject(parent)
  {
  }
  ~SessionPlatform() override = default;
  virtual QImage screen();
  virtual bool requestScreenPermission()
  {
    return true;
  }
  virtual bool startAudioCapture() = 0;
  virtual void stopAudioCapture() = 0;
  virtual bool playAudio(const QByteArray &pcm) = 0;
  virtual void stopAudioPlayback() = 0;
  virtual bool startInputCapture(InputMode mode) = 0;
  virtual void stopInputCapture() = 0;
  virtual bool inject(const Input &input) = 0;
  virtual void releaseInput() = 0;
  virtual bool lockDesktop() = 0;
  virtual bool wakeDisplays() = 0;
  virtual bool locked() = 0;
  QString lastError() const
  {
    return m_error;
  }
Q_SIGNALS:
  void audioCaptured(const QByteArray &pcm);
  void inputCaptured(const deskflow::gui::session::Input &input);
  void failure(const QString &feature, const QString &message);

protected:
  bool fail(const QString &message)
  {
    m_error = message;
    return false;
  }
  QString m_error;
};
std::unique_ptr<SessionPlatform> createSessionPlatform();
} // namespace deskflow::gui::session
Q_DECLARE_METATYPE(deskflow::gui::session::Input)
