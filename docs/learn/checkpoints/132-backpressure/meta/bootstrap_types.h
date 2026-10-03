#pragma once
#include "meta/profile_wire.h"
namespace study_meta {
enum class BootstrapState {
    online, offline_saved, unsaved, storage_blocked, key_rejected, creation_unconfirmed
};
struct BootstrapResult {
    BootstrapState state = BootstrapState::storage_blocked;
    std::optional<PublicProfile> profile; // only successful verification populates this
};
} // namespace study_meta
