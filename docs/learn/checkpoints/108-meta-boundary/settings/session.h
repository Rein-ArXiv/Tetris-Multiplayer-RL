#pragma once
#include "settings/store.h"
#include "client/menu_model.h"
#include <utility>
namespace study_settings {
enum class Change { unchanged, saved, blocked, failed };
// One process owns this path. Initial load decides whether saves are allowed;
// repairing a blocked file requires restarting this session. No background retries.
class Session {
public:
    explicit Session(std::filesystem::path path) : path_(std::move(path)), loaded_(load(path_)) {
        preferences_.set_decorations(loaded_.config.decorations);
        (void)preferences_.select_character(loaded_.config.character);
    }
    const study_menu::Preferences& preferences() const noexcept { return preferences_; }
    const Loaded& initial_load() const noexcept { return loaded_; }
    Change apply(study_menu::Action action) {
        if (!preferences_.apply(action)) return Change::unchanged;
        if (!loaded_.writable()) return Change::blocked;
        const Config snapshot{preferences_.decorations(), std::string(preferences_.character_id())};
        return save(path_, snapshot) ? Change::saved : Change::failed;
    }
private:
    std::filesystem::path path_;
    Loaded loaded_;
    study_menu::Preferences preferences_;
};
} // namespace study_settings
