#pragma once
#include "client/async_job.h"
#include "meta/ranking_wire.h"
namespace study_ranking_ui {
enum class Status {idle,busy,ready,failed};
struct View {Status status=Status::idle;study_meta::Ranking rows;};
// A successful empty list is ready; it is not a failed request.
template<class Worker>
class Controller final {
public:
    explicit Controller(std::unique_ptr<Worker> worker):job_(std::move(worker)){}
    const View& view()const {return view_;}
    bool busy()const {return job_.valid();}
    bool request() {
        if(busy())return false;
        try {
            if(!job_.start())return false;
            view_={Status::busy,{}};return true;
        }catch(const std::exception&) {view_={Status::failed,{}};return false;}
    }
    bool poll() {
        if(!job_.ready())return false;
        try {
            auto result=job_.take();
            view_=result && study_meta::valid_ranking(*result)
                ? View{Status::ready,std::move(*result)} : View{Status::failed,{}};
        }catch(const std::exception&) {view_={Status::failed,{}};}
        return true;
    }
private:
    View view_;
    study_jobs::Job<Worker> job_;
};
} // namespace study_ranking_ui
