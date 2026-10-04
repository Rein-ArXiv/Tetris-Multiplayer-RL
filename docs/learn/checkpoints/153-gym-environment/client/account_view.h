#pragma once

#include <optional>

#include "meta/bootstrap_types.h"

namespace study_account_ui {

// Presentation-only status for the account/key UI.
enum class Status {
  idle,
  busy,
  online,
  offline_saved,
  unsaved,
  storage_blocked,
  key_rejected,
  creation_unconfirmed,
  failed,
};

// Value snapshot exposed as const by the controller; never carries credentials.
struct View {
  Status status = Status::idle;
  bool has_unsaved_key = false;
  std::optional<study_meta::PublicProfile> profile;
};

// Maps a bootstrap result to a view. An unsaved key always takes precedence.
inline View present(const study_meta::BootstrapResult& result, bool unsaved) {
  View view;
  if (unsaved) {
    view.status = Status::unsaved;
    view.has_unsaved_key = true;
    view.profile.reset();
    return view;
  }

  switch (result.state) {
    case study_meta::BootstrapState::online:
      // Online requires a confirmed public profile.
      if (!result.profile.has_value()) {
        view.status = Status::failed;
        return view;
      }
      view.status = Status::online;
      view.profile = result.profile;
      return view;
    case study_meta::BootstrapState::offline_saved:
      view.status = Status::offline_saved;
      return view;
    case study_meta::BootstrapState::unsaved:
      view.status = Status::unsaved;
      view.has_unsaved_key = true;
      return view;
    case study_meta::BootstrapState::storage_blocked:
      view.status = Status::storage_blocked;
      return view;
    case study_meta::BootstrapState::key_rejected:
      view.status = Status::key_rejected;
      return view;
    case study_meta::BootstrapState::creation_unconfirmed:
      view.status = Status::creation_unconfirmed;
      return view;
    default:
      view.status = Status::failed;
      return view;
  }
}

// One explicit connect action is disabled while busy or issuance is ambiguous.
inline bool may_connect(const View& view) {
  return view.status != Status::busy &&
         view.status != Status::creation_unconfirmed;
}

}  // namespace study_account_ui
