// Bounded, owned clipping results in automap coordinates before screen pan.
// Cache misses use the exact same raster query; no drawing is skipped.
#pragma once
#include "ExplorationMask.hpp"
#include <array>

namespace exploration {
class RasterClipCache {
public:
    using Key=std::array<int,5>; // left, top, right, bottom, coverage mode
    static constexpr std::size_t slotCount=8192,rectLimit=16;
private:
    struct Entry {Key key{};std::uint64_t epoch=0;std::vector<Rect> rects;};
    std::array<Entry,slotCount> entries_{};
    std::vector<Rect> scratch_;
    std::uint64_t epoch_=1;
    std::size_t hits_=0,misses_=0;
public:
    void invalidate() {
        if(++epoch_==0){for(auto& entry:entries_)entry.epoch=0;epoch_=1;}
    }
    template<class Build,class Emit> void query(Key key,Build build,Emit emit) {
        std::uint32_t hash=2166136261u;
        for(auto value:key){hash^=std::uint32_t(value);hash*=16777619u;hash^=hash>>16;}
        auto& entry=entries_[hash&(slotCount-1)];
        if(entry.epoch==epoch_ && entry.key==key){++hits_;for(auto r:entry.rects)emit(r);return;}
        ++misses_;scratch_.clear();
        build([&](Rect r){
            if(!scratch_.empty() && scratch_.back().left==r.left && scratch_.back().right==r.right && scratch_.back().bottom==r.top)
                scratch_.back().bottom=r.bottom;
            else scratch_.push_back(r);
        });
        if(scratch_.size()<=rectLimit){
            if(!scratch_.empty() && entry.rects.capacity()<rectLimit)entry.rects.reserve(rectLimit);
            entry.rects.assign(scratch_.begin(),scratch_.end());entry.key=key;entry.epoch=epoch_;
        } else entry.epoch=0; // Complex cells still draw, without a cached entry.
        for(auto r:scratch_)emit(r);
    }
    std::size_t hits() const{return hits_;}
    std::size_t misses() const{return misses_;}
};
}
