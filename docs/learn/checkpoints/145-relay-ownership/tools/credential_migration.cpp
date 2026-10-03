#include "meta/credential_migration.h"
#include <filesystem>
#include <iostream>
#include <cstdlib>
using namespace study_migration;
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::invalid_argument("usage: credential_migration MODE DB");
        const std::string mode = argv[1], path = argv[2];
        if (mode != "init" && mode != "migrate" && mode != "crash-row" && mode != "crash-commit" && mode != "crash-vacuum")
            throw std::invalid_argument("unknown experiment mode");
        if (mode == "init") {
            if (std::filesystem::exists(path)) throw std::runtime_error("fixture path already exists");
            SqliteDb db(path);
            db.exec("CREATE TABLE players(id INTEGER PRIMARY KEY,token TEXT UNIQUE NOT NULL,bp INTEGER NOT NULL)");
            Statement insert(db.get(), "INSERT INTO players VALUES(7,?1,80)");
            // Public fixture only. No issued account credential is used.
            insert.text(1, std::string(study_credentials::account_bytes * study_credentials::hex_per_byte, 'a'));
            insert.done();
            std::cout << "public legacy fixture created\n";
        } else {
            if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("existing experiment DB required");
            SqliteDb db(path);
            migrate(db, [&](Point point) {
                if ((mode == "crash-row" && point == Point::after_row) ||
                    (mode == "crash-commit" && point == Point::after_commit) ||
                    (mode == "crash-vacuum" && point == Point::after_vacuum))
                    std::_Exit(73); // Deliberately bypass destructors, in this lab process only.
            });
            std::cout << "hash migration and local scrub completed\n";
        }
        return 0;
    } catch (const std::exception&) {
        std::cerr << "migration refused or incomplete; inspect experiment state before retry\n";
        return 1;
    }
}
