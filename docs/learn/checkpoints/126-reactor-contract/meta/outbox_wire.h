#ifndef STUDY_META_OUTBOX_WIRE_H
#define STUDY_META_OUTBOX_WIRE_H

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "meta/wire.h"
#include "net/match_submission.h"

namespace study_meta {

// pending may already have reached the server; stopped halts automatic sends.
// confirmed means a matching receipt has been observed.
enum class OutboxStatus { pending, stopped, confirmed };

struct OutboxDocument {
  std::string origin;
  study_net::MatchRecord request;
  OutboxStatus status = OutboxStatus::pending;
  std::optional<study_net::Receipt> receipt;
};

// Structural comparison only: a match means the receipt echoes the
// request's key and players and carries a positive row. It says nothing
// about which writer produced either document, so it does not authenticate.
inline bool receipt_matches(const study_net::MatchRecord& request,
                            const study_net::Receipt& receipt) {
  return receipt.row > 0 && receipt.key == request.key &&
         receipt.player_a == request.player_a &&
         receipt.player_b == request.player_b;
}

inline bool valid_outbox(const OutboxDocument& doc) {
  if (doc.origin.empty()) return false;
  if (!study_net::MatchSubmission(1).prepare(doc.request)) return false;

  switch (doc.status) {
    case OutboxStatus::confirmed:
      return doc.receipt.has_value() &&
             receipt_matches(doc.request, *doc.receipt);
    case OutboxStatus::pending:
    case OutboxStatus::stopped:
      return !doc.receipt.has_value();
  }
  return false;  // unknown enum value
}

inline std::string outbox_json(const OutboxDocument& doc) {
  if (!valid_outbox(doc)) {
    throw std::invalid_argument("invalid outbox document");
  }

  // Exactly five flat fields; request/receipt are JSON payloads stored as
  // strings so the existing small flat-wire parser can reuse them directly.
  Json j = Json::object();
  j["version"] = 1;
  j["origin"] = doc.origin;
  j["request"] = record_json(doc.request).dump();
  switch (doc.status) {
    case OutboxStatus::pending:
      j["status"] = "pending";
      break;
    case OutboxStatus::stopped:
      j["status"] = "stopped";
      break;
    case OutboxStatus::confirmed:
      j["status"] = "confirmed";
      break;
    default:
      throw std::invalid_argument("invalid outbox status");
  }
  j["receipt"] = doc.receipt.has_value()
                     ? receipt_json(*doc.receipt).dump()
                     : std::string();
  return j.dump();
}

inline std::optional<OutboxDocument> parse_outbox(const std::string& body) {
  const auto j = object(body, 5);
  if (!j) return std::nullopt;

  const auto version = number(*j, "version");
  if (!version || *version != 1) return std::nullopt;

  const auto origin = text(*j, "origin");
  const auto request_payload = text(*j, "request");
  const auto status_text = text(*j, "status");
  const auto receipt_payload = text(*j, "receipt");
  if (!origin || !request_payload || !status_text || !receipt_payload) {
    return std::nullopt;
  }

  // Decode the JSON payloads before any semantic checks.
  const auto request = parse_record(*request_payload);
  if (!request) return std::nullopt;

  OutboxStatus status;
  if (*status_text == "pending") {
    status = OutboxStatus::pending;
  } else if (*status_text == "stopped") {
    status = OutboxStatus::stopped;
  } else if (*status_text == "confirmed") {
    status = OutboxStatus::confirmed;
  } else {
    return std::nullopt;  // no defaulting of unknown values
  }

  std::optional<study_net::Receipt> receipt;
  if (!receipt_payload->empty()) {
    const auto parsed = parse_receipt(*receipt_payload);
    if (!parsed) return std::nullopt;
    receipt = *parsed;
  }

  OutboxDocument doc;
  doc.origin = *origin;
  doc.request = *request;
  doc.status = status;
  doc.receipt = receipt;

  if (!valid_outbox(doc)) return std::nullopt;
  return doc;
}

}  // namespace study_meta

#endif  // STUDY_META_OUTBOX_WIRE_H
