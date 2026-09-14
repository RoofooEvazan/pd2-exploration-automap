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
    static constexpr std::size_t ways=4,setCount=slotCount/ways;
    struct Entry {Key key{};std::uint64_t epoch=0,coverage=0,touched=0;bool full=false;std::vector<Rect> rects;};
    std::array<Entry,slotCount> entries_{};
    std::vector<Rect> scratch_;
    std::uint64_t epoch_=1,coverage_=1;
    std::uint64_t clock_=0;
    std::size_t hits_=0,misses_=0;
public:
    void invalidate() {
        if(++epoch_==0){for(auto& entry:entries_)entry.epoch=0;epoch_=1;}
        coverage_=1;clock_=0;
    }
    // Only valid for monotonic exploration in the same coordinate context.
    // Fully visible artwork stays fully visible; partial/empty clips must refresh.
    void grow() {
        if(++coverage_==0)invalidate();
    }
    template<class Build,class Emit> void query(Key key,Build build,Emit emit) {
        std::uint32_t hash=2166136261u;
        for(auto value:key){hash^=std::uint32_t(value);hash*=16777619u;hash^=hash>>16;}
        const auto first=(hash&(setCount-1))*ways;
        Entry* victim=&entries_[first];
        // Nearby repeated tiles often hash to the same slot. Four candidates
        // avoid rebuilding both clips every frame without raising the slot budget.
        for(std::size_t i=first;i<first+ways;++i) {
            auto& candidate=entries_[i];
            if(candidate.epoch==epoch_ && candidate.key==key) {
                if(candidate.full || candidate.coverage==coverage_) {
                    ++hits_;candidate.touched=++clock_;for(auto r:candidate.rects)emit(r);return;
                }
                victim=&candidate;break;
            }
            if(candidate.epoch!=epoch_)victim=&candidate;
            else if(victim->epoch==epoch_ && candidate.touched<victim->touched)victim=&candidate;
        }
        auto& entry=*victim;
        ++misses_;scratch_.clear();
        build([&](Rect r){
            if(!scratch_.empty() && scratch_.back().left==r.left && scratch_.back().right==r.right && scratch_.back().bottom==r.top)
                scratch_.back().bottom=r.bottom;
            else scratch_.push_back(r);
        });
        if(scratch_.size()<=rectLimit){
            if(!scratch_.empty() && entry.rects.capacity()<rectLimit)entry.rects.reserve(rectLimit);
            entry.rects.assign(scratch_.begin(),scratch_.end());entry.key=key;entry.epoch=epoch_;entry.coverage=coverage_;entry.touched=++clock_;
            entry.full=scratch_.size()==1 && scratch_[0].left==key[0] && scratch_[0].top==key[1] &&
                scratch_[0].right==key[2] && scratch_[0].bottom==key[3];
        } else entry.epoch=0; // Complex cells still draw, without a cached entry.
        for(auto r:scratch_)emit(r);
    }
    std::size_t hits() const{return hits_;}
    std::size_t misses() const{return misses_;}
};
}
