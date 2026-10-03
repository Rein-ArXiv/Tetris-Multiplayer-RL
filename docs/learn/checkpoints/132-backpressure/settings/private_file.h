#pragma once
#include <string>
namespace study_files {
// Complete temporary file, then same-directory replacement. POSIX also syncs
// file and directory. A directory-sync failure can occur AFTER publication.
// Caller supplies a path with a parent; multiple writers are not serialized.
bool write_private_file(const std::string& path, const std::string& contents);
}
