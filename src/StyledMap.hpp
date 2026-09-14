// Connected floors, shade bands, contours and frontier geometry.
// Full rebuilds provide the incremental-cache reference.
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
struct Stroke {Point a,b;bool bank=false;};
struct Layer {unsigned gray,alpha;std::vector<Quad> quads;std::vector<Quad> redQuads{};};
struct Drawing {
    // Low outer rim, bright inner rim, then a gradual fade into the floor.
    std::array<Layer,7> layers{{{52,70,{}},{116,175,{}},{96,150,{}},{65,115,{}},{38,80,{}},{23,55,{}},{14,40,{}}}};
    std::vector<Stroke> walls;
    std::size_t quads=0;
};
// Remove artificial splits in axis-aligned walls without bridging any gap.
// Runs remain in world coordinates, so both zooms use the same exact endpoints.
inline void compactWalls(std::vector<Stroke>& walls) {
    auto vertical=[](const Stroke& w){return w.a.x==w.b.x;};
    for(auto& w:walls) {
        if((vertical(w) && w.a.y>w.b.y) || (!vertical(w) && w.a.x>w.b.x))std::swap(w.a,w.b);
    }
    auto key=[&](const Stroke& w){return std::make_tuple(vertical(w),vertical(w)?w.a.x:w.a.y,
        vertical(w)?w.a.y:w.a.x);};
    std::sort(walls.begin(),walls.end(),[&](const Stroke& a,const Stroke& b){return key(a)<key(b);});
    std::size_t keep=0;
    for(auto w:walls) {
        if(keep) {
            auto& last=walls[keep-1];
            const bool straight=(vertical(last) && vertical(w)) ||
                (last.a.y==last.b.y && w.a.y==w.b.y);
            if(straight && last.bank==w.bank && last.b.x==w.a.x && last.b.y==w.a.y){last.b=w.b;continue;}
        }
        walls[keep++]=w;
    }
    walls.resize(keep);
}
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
    exploration::Mask candidates_{1},floor_{1},known_{1},banks_{1};
    std::set<int> connectionRows_;
    bool roomBudgetReached_=false;
    std::size_t capturedCells_=0;
    std::set<std::tuple<int,int,int,int>> rooms_;
    std::vector<Stroke> permanentWalls_;
    std::size_t wallRevision_=0;
    mutable Rows fineFloorRows_;
    mutable std::size_t fineFloorRevision_=0;
    const Row& fineFloor(int y) const {
        if(fineFloorRevision_!=revision){fineFloorRows_.clear();fineFloorRevision_=revision;}
        const int coarse=floor_div(y,4);
        auto [it,inserted]=fineFloorRows_.try_emplace(coarse);
        if(inserted)for(auto s:rowAt(floor_.rows(),coarse))it->second.push_back({s.first*4,s.second*4});
        return it->second;
    }
    Row boundarySpace(int y,const Row& spans,bool throughUnknown,const Rows* reachable=nullptr) const {
        if(!throughUnknown)return overlap(spans,fineFloor(y));
        if(reachable)return overlap(spans,rowAt(*reachable,y));
        Row blocked;
        for(auto s:subtract(rowAt(known_.rows(),floor_div(y,4)),rowAt(floor_.rows(),floor_div(y,4))))
            blocked.push_back({s.first*4,s.second*4});
        return subtract(spans,blocked);
    }
    Rows openFrontier(const Rows& visible,int width,bool throughUnknown,const Rows* reachable) const {
        Rows seeds;
        for(const auto& entry:visible) {
            int y=entry.first;Row row;
            for(int dy:{-1,1}) {
                auto edge=boundarySpace(y+dy,boundarySpace(y,subtract(entry.second,rowAt(visible,y+dy)),throughUnknown,reachable),throughUnknown,reachable);
                row.insert(row.end(),edge.begin(),edge.end());
            }
            for(auto s:entry.second) {
                for(int side:{-1,1}) {
                    int x=side<0?s.first:s.second-1,outside=x+side;
                    if(!boundarySpace(y,{{x,x+1}},throughUnknown,reachable).empty() && !boundarySpace(y,{{outside,outside+1}},throughUnknown,reachable).empty())
                        row.push_back({x,x+1});
                }
            }
            if(!row.empty()){normalize(row);seeds.emplace(y,std::move(row));}
        }
        return dilate(seeds,width);
    }
    void buildWalls() {
        permanentWalls_.clear();
        floor_.frontier([&](Point a,Point b){
            // Unknown/unloaded neighbors must never become invented walls.
            int count=int(std::abs(b.x-a.x)+std::abs(b.y-a.y));
            if(!count)return;
            Point step{(b.x-a.x)/count,(b.y-a.y)/count};
            Point normal{step.y*.5,-step.x*.5};int start=-1;bool bank=false;
            for(int i=0;i<=count;++i) {
                Point mid{a.x+step.x*(i+.5),a.y+step.y*(i+.5)};
                bool keep=i<count && known_.contains({mid.x+normal.x,mid.y+normal.y}) &&
                    known_.contains({mid.x-normal.x,mid.y-normal.y});
                const bool material=keep && (banks_.contains({mid.x+normal.x,mid.y+normal.y}) ||
                    banks_.contains({mid.x-normal.x,mid.y-normal.y}));
                if(start>=0 && (!keep || bank!=material)){
                    permanentWalls_.push_back({{a.x+step.x*start,a.y+step.y*start},{a.x+step.x*i,a.y+step.y*i},bank});start=-1;
                }
                if(keep && start<0){start=i;bank=material;}
            }
        });
        wallRevision_=revision;
    }
