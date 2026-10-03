#include "meta/migrations.h"
#include <iostream>
#include <string>
int main(int argc,char** argv) {
    try {
        if(argc<2 || argc>3 || (argc==3 && std::string(argv[2])!="--pause-after-column"))return 2;
        study_meta::SqliteDb db(argv[1]);
        db.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
        study_meta::migrate_study(db,[&](study_meta::MigrationPoint point) {
            if(argc==3 && point==study_meta::MigrationPoint::after_column) {
                std::cout<<"COLUMN_PENDING\n"<<std::flush;
                std::string line;
                if(!std::getline(std::cin,line))throw std::runtime_error("experiment input closed");
            }
        });
        std::cout<<"schema_version="<<study_meta::schema_integer(db,"PRAGMA user_version")<<'\n';
    }catch(const std::exception& e){std::cerr<<"migration: "<<e.what()<<'\n';return 1;}
}
