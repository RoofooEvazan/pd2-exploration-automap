// Sparse exploration masks, frontier extraction and reference primitive clipping.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>
#include "ProjectedMask.hpp"

namespace exploration {
struct Point { double x, y; };
struct Rect { int left, top, right, bottom; }; // Half-open raster coordinates.
inline Rect intersect(Rect a, Rect b) {
    return {std::max(a.left,b.left),std::max(a.top,b.top),
            std::min(a.right,b.right),std::min(a.bottom,b.bottom)};
}
class Mask {
    double cellSize_;
    using Span=std::pair<int,int>; // Half-open cell interval.
    using Row=std::vector<Span>;
    std::map<int,Row> rows_;
    std::size_t size_=0;
    mutable bool dirty_=true;
    mutable std::vector<std::pair<Point,Point>> edges_;
    ProjectedMask projected_[2];
    void revealSpan(int y,int left,int right) {
        auto& row=rows_[y];
        auto first=std::lower_bound(row.begin(),row.end(),left,
            [](Span s,int x){return s.second<x;});
        auto last=first;
        int l=left,r=right;
        std::size_t removed=0;
        while(last!=row.end() && last->first<=r) {
            l=std::min(l,last->first);r=std::max(r,last->second);
            removed+=last->second-last->first;++last;
        }
        if(first!=last && std::next(first)==last && first->first==l && first->second==r)return;
        if(cellSize_==0.25) {
            projected_[0].revealQuarterSpan(left,right,y,10);
            projected_[1].revealQuarterSpan(left,right,y,20);
        }
        size_+=static_cast<std::size_t>(r-l)-removed;
        first=row.erase(first,last);row.insert(first,{l,r});dirty_=true;
    }
    template<class Emit> void exposed(Span s,int neighbor,Emit emit) const {
        auto it=rows_.find(neighbor);
        int x=s.first;
        if(it!=rows_.end())for(auto cover:it->second) {
            if(cover.second<=x)continue;
            if(cover.first>=s.second)break;
            if(cover.first>x)emit(x,std::min(cover.first,s.second));
            x=std::max(x,cover.second);if(x>=s.second)return;
        }
        if(x<s.second)emit(x,s.second);
    }
    void rebuildEdges() const {
        edges_.clear();
        // Join straight edges so fine cells do not multiply line submissions.
        std::map<int,Row> vertical;
        for(const auto& entry:rows_) {
            int y=entry.first;
            for(auto s:entry.second) {
                exposed(s,y-1,[&](int l,int r){edges_.push_back({{l*cellSize_,y*cellSize_},{r*cellSize_,y*cellSize_}});});
                exposed(s,y+1,[&](int l,int r){edges_.push_back({{r*cellSize_,(y+1)*cellSize_},{l*cellSize_,(y+1)*cellSize_}});});
                for(int x:{s.first,s.second}) {
                    auto& runs=vertical[x];
                    if(!runs.empty() && runs.back().second==y)runs.back().second=y+1;
                    else runs.push_back({y,y+1});
                }
            }
        }
        for(const auto& entry:vertical)for(auto s:entry.second)
            edges_.push_back({{entry.first*cellSize_,s.first*cellSize_},{entry.first*cellSize_,s.second*cellSize_}});
        dirty_=false;
    }
public:
    explicit Mask(double cellSize=5) : cellSize_(cellSize) {
        if (!std::isfinite(cellSize) || cellSize<=0) throw std::invalid_argument("cellSize");
    }
    double cellSize() const { return cellSize_; }
    const auto& rows() const {return rows_;}
    void revealRange(int y,int left,int right) {if(left<right)revealSpan(y,left,right);}
    bool cell(int x,int y) const {
        auto row=rows_.find(y);if(row==rows_.end())return false;
        auto span=std::upper_bound(row->second.begin(),row->second.end(),x,
            [](int v,Span s){return v<s.first;});
        return span!=row->second.begin() && x<std::prev(span)->second;
    }
    void reveal(int x,int y) { revealSpan(y,x,x+1); }
    std::size_t size() const { return size_; }
    template<class Emit> void clipProjected(Rect bounds,int divisor,int shiftX,int shiftY,Emit emit) const {
        if(cellSize_!=0.25 || (divisor!=10 && divisor!=20))throw std::invalid_argument("projection");
        projected_[divisor==10?0:1].clip(bounds.left+shiftX,bounds.top+shiftY,bounds.right+shiftX,bounds.bottom+shiftY,
            [&](int l,int t,int r,int b){emit(Rect{l-shiftX,t-shiftY,r-shiftX,b-shiftY});});
    }
    bool contains(Point p) const {
        return cell(static_cast<int>(std::floor(p.x/cellSize_)),
                    static_cast<int>(std::floor(p.y/cellSize_)));
    }
    // Fallback: reveal a disk in cell units at each sampled player cell.
    // Teleports reveal the destination only; never interpolate unknown travel.
    void revealAround(Point p,int radius) {
        if(radius<0 || radius>1024) throw std::invalid_argument("radius");
        int cx=static_cast<int>(std::floor(p.x/cellSize_));
        int cy=static_cast<int>(std::floor(p.y/cellSize_));
        for(int y=-radius;y<=radius;++y) {
            int dx=static_cast<int>(std::sqrt(double(radius*radius-y*y)));
            revealSpan(cy+y,cx-dx,cx+dx+1);
        }
    }
    template<class Emit> void frontier(Emit emit) const {
        if(dirty_)rebuildEdges();
        for(const auto& edge:edges_)emit(edge.first,edge.second);
    }
};
// Caller supplies a unique game serial and a unique level-instance serial.
// Recycled pointers or level numbers alone are insufficient instance keys.
class Session {
    std::uint64_t game_=0;
    std::map<std::uint64_t,Mask> levels_;
    double cellSize_;
public:
    explicit Session(double cellSize=5.0):cellSize_(cellSize){}
    void leave() { levels_.clear(); game_=0; }
    Mask& select(std::uint64_t game,std::uint64_t levelInstance) {
        if(game_!=game) { levels_.clear(); game_=game; }
        return levels_.try_emplace(levelInstance,cellSize_).first->second;
    }
};
// Emit disjoint scanline clips for the original primitive.
// inverse maps raster pixel centers to stable world coordinates.
template<class Inverse,class Emit>
void clipPrimitive(const Mask& mask,Rect bounds,Rect viewport,Inverse inverse,Emit emit) {
    Rect r=intersect(bounds,viewport);
    for(int y=r.top;y<r.bottom;++y) {
        int start=r.left;
        bool active=false;
        for(int x=r.left;x<r.right;++x) {
            bool visible=mask.contains(inverse(Point{x+0.5,y+0.5}));
            if(visible && !active) { start=x; active=true; }
            if(!visible && active) { emit(Rect{start,y,x,y+1}); active=false; }
        }
        if(active) emit(Rect{start,y,r.right,y+1});
    }
}
} // namespace exploration