public:
    std::size_t revision=0;
    bool wanted(int x,int y,int w,int h) const {return !roomBudgetReached_ && rooms_.size()<2048 && !rooms_.count({x,y,w,h});}
    void ingest(int x,int y,int w,int h,const std::vector<std::uint16_t>& flags,const std::vector<std::uint8_t>& banks={}) {
        if(w<1 || h<1 || w>512 || h>512 || flags.size()!=std::size_t(w)*h || !wanted(x,y,w,h))return;
        if(capturedCells_+flags.size()>2000000){roomBudgetReached_=true;return;}
        capturedCells_+=flags.size();
        if(banks.size()==flags.size())for(int row=0;row<h;++row)for(int col=0;col<w;++col)
            if(banks[std::size_t(row)*w+col] && (flags[std::size_t(row)*w+col]&0x27)==1)
                banks_.revealRange(y+row,x+col,x+col+1);
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
    Rows reachableBoundary(const Rows& visible) const {
        // Only unknown space connected to discovered floor may carry a loading
        // frontier. A thin known wall must also stop an arc on its far side.
        // The one-cell halo lets the edge test inspect its outside neighbor.
        const auto halo=dilate(visible,1);
        Rows available,result;Row known;int coarse=0;bool haveCoarse=false;
        for(const auto& [y,row]:halo) {
            const int cy=floor_div(y,4);
            if(!haveCoarse || coarse!=cy) {
                coarse=cy;haveCoarse=true;known.clear();
                for(auto span:rowAt(known_.rows(),cy))known.push_back({span.first*4,span.second*4});
            }
            auto unknown=subtract(row,known);
            if(!unknown.empty())available.emplace(y,std::move(unknown));
            auto floor=overlap(row,fineFloor(y));
            if(!floor.empty())result.emplace(y,std::move(floor));
        }
        if(!floor_.size())return available;
        if(available.empty())return result;
        // Known floor is already a seed. Connect only the unknown intervals;
        // dense rooms and their interior obstacles need no graph nodes.
        auto touches=[&](int y,Span span) {
            const auto& row=rowAt(result,y);
            auto it=std::lower_bound(row.begin(),row.end(),span.first,[](Span s,int x){return s.second<=x;});
            return it!=row.end() && it->first<span.second;
        };
        struct Node {std::size_t parent;bool seeded;};
        std::vector<Node> nodes;
        auto root=[&](std::size_t i){while(nodes[i].parent!=i){nodes[i].parent=nodes[nodes[i].parent].parent;i=nodes[i].parent;}return i;};
        std::size_t previousStart=0;const Row* previous=nullptr;int previousY=0;
        for(const auto& [y,row]:available) {
            const auto start=nodes.size();
            for(auto span:row) {
                nodes.push_back({nodes.size(),touches(y,{span.first-1,span.second+1}) || touches(y-1,span) || touches(y+1,span)});
            }
            if(previous && y==previousY+1) {
                std::size_t i=0,j=0;
                while(i<row.size() && j<previous->size()) {
                    if(row[i].first<(*previous)[j].second && (*previous)[j].first<row[i].second) {
                        const auto a=root(start+i),b=root(previousStart+j);
                        if(a!=b){nodes[b].parent=a;nodes[a].seeded=nodes[a].seeded || nodes[b].seeded;}
                    }
                    if(row[i].second<(*previous)[j].second)++i;else ++j;
                }
            }
            previous=&row;previousY=y;previousStart=start;
        }
        std::size_t index=0;
        for(const auto& [y,row]:available)for(auto span:row) {
            if(nodes[root(index)].seeded)result[y].push_back(span);
            ++index;
        }
        for(auto& [y,row]:result)normalize(row);
        return result;
    }
    template<class VisibleMask> Drawing build(const VisibleMask& explored,const exploration::Rect* region=nullptr,int width=12,bool throughUnknown=false,const Rows* reachable=nullptr) {
        Drawing drawing;
        if(wallRevision_!=revision)buildWalls();
        const auto& visible=explored.rows();
        Rows boundary;
        if(throughUnknown && !reachable){boundary=reachableBoundary(visible);reachable=&boundary;}
        width=std::clamp(width,6,24);
        const auto open=openFrontier(visible,width,throughUnknown,reachable);
        constexpr int radii[]={0,1,3,5,7,9,12};
        Rows outer=visible;
        for(std::size_t layer=0;layer<drawing.layers.size();++layer) {
            Rows inner=layer+1<drawing.layers.size()?erode(visible,std::max(1,(radii[layer+1]*width+6)/12)):Rows{};
            // Join equal runs vertically; every floor point belongs to exactly
            // one shade band, avoiding darkening from repeated alpha blending.
            Rows grayRows,redRows;
            for(const auto& entry:outer) {
                int y=entry.first;
                Row band=subtract(entry.second,rowAt(inner,y));
                Row red=layer+1<drawing.layers.size()?overlap(boundarySpace(y,band,throughUnknown,reachable),rowAt(open,y)):Row{};
                Row gray=subtract(overlap(band,fineFloor(y)),red);
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
                    drawing.walls.push_back({{line.a.x+step.x*start,line.a.y+step.y*start},{line.a.x+step.x*i,line.a.y+step.y*i},line.bank});start=-1;
                }
            }
        }
        return drawing;
    }
private:
    static int floor_div(int v,int divisor) {int q=v/divisor;return q-(v%divisor<0);}
};
}
