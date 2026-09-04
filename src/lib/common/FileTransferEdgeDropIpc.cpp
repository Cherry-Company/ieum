/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "common/FileTransferEdgeDropIpc.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>
#include <limits>

namespace deskflow::ipc {
namespace {

constexpr int kVersion = 1;
constexpr qsizetype kMaxPathCount = 100;
constexpr qsizetype kMaxEncodedValueBytes = 60 * 1024;

bool isBase64Url(const QString &value)
{
  if (value.isEmpty()) {
    return false;
  }

  for (const auto character : value) {
    const auto unicode = character.unicode();
    const auto alphaNumeric =
        (unicode >= 'A' && unicode <= 'Z') || (unicode >= 'a' && unicode <= 'z') || (unicode >= '0' && unicode <= '9');
    if (!alphaNumeric && unicode != '-' && unicode != '_') {
      return false;
    }
  }
  return true;
}

FileTransferEdgeDropIpcEncodeResult encodeFailure(FileTransferEdgeDropIpcError error)
{
  return {.error = error};
}

FileTransferEdgeDropIpcDecodeResult decodeFailure(FileTransferEdgeDropIpcError error)
{
  return {.error = error, .value = std::nullopt};
}

bool isValidDirection(Direction direction)
{
  return direction >= Direction::FirstDirection && direction <= Direction::LastDirection;
}

FileTransferEdgeDropIpcError validatePaths(const QStringList &paths)
{
  if (paths.isEmpty()) {
    return FileTransferEdgeDropIpcError::EmptyPath;
  }
  if (paths.size() > kMaxPathCount) {
    return FileTransferEdgeDropIpcError::TooManyPaths;
  }

  for (const auto &path : paths) {
    if (path.isEmpty()) {
      return FileTransferEdgeDropIpcError::EmptyPath;
    }
    if (!QDir::isAbsolutePath(path)) {
      return FileTransferEdgeDropIpcError::RelativePath;
    }
  }
  return FileTransferEdgeDropIpcError::None;
}

FileTransferEdgeDropIpcError validateValue(const FileTransferEdgeDropIpcValue &value)
{
  if (!isValidDirection(value.direction)) {
    return FileTransferEdgeDropIpcError::InvalidDirection;
  }
  return validatePaths(value.paths);
}

std::optional<std::int32_t> parseInt32(const QJsonValue &value, FileTransferEdgeDropIpcError &error)
{
  if (!value.isDouble()) {
    error = FileTransferEdgeDropIpcError::InvalidFieldType;
    return std::nullopt;
  }

  const auto number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
  if (!std::isfinite(number) || std::trunc(number) != number || number < (std::numeric_limits<std::int32_t>::min)() ||
      number > (std::numeric_limits<std::int32_t>::max)()) {
    error = FileTransferEdgeDropIpcError::InvalidInteger;
    return std::nullopt;
  }

  return static_cast<std::int32_t>(number);
}

std::optional<QByteArray> decodeBase64Url(const QString &value)
{
  if (!isBase64Url(value)) {
    return std::nullopt;
  }

  const auto encoded = value.toLatin1();
  const auto result =
      QByteArray::fromBase64Encoding(encoded, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
  if (!result) {
    return std::nullopt;
  }

  if (result.decoded.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals) != encoded) {
    return std::nullopt;
  }
  return result.decoded;
}

QByteArray encodeJson(const FileTransferEdgeDropIpcValue &value)
{
  QJsonArray pathArray;
  for (const auto &path : value.paths) {
    pathArray.append(path);
  }

  QByteArray json;
  json.reserve(pathArray.size() * 64);
  json.append(R"({"v":1,"d":)");
  json.append(QByteArray::number(static_cast<int>(value.direction)));
  json.append(R"(,"x":)");
  json.append(QByteArray::number(value.x));
  json.append(R"(,"y":)");
  json.append(QByteArray::number(value.y));
  json.append(R"(,"p":)");
  json.append(QJsonDocument(pathArray).toJson(QJsonDocument::Compact));
  json.append('}');
  return json;
}

} // namespace

FileTransferEdgeDropIpcEncodeResult encodeFileTransferEdgeDropIpc(const FileTransferEdgeDropIpcValue &value)
{
  const auto validation = validateValue(value);
  if (validation != FileTransferEdgeDropIpcError::None) {
    return encodeFailure(validation);
  }

  const auto json = encodeJson(value);
  const auto encoded = QString::fromLatin1(json.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  if (encoded.toUtf8().size() > kMaxEncodedValueBytes) {
    return encodeFailure(FileTransferEdgeDropIpcError::EncodedValueTooLarge);
  }

  return {.encodedValue = encoded};
}

FileTransferEdgeDropIpcDecodeResult decodeFileTransferEdgeDropIpc(const QString &encodedValue)
{
  if (encodedValue.toUtf8().size() > kMaxEncodedValueBytes) {
    return decodeFailure(FileTransferEdgeDropIpcError::EncodedValueTooLarge);
  }

  const auto decoded = decodeBase64Url(encodedValue);
  if (!decoded.has_value()) {
    return decodeFailure(FileTransferEdgeDropIpcError::InvalidBase64);
  }

  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(*decoded, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return decodeFailure(FileTransferEdgeDropIpcError::InvalidJson);
  }

  const auto object = document.object();
  for (const auto &key : {QStringLiteral("v"), QStringLiteral("d"), QStringLiteral("x"), QStringLiteral("y"),
                          QStringLiteral("p")}) {
    if (!object.contains(key)) {
      return decodeFailure(FileTransferEdgeDropIpcError::MissingField);
    }
  }
  if (object.size() != 5) {
    return decodeFailure(FileTransferEdgeDropIpcError::UnknownField);
  }
  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    const auto &key = it.key();
    if (key != QLatin1String("v") && key != QLatin1String("d") && key != QLatin1String("x") &&
        key != QLatin1String("y") && key != QLatin1String("p")) {
      return decodeFailure(FileTransferEdgeDropIpcError::UnknownField);
    }
  }

  FileTransferEdgeDropIpcError error = FileTransferEdgeDropIpcError::None;
  const auto version = parseInt32(object.value(QStringLiteral("v")), error);
  if (!version.has_value()) {
    return decodeFailure(error);
  }
  if (*version != kVersion) {
    return decodeFailure(FileTransferEdgeDropIpcError::InvalidVersion);
  }

  const auto directionValue = parseInt32(object.value(QStringLiteral("d")), error);
  if (!directionValue.has_value()) {
    return decodeFailure(error);
  }
  const auto direction = static_cast<Direction>(*directionValue);
  if (!isValidDirection(direction)) {
    return decodeFailure(FileTransferEdgeDropIpcError::InvalidDirection);
  }

  const auto x = parseInt32(object.value(QStringLiteral("x")), error);
  if (!x.has_value()) {
    return decodeFailure(error);
  }

  const auto y = parseInt32(object.value(QStringLiteral("y")), error);
  if (!y.has_value()) {
    return decodeFailure(error);
  }

  const auto pathsValue = object.value(QStringLiteral("p"));
  if (!pathsValue.isArray()) {
    return decodeFailure(FileTransferEdgeDropIpcError::InvalidFieldType);
  }

  QStringList paths;
  const auto pathsArray = pathsValue.toArray();
  if (pathsArray.size() > kMaxPathCount) {
    return decodeFailure(FileTransferEdgeDropIpcError::TooManyPaths);
  }
  if (pathsArray.isEmpty()) {
    return decodeFailure(FileTransferEdgeDropIpcError::EmptyPath);
  }

  paths.reserve(pathsArray.size());
  for (const auto &entry : pathsArray) {
    if (!entry.isString()) {
      return decodeFailure(FileTransferEdgeDropIpcError::InvalidPathType);
    }

    const auto path = entry.toString();
    if (path.isEmpty()) {
      return decodeFailure(FileTransferEdgeDropIpcError::EmptyPath);
    }
    if (!QDir::isAbsolutePath(path)) {
      return decodeFailure(FileTransferEdgeDropIpcError::RelativePath);
    }
    paths.append(path);
  }

  return {.value = FileTransferEdgeDropIpcValue{.direction = direction, .x = *x, .y = *y, .paths = std::move(paths)}};
}

} // namespace deskflow::ipc
