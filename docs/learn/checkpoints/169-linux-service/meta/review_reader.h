#pragma once
#include "meta/sqlite_handle.h"
#include "meta/review_sample.h"
#include <charconv>
namespace study_meta {
// An operator-local read-only connection. No account credential columns are read.
class ReviewReader {
public:
    explicit ReviewReader(const std::string& path) {
        if(path.empty() || path.find('\0')!=std::string::npos)throw std::invalid_argument("review database path");
        sqlite3* raw=nullptr;
        const auto rc=sqlite3_open_v2(path.c_str(),&raw,SQLITE_OPEN_READONLY,nullptr);
        db_.reset(raw);
        if(rc!=SQLITE_OK || !raw)throw std::runtime_error("review database open failed");
        if(sqlite3_busy_timeout(raw,1000)!=SQLITE_OK || !read_only())throw std::runtime_error("review read-only setup failed");
        Statement app(raw,"PRAGMA application_id"),version(raw,"PRAGMA user_version");
        if(!app.next() || app.integer_column(0)!=1413829714 || !version.next() || version.integer_column(0)!=7)
            throw std::runtime_error("unsupported review schema");
    }
    bool read_only()const noexcept{return sqlite3_db_readonly(db_.get(),"main")==1;}
    ReviewSample read(std::size_t limit) {
        if(!limit || limit>kMaxReviewRows)throw std::invalid_argument("review row limit");
        Statement q(db_.get(),"SELECT id,player_a,player_b,winner,ticks FROM matches ORDER BY id DESC LIMIT ?1");
        q.integer(1,static_cast<sqlite3_int64>(limit+1));
        ReviewSample sample{{},limit,false};sample.rows.reserve(limit);
        while(q.next()) {
            if(sample.rows.size()==limit){sample.has_older=true;continue;}
            const auto id=q.integer_column(0);
            if(id<=0)throw std::runtime_error("invalid result identity");
            sample.rows.push_back({static_cast<std::uint64_t>(id),decimal(q.text_column(1)),
                decimal(q.text_column(2)),q.is_null(3)?0:decimal(q.text_column(3)),decimal(q.text_column(4))});
        }
        return sample; // One SELECT reads one coherent SQLite snapshot.
    }
private:
    static std::uint64_t decimal(const std::string& s) {
        std::uint64_t value=0;const auto result=std::from_chars(s.data(),s.data()+s.size(),value);
        if(result.ec!=std::errc{} || result.ptr!=s.data()+s.size() || !value || std::to_string(value)!=s)
            throw std::runtime_error("invalid canonical review ID or tick count");
        return value;
    }
    struct Close {void operator()(sqlite3* db)const noexcept{if(db)sqlite3_close_v2(db);}};
    std::unique_ptr<sqlite3,Close> db_;
};
} // namespace study_meta
