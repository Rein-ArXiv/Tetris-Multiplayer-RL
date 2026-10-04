#pragma once
#include "meta/bootstrap_types.h"
namespace study_meta {
struct AccountOutcome {
    std::optional<BootstrapResult> result;
    bool has_unsaved_key = false;
};
} // namespace study_meta
