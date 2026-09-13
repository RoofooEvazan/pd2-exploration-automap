// Incremental geometry regions with disjoint ownership and quad compaction.
#pragma once
#include "StyledMap.hpp"

namespace styled_map {
// All cache mutation happens on the existing single worker thread.
class ChunkedMap {
    static constexpr int side=64,padding=14; // Fine cells; the largest filter reaches 13.
    using Key=std::pair<int,int>;
    Level floor_;
    Rows previousVisible_,previousFloor_,previousKnown_;
    std::size_t previousRevision_=0;
    std::map<Key,Drawing> chunks_;
    static void compact(std::vector<Quad>& quads) {
        // Remove artificial splits at cache borders before publishing, keeping
        // tile ownership private to the worker instead of adding GPU work.
        for(bool horizontal:{true,false}) {
            std::sort(quads.begin(),quads.end(),[&](const Quad& a,const Quad& b){
                return horizontal?std::tie(a.a.y,a.c.y,a.a.x)<std::tie(b.a.y,b.c.y,b.a.x):
                    std::tie(a.a.x,a.c.x,a.a.y)<std::tie(b.a.x,b.c.x,b.a.y);
            });
            std::size_t keep=0;
            for(auto q:quads) {
                if(keep) {
                    auto& last=quads[keep-1];
                    if(horizontal && last.a.y==q.a.y && last.c.y==q.c.y && last.c.x==q.a.x){last.b.x=last.c.x=q.c.x;continue;}
                    if(!horizontal && last.a.x==q.a.x && last.c.x==q.c.x && last.c.y==q.a.y){last.c.y=last.d.y=q.c.y;continue;}
                }
                quads[keep++]=q;
            }
            quads.resize(keep);
        }
    }
    static int divide(int n){return n/side-(n%side<0);}
    static void mark(std::set<Key>& dirty,Span s,int y,int scale) {
        int left=s.first*scale-padding,right=s.second*scale+padding;
        int top=y*scale-padding,bottom=(y+1)*scale+padding;
        for(int cy=divide(top);cy<=divide(bottom-1);++cy)
            for(int cx=divide(left);cx<=divide(right-1);++cx)dirty.insert({cx,cy});
    }
    static void changes(const Rows& old,const Rows& now,int scale,std::set<Key>& dirty) {
        auto a=old.begin(),b=now.begin();
        while(a!=old.end() || b!=now.end()) {
            if(b==now.end() || (a!=old.end() && a->first<b->first)) {
                for(auto span:a->second)mark(dirty,span,a->first,scale);++a;
            } else if(a==old.end() || b->first<a->first) {
                for(auto span:b->second)mark(dirty,span,b->first,scale);++b;
            } else {
                if(a->second!=b->second) {
                    for(auto span:subtract(a->second,b->second))mark(dirty,span,a->first,scale);
                    for(auto span:subtract(b->second,a->second))mark(dirty,span,b->first,scale);
                }
                ++a;++b;
            }
        }
    }
    struct Window {
        Rows spans;
        const Rows& rows() const {return spans;}
        bool contains(Point p) const {
            const auto& row=rowAt(spans,int(std::floor(p.y*4)));int x=int(std::floor(p.x*4));
            auto it=std::upper_bound(row.begin(),row.end(),x,[](int v,Span s){return v<s.first;});
            return it!=row.begin() && x<std::prev(it)->second;
        }
    };
    static Window window(const Rows& rows,exploration::Rect bounds) {
        Window result;
        for(auto it=rows.lower_bound(bounds.top-padding);it!=rows.end() && it->first<bounds.bottom+padding;++it) {
            Row clipped;
            for(auto span:it->second) {
                int left=std::max(span.first,bounds.left-padding),right=std::min(span.second,bounds.right+padding);
                if(left<right)clipped.push_back({left,right});
            }
            if(!clipped.empty())result.spans.emplace(it->first,std::move(clipped));
        }
        return result;
    }
    void replace(const std::set<Key>& keys,const Drawing& source) {
        for(auto key:keys)chunks_.erase(key);
        // Filter a neighboring group once, then distribute its disjoint quads.
        // Re-running the same 14-cell filter halo for each tile is unnecessary.
        for(std::size_t layer=0;layer<source.layers.size();++layer)for(int red:{0,1})
            for(auto q:red?source.layers[layer].redQuads:source.layers[layer].quads) {
                int l=int(lround(q.a.x*4)),r=int(lround(q.c.x*4)),t=int(lround(q.a.y*4)),b=int(lround(q.c.y*4));
                for(int cy=divide(t);cy<=divide(b-1);++cy)for(int cx=divide(l);cx<=divide(r-1);++cx)if(keys.count({cx,cy})) {
                    auto& drawing=chunks_[{cx,cy}];auto& target=drawing.layers[layer];
                    addRect(red?target.redQuads:target.quads,std::max(l,cx*side),std::min(r,(cx+1)*side),std::max(t,cy*side),std::min(b,(cy+1)*side));
                    ++drawing.quads;
                }
            }
        for(auto wall:source.walls) {
            int ax=int(lround(wall.a.x*4)),ay=int(lround(wall.a.y*4)),bx=int(lround(wall.b.x*4)),by=int(lround(wall.b.y*4));
            if(ax==bx) {
                int cx=divide(ax);
                for(int cy=divide(std::min(ay,by));cy<=divide(std::max(ay,by)-1);++cy)if(keys.count({cx,cy})) {
                    auto part=wall;part.a.y=std::clamp(ay,cy*side,(cy+1)*side)*.25;part.b.y=std::clamp(by,cy*side,(cy+1)*side)*.25;
                    chunks_[{cx,cy}].walls.push_back(part);
                }
            } else {
                int cy=divide(ay);
                for(int cx=divide(std::min(ax,bx));cx<=divide(std::max(ax,bx)-1);++cx)if(keys.count({cx,cy})) {
                    auto part=wall;part.a.x=std::clamp(ax,cx*side,(cx+1)*side)*.25;part.b.x=std::clamp(bx,cx*side,(cx+1)*side)*.25;
                    chunks_[{cx,cy}].walls.push_back(part);
                }
            }
        }
    }
public:
    std::size_t rebuiltChunks=0;
    Level& floor(){return floor_;}
    std::size_t chunkCount() const {return chunks_.size();}
    template<class VisibleMask> Drawing build(const VisibleMask& visible) {
        std::set<Key> dirty;
        changes(previousVisible_,visible.rows(),1,dirty);
        if(previousRevision_!=floor_.revision) {
            changes(previousFloor_,floor_.floorRows(),4,dirty);
            changes(previousKnown_,floor_.knownRows(),4,dirty);
        }
        rebuiltChunks=0;
        while(!dirty.empty()) {
            std::set<Key> group;std::vector<Key> queue{*dirty.begin()};dirty.erase(dirty.begin());
            int minX=queue[0].first,maxX=minX,minY=queue[0].second,maxY=minY;
            for(std::size_t i=0;i<queue.size();++i) {
                const auto key=queue[i];group.insert(key);
                minX=std::min(minX,key.first);maxX=std::max(maxX,key.first);minY=std::min(minY,key.second);maxY=std::max(maxY,key.second);
                for(auto neighbor:std::array<Key,4>{{{key.first-1,key.second},{key.first+1,key.second},{key.first,key.second-1},{key.first,key.second+1}}}) {
                    auto found=dirty.find(neighbor);if(found!=dirty.end()){queue.push_back(*found);dirty.erase(found);}
                }
            }
            exploration::Rect bounds{minX*side,minY*side,(maxX+1)*side,(maxY+1)*side};
            auto view=window(visible.rows(),bounds);
            if(view.spans.empty()){for(auto key:group)chunks_.erase(key);continue;}
            auto drawing=floor_.build(view,&bounds);replace(group,drawing);rebuiltChunks+=group.size();
        }
        previousVisible_=visible.rows();
        if(previousRevision_!=floor_.revision) {
            previousFloor_=floor_.floorRows();previousKnown_=floor_.knownRows();previousRevision_=floor_.revision;
        }
        Drawing result;std::size_t wallCount=0;
        for(const auto& chunk:chunks_)wallCount+=chunk.second.walls.size();
        result.walls.reserve(wallCount);
        for(std::size_t i=0;i<result.layers.size();++i) {
            std::size_t gray=0,red=0;
            for(const auto& chunk:chunks_){gray+=chunk.second.layers[i].quads.size();red+=chunk.second.layers[i].redQuads.size();}
            auto& dest=result.layers[i];dest.quads.reserve(gray);dest.redQuads.reserve(red);
            for(const auto& chunk:chunks_) {
                const auto& src=chunk.second.layers[i];
                dest.quads.insert(dest.quads.end(),src.quads.begin(),src.quads.end());
                dest.redQuads.insert(dest.redQuads.end(),src.redQuads.begin(),src.redQuads.end());
            }
            compact(dest.quads);compact(dest.redQuads);result.quads+=dest.quads.size()+dest.redQuads.size();
        }
        for(const auto& chunk:chunks_)result.walls.insert(result.walls.end(),chunk.second.walls.begin(),chunk.second.walls.end());
        compactWalls(result.walls);
        return result;
    }
};
}
