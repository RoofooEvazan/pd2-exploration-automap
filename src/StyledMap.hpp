// Builds connected floor geometry from room flags and an exploration mask.
// Produces shade bands, gray wall outlines, and red open exploration edges.
// Its full rebuild also serves as the reference for incremental-cache tests.

#pragma once
#include "ExplorationMask.hpp"
#include <array>
#include <tuple>
#include <unordered_set>

namespace styled_map {
using exploration::Point;
using Span=std::pair<int,int>;
using Row=std::vector<Span>;
using Rows=std::map<int,Row>;
struct Quad {Point a,b,c,d;};
struct Stroke {Point a,b;};
struct Layer {unsigned gray,alpha;std::vector<Quad> quads;std::vector<Quad> redQuads{};};
struct Drawing {
    // Low outer rim, bright inner rim, then a gradual fade into the floor.
    std::array<Layer,7> layers{{{52,70,{}},{116,175,{}},{96,150,{}},{65,115,{}},{38,80,{}},{23,55,{}},{14,40,{}}}};
    std::vector<Stroke> walls;
    std::size_t quads=0;
};
inline Row overlap(const Row& a,const Row& b) {
    Row out;std::size_t i=0,j=0;
    while(i<a.size() && j<b.size()) {
        int l=std::max(a[i].first,b[j].first),r=std::min(a[i].second,b[j].second);
        if(l<r)out.push_back({l,r});
        if(a[i].second<b[j].second)++i;else ++j;
    }
    return out;
}
inline Row subtract(const Row& a,const Row& b) {
    Row out;std::size_t j=0;
    for(auto span:a) {
        int x=span.first;while(j<b.size() && b[j].second<=x)++j;
        auto k=j;
        while(k<b.size() && b[k].first<span.second) {
            if(b[k].first>x)out.push_back({x,std::min(span.second,b[k].first)});
            x=std::max(x,b[k].second);if(x>=span.second)break;++k;
        }
        if(x<span.second)out.push_back({x,span.second});
    }
    return out;
}
inline const Row& rowAt(const Rows& rows,int y) {
    static const Row empty;auto it=rows.find(y);return it==rows.end()?empty:it->second;
}
// Exact erosion by an integer disk on the quarter-subtile exploration grid.
// Only row intervals are processed; cost does not scale with filled area.
inline Rows erode(const Rows& source,int radius) {
    if(radius==0)return source;
    Rows result;Row keep,scratch;
    std::vector<int> widths;
    for(int dy=-radius;dy<=radius;++dy)widths.push_back(int(floor(sqrt(double(radius*radius-dy*dy)))));
    for(const auto& entry:source) {
        keep.assign(entry.second.begin(),entry.second.end());
        for(int dy=-radius;dy<=radius && !keep.empty();++dy) {
            const int dx=widths[dy+radius];const auto& neighbor=rowAt(source,entry.first+dy);
            scratch.clear();std::size_t i=0,j=0;
            // Intersect directly with narrowed neighbor runs. Reuse both
            // buffers across all rows instead of allocating for every tap.
            while(i<keep.size() && j<neighbor.size()) {
                int nl=neighbor[j].first+dx,nr=neighbor[j].second-dx;
                if(nl>=nr){++j;continue;}
                int l=std::max(keep[i].first,nl),r=std::min(keep[i].second,nr);
                if(l<r)scratch.push_back({l,r});
                if(keep[i].second<nr)++i;else ++j;
            }
            keep.swap(scratch);
        }
        if(!keep.empty())result.emplace(entry.first,keep);
    }
    return result;
}
inline void addRect(std::vector<Quad>& quads,int l,int r,int top,int bottom) {
    if(l>=r || top>=bottom)return;
    quads.push_back({{l*.25,top*.25},{r*.25,top*.25},{r*.25,bottom*.25},{l*.25,bottom*.25}});
}
inline void normalize(Row& spans) {
    std::sort(spans.begin(),spans.end());std::size_t count=0;
    for(auto s:spans) {
        if(count && spans[count-1].second>=s.first)spans[count-1].second=std::max(spans[count-1].second,s.second);
        else spans[count++]=s;
    }
    spans.resize(count);
}
inline Rows dilate(const Rows& rows,int radius) {
    Rows out;
    for(const auto& entry:rows)for(int dy=-radius;dy<=radius;++dy) {
        int dx=int(floor(sqrt(double(radius*radius-dy*dy))));
        auto& dest=out[entry.first+dy];
        for(auto s:entry.second)dest.push_back({s.first-dx,s.second+dx});
    }
    for(auto& entry:out)normalize(entry.second);
    return out;
}
inline void mergeQuads(const Rows& rows,std::vector<Quad>& quads) {
    std::map<Span,Span> active;int previousY=0;bool first=true;
    for(const auto& entry:rows) {
        int y=entry.first;
        if(!first && y!=previousY+1) {
            for(const auto& r:active)addRect(quads,r.first.first,r.first.second,r.second.first,r.second.second);
            active.clear();
        }
        std::set<Span> present(entry.second.begin(),entry.second.end());
        for(auto it=active.begin();it!=active.end();) {
            if(!present.count(it->first)) {
                addRect(quads,it->first.first,it->first.second,it->second.first,it->second.second);it=active.erase(it);
            } else ++it;
        }
        for(auto span:entry.second) {
            auto it=active.find(span);
            if(it==active.end())active.emplace(span,Span{y,y+1});else it->second.second=y+1;
        }
        first=false;previousY=y;
    }
    for(const auto& r:active)addRect(quads,r.first.first,r.first.second,r.second.first,r.second.second);
}
class Level {
    exploration::Mask candidates_{1},floor_{1},known_{1};
    std::set<int> connectionRows_;
    bool roomBudgetReached_=false;
    std::size_t capturedCells_=0;
    std::set<std::tuple<int,int,int,int>> rooms_;
    std::vector<Stroke> permanentWalls_;
    std::size_t wallRevision_=0;
    Row fineFloor(int y) const {
        Row result;
        for(auto s:rowAt(floor_.rows(),floor_div(y,4)))result.push_back({s.first*4,s.second*4});
        return result;
    }
    Rows openFrontier(const Rows& visible) const {
        Rows seeds;
        for(const auto& entry:visible) {
            int y=entry.first;Row row;const auto floor=fineFloor(y);
            for(int dy:{-1,1}) {
                auto edge=overlap(overlap(subtract(entry.second,rowAt(visible,y+dy)),floor),fineFloor(y+dy));
                row.insert(row.end(),edge.begin(),edge.end());
            }
            for(auto s:entry.second) {
                for(int side:{-1,1}) {
                    int x=side<0?s.first:s.second-1,outside=x+side;
                    if(floor_.contains({(x+.5)*.25,(y+.5)*.25}) && floor_.contains({(outside+.5)*.25,(y+.5)*.25}))
                        row.push_back({x,x+1});
                }
            }
            if(!row.empty()){normalize(row);seeds.emplace(y,std::move(row));}
        }
        return dilate(seeds,12);
    }
    void buildWalls() {
        permanentWalls_.clear();
        floor_.frontier([&](Point a,Point b){
            // Unknown/unloaded neighbors must never become invented walls.
            int count=int(std::abs(b.x-a.x)+std::abs(b.y-a.y));
            if(!count)return;
            Point step{(b.x-a.x)/count,(b.y-a.y)/count};
            Point normal{step.y*.5,-step.x*.5};int start=-1;
            for(int i=0;i<=count;++i) {
                Point mid{a.x+step.x*(i+.5),a.y+step.y*(i+.5)};
                bool keep=i<count && known_.contains({mid.x+normal.x,mid.y+normal.y}) &&
                    known_.contains({mid.x-normal.x,mid.y-normal.y});
                if(keep && start<0)start=i;
                if(!keep && start>=0){
                    permanentWalls_.push_back({{a.x+step.x*start,a.y+step.y*start},{a.x+step.x*i,a.y+step.y*i}});start=-1;
                }
            }
        });
        wallRevision_=revision;
    }
public:
    std::size_t revision=0;
    bool wanted(int x,int y,int w,int h) const {return !roomBudgetReached_ && rooms_.size()<2048 && !rooms_.count({x,y,w,h});}
    void ingest(int x,int y,int w,int h,const std::vector<std::uint16_t>& flags) {
        if(w<1 || h<1 || w>512 || h>512 || flags.size()!=std::size_t(w)*h || !wanted(x,y,w,h))return;
        if(capturedCells_+flags.size()>2000000){roomBudgetReached_=true;return;}
        capturedCells_+=flags.size();
        for(int row=0;row<h;++row) {
            known_.revealRange(y+row,x,x+w);connectionRows_.insert(y+row);int start=-1;
            for(int col=0;col<=w;++col) {
                // WALL and BLANK are permanent terrain. Creature, missile,
                // item, object and door bits must not punch transient holes.
                bool floor=col<w && !(flags[std::size_t(row)*w+col]&0x0021);
                if(floor && start<0)start=col;
                if(!floor && start>=0){candidates_.revealRange(y+row,x+start,x+col);start=-1;}
            }
        }
        rooms_.insert({x,y,w,h});++revision;
    }
    bool connect(Point player) {
        int px=int(floor(player.x)),py=int(floor(player.y));
        if(!candidates_.cell(px,py))return false;
        std::vector<std::pair<int,int>> queue;
        // Only newly loaded rows can bridge existing components. Old connected
        // runs stay in floor_; never flood all previously visited cells again.
        for(int y:connectionRows_) {
            for(auto span:subtract(rowAt(candidates_.rows(),y),rowAt(floor_.rows(),y))) {
                bool adjacent=floor_.cell(span.first-1,y) || floor_.cell(span.second,y);
                if(!adjacent)for(int dy:{-1,1})
                    if(!overlap(Row{span},rowAt(floor_.rows(),y+dy)).empty()){adjacent=true;break;}
                if(adjacent)queue.push_back({span.first,y});
            }
        }
        if(!floor_.cell(px,py))queue.push_back({px,py});
        if(!queue.empty())++revision;
        for(std::size_t i=0;i<queue.size();++i) {
            const auto p=queue[i];if(floor_.cell(p.first,p.second))continue;
            const auto& source=rowAt(candidates_.rows(),p.second);
            auto found=std::upper_bound(source.begin(),source.end(),p.first,[](int x,Span span){return x<span.first;});
            if(found==source.begin())continue;
            const auto candidate=*std::prev(found);if(p.first>=candidate.second)continue;
            auto runs=subtract(Row{candidate},rowAt(floor_.rows(),p.second));
            for(auto run:runs)if(p.first>=run.first && p.first<run.second) {
                floor_.revealRange(p.second,run.first,run.second);
                for(int dy:{-1,1}) {
                    auto fresh=subtract(overlap(Row{run},rowAt(candidates_.rows(),p.second+dy)),rowAt(floor_.rows(),p.second+dy));
                    for(auto span:fresh)queue.push_back({span.first,p.second+dy});
                }
                break;
            }
        }
        connectionRows_.clear();return true;
    }
    const Rows& floorRows() const {return floor_.rows();}
    const Rows& knownRows() const {return known_.rows();}
    bool contains(Point p) const {return floor_.contains(p);}
    std::size_t size() const {return floor_.size();}
    std::size_t roomCount() const {return rooms_.size();}
    template<class VisibleMask> Drawing build(const VisibleMask& explored,const exploration::Rect* region=nullptr) {
        Drawing drawing;
        if(wallRevision_!=revision)buildWalls();
        const auto& visible=explored.rows();
        // Color only edges with known open floor on BOTH sides of the mask.
        // A mask edge that terminates at a real wall has no red seed.
        const auto open=openFrontier(visible);
        constexpr int radii[]={0,1,3,5,7,9,12};
        Rows outer=visible;
        for(std::size_t layer=0;layer<drawing.layers.size();++layer) {
            Rows inner=layer+1<drawing.layers.size()?erode(visible,radii[layer+1]):Rows{};
            // Join equal runs vertically; every floor point belongs to exactly
            // one shade band, avoiding darkening from repeated alpha blending.
            Rows grayRows,redRows;
            for(const auto& entry:outer) {
                int y=entry.first;
                Row spans=overlap(subtract(entry.second,rowAt(inner,y)),fineFloor(y));
                Row red=layer+1<drawing.layers.size()?overlap(spans,rowAt(open,y)):Row{};
                Row gray=subtract(spans,red);
                if(!gray.empty())grayRows.emplace(y,std::move(gray));
                if(!red.empty())redRows.emplace(y,std::move(red));
            }
            mergeQuads(grayRows,drawing.layers[layer].quads);mergeQuads(redRows,drawing.layers[layer].redQuads);
            drawing.quads+=drawing.layers[layer].quads.size()+drawing.layers[layer].redQuads.size();outer=std::move(inner);
        }
        // Native floor boundaries are axis-aligned in world space. Clip them
        // at quarter-subtile precision, then merge retained adjacent pieces.
        for(auto line:permanentWalls_) {
            if(region && (std::max(line.a.x,line.b.x)<region->left*.25 || std::min(line.a.x,line.b.x)>region->right*.25 ||
                std::max(line.a.y,line.b.y)<region->top*.25 || std::min(line.a.y,line.b.y)>region->bottom*.25))continue;
            if(region) {
                line.a.x=std::clamp(line.a.x,region->left*.25,region->right*.25);
                line.b.x=std::clamp(line.b.x,region->left*.25,region->right*.25);
                line.a.y=std::clamp(line.a.y,region->top*.25,region->bottom*.25);
                line.b.y=std::clamp(line.b.y,region->top*.25,region->bottom*.25);
            }
            int count=int(lround((std::abs(line.b.x-line.a.x)+std::abs(line.b.y-line.a.y))*4));
            if(count<1)continue;
            Point step{(line.b.x-line.a.x)/count,(line.b.y-line.a.y)/count};int start=-1;
            Point side{step.y*.01,-step.x*.01};
            for(int i=0;i<=count;++i) {
                Point p{line.a.x+step.x*(i+.5),line.a.y+step.y*(i+.5)};
                bool keep=i<count && (explored.contains({p.x+side.x,p.y+side.y}) || explored.contains({p.x-side.x,p.y-side.y}));
                if(keep && start<0)start=i;
                if(!keep && start>=0) {
                    drawing.walls.push_back({{line.a.x+step.x*start,line.a.y+step.y*start},{line.a.x+step.x*i,line.a.y+step.y*i}});start=-1;
                }
            }
        }
        return drawing;
    }
private:
    static int floor_div(int v,int divisor) {int q=v/divisor;return q-(v%divisor<0);}
};
}
