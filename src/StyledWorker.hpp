// Background geometry construction from owned session/area snapshots.
#pragma once
#include "StyledMap.hpp"
#include "StyledChunks.hpp"
#include "PreparedFloors.hpp"
#include "TerrainCoverage.hpp"
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace styled_map {
// A snapshot has no projected sprite rasters, cached edges or game pointers.
struct VisibleSnapshot {
    Rows spans;
    const Rows& rows() const {return spans;}
    bool contains(Point p) const {
        const auto& row=rowAt(spans,int(floor(p.y*4)));
        int x=int(floor(p.x*4));
        auto it=std::upper_bound(row.begin(),row.end(),x,[](int v,Span s){return v<s.first;});
        return it!=row.begin() && x<std::prev(it)->second;
    }
};
struct CapturedRoom {int x,y,w,h;std::vector<std::uint16_t> flags;};
// Owned immutable grids are retained so coalescing a pending update or changing
// areas cannot drop the only copy of a room. The worker never reads game memory.
class FloorCopies {
    std::set<std::tuple<int,int,int,int>> seen_;
    std::size_t cells_=0;
    bool full_=false;
public:
    std::vector<std::shared_ptr<const CapturedRoom>> rooms;
    bool contains(int x,int y,int w,int h) const {return seen_.count({x,y,w,h})!=0;}
    bool wanted(int x,int y,int w,int h) const {return !full_ && rooms.size()<2048 && !seen_.count({x,y,w,h});}
    void ingest(int x,int y,int w,int h,const std::vector<std::uint16_t>& flags) {
        if(w<1 || h<1 || w>512 || h>512 || flags.size()!=std::size_t(w)*h || !wanted(x,y,w,h))return;
        if(cells_+flags.size()>2000000){full_=true;return;}
        auto room=std::make_shared<CapturedRoom>(CapturedRoom{x,y,w,h,flags});
        rooms.push_back(std::move(room));seen_.insert({x,y,w,h});cells_+=flags.size();
    }
};
struct BuildRequest {
    int boundaryWidth=12;
    bool boundaryThroughUnknown=false;
    bool excludeTown=false;
    exploration::Rect townBounds{};
    std::chrono::steady_clock::time_point queuedAt=std::chrono::steady_clock::now();
    std::uint64_t session=0,level=0;
    std::size_t maskSize=0;
    Point player{};
    VisibleSnapshot visible;
    std::vector<std::shared_ptr<const CapturedRoom>> rooms;
};
struct BuildResult {
    bool boundaryThroughUnknown=false;
    bool excludesTown=false;
    std::uint64_t session=0,level=0;
    std::size_t maskSize=0,floorCells=0,roomCount=0;
    std::size_t rebuiltChunks=0,totalChunks=0;
    double milliseconds=0,latencyMilliseconds=0;
    bool success=false;
    Drawing drawing;
    std::shared_ptr<const TerrainCoverage> coverage;
    std::unique_ptr<PreparedFloors> preparedFloors;
};
// Towns can share an automap layer with outdoor areas. Remove only colored
// boundary quads inside their world rectangle; the underlying cache stays shared.
inline void excludeTownBoundary(Drawing& drawing,exploration::Rect town) {
    drawing.quads=0;
    for(auto& layer:drawing.layers) {
        std::vector<Quad> kept;
        for(const auto& q:layer.redQuads) {
            const double l=std::max(q.a.x,double(town.left)),r=std::min(q.c.x,double(town.right));
            const double t=std::max(q.a.y,double(town.top)),b=std::min(q.c.y,double(town.bottom));
            if(l>=r || t>=b){kept.push_back(q);continue;}
            auto add=[&](double left,double top,double right,double bottom){
                if(left<right && top<bottom)kept.push_back({{left,top},{right,top},{right,bottom},{left,bottom}});
            };
            add(q.a.x,q.a.y,q.c.x,t);add(q.a.x,b,q.c.x,q.c.y);
            add(q.a.x,t,l,b);add(r,t,q.c.x,b);
        }
        layer.redQuads=std::move(kept);drawing.quads+=layer.quads.size()+layer.redQuads.size();
    }
}
// One worker, one replaceable pending snapshot, one completed result. A slow
// rebuild cannot create an unbounded queue or block the game's drawing thread.
class Worker {
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_=false;
    std::unique_ptr<BuildRequest> pending_;
    std::unique_ptr<BuildResult> ready_;
    std::thread thread_;
    void run() {
        std::uint64_t session=0;
        struct CachedLevel {
            ChunkedMap map;
            std::size_t coverageRooms=0;
            std::shared_ptr<const TerrainCoverage> coverage;
        };
        std::map<std::uint64_t,CachedLevel> levels;
        for(;;) {
            std::unique_ptr<BuildRequest> request;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait(lock,[&]{return stop_ || bool(pending_);});
                if(stop_)return;
                request=std::move(pending_);
            }
            std::unique_ptr<BuildResult> result;
            try {
                result=std::make_unique<BuildResult>();
                result->session=request->session;result->level=request->level;result->maskSize=request->maskSize;
                result->roomCount=request->rooms.size();
                result->boundaryThroughUnknown=request->boundaryThroughUnknown;
                result->excludesTown=request->excludeTown;
                auto start=std::chrono::steady_clock::now();
                if(session!=request->session){levels.clear();session=request->session;}
                // These caches can be reconstructed from retained room copies.
                if(levels.size()>=32 && !levels.count(request->level))levels.erase(levels.begin());
                auto& cached=levels[request->level];auto& map=cached.map;auto& floor=map.floor();
                for(const auto& room:request->rooms)
                    floor.ingest(room->x,room->y,room->w,room->h,room->flags);
                if(floor.connect(request->player) || request->boundaryThroughUnknown) {
                    result->drawing=map.build(request->visible,request->boundaryWidth,request->boundaryThroughUnknown);
                    if(!cached.coverage || cached.coverageRooms!=floor.roomCount()) {
                        cached.coverage=std::make_shared<TerrainCoverage>(floor.knownRows());
                        cached.coverageRooms=floor.roomCount();
                    }
                    result->coverage=cached.coverage;
                    if(request->excludeTown)excludeTownBoundary(result->drawing,request->townBounds);
                    result->preparedFloors=PreparedFloors::build(result->drawing);
                    result->floorCells=floor.size();result->success=true;
                    result->rebuiltChunks=map.rebuiltChunks;result->totalChunks=map.chunkCount();
                }
                result->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                result->latencyMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-request->queuedAt).count();
            } catch(...) {if(result)result->success=false;}
            // Destroy replaced results outside the short publication lock.
            {std::lock_guard<std::mutex> lock(mutex_);ready_.swap(result);}
        }
    }
public:
    Worker():thread_([this]{run();}){}
    ~Worker(){
        {std::lock_guard<std::mutex> lock(mutex_);stop_=true;}
        wake_.notify_one();if(thread_.joinable())thread_.join();
    }
    void submit(std::unique_ptr<BuildRequest> request) {
        {std::lock_guard<std::mutex> lock(mutex_);pending_.swap(request);}
        wake_.notify_one();
    }
    std::unique_ptr<BuildResult> take() {
        std::lock_guard<std::mutex> lock(mutex_);return std::move(ready_);
    }
};
}
