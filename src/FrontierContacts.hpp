#pragma once
#include "ExplorationMask.hpp"

namespace exploration {
// Occupancy only: no colors or geometry from the asset are changed. DC6 rows
// are encoded bottom first; keep decoded spans in screen (top first) order.
struct Silhouette {
    using Span=std::pair<int,int>;
    std::vector<std::vector<Span>> rows;
    bool decode(const unsigned char* data,std::size_t size,int width,int height) {
        rows.clear();
        if(!data || width<1 || width>512 || height<1 || height>512)return false;
        std::vector<std::vector<Span>> decoded(static_cast<std::size_t>(height));
        std::size_t pos=0;int x=0,y=height-1;
        while(pos<size && y>=0) {
            unsigned code=data[pos++];
            if(code==128){x=0;--y;continue;}
            int count=code&127;
            if(!count || x+count>width)return false;
            if(code<128) {
                if(static_cast<std::size_t>(count)>size-pos)return false;
                // Palette index zero is transparent in the native texture.
                int start=-1;
                for(int i=0;i<count;++i) {
                    if(data[pos+i] && start<0)start=x+i;
                    if(!data[pos+i] && start>=0){decoded[y].push_back({start,x+i});start=-1;}
                }
                if(start>=0)decoded[y].push_back({start,x+count});
                pos+=count;
            }
            x+=count;
        }
        if(y!=-1 || pos!=size)return false;
        rows=std::move(decoded);return true;
    }
};

class FrontierContacts {
    struct Box { double left,top,right,bottom; };
    using Range=std::pair<double,double>;
    struct Edge { Point a,b;std::vector<Range> ranges; };
    std::vector<Edge> edges_;
    std::vector<std::vector<std::size_t>> bins_;
    std::vector<unsigned> stamps_;
    unsigned stamp_=0;
    int columns_=0,rows_=0;
    Rect view_{};
    static constexpr int binSize=16;
    std::vector<std::size_t> candidates_;
    static bool cut(Point a,Point b,Box box,Range& range) {
        double lo=0,hi=1,dx=b.x-a.x,dy=b.y-a.y;
        double p[]={-dx,dx,-dy,dy},q[]={a.x-box.left,box.right-a.x,a.y-box.top,box.bottom-a.y};
        for(int i=0;i<4;++i) {
            if(p[i]==0){if(q[i]<0)return false;}
            else {double v=q[i]/p[i];if(p[i]<0)lo=std::max(lo,v);else hi=std::min(hi,v);if(lo>hi)return false;}
        }
        range={lo,hi};return true;
    }
    static Point at(const Edge& edge,double t) {
        return {edge.a.x+(edge.b.x-edge.a.x)*t,edge.a.y+(edge.b.y-edge.a.y)*t};
    }
    template<class Visit> void binsFor(Box box,Visit visit) const {
        if(!columns_ || box.right<view_.left || box.bottom<view_.top || box.left>=view_.right || box.top>=view_.bottom)return;
        int x1=std::max(0,int(floor((box.left-view_.left)/binSize)));
        int y1=std::max(0,int(floor((box.top-view_.top)/binSize)));
        int x2=std::min(columns_-1,int(floor((box.right-view_.left)/binSize)));
        int y2=std::min(rows_-1,int(floor((box.bottom-view_.top)/binSize)));
        for(int y=y1;y<=y2;++y)for(int x=x1;x<=x2;++x)visit(std::size_t(y)*columns_+x);
    }
    template<class Visit> void nearby(Box box,Visit visit) {
        if(++stamp_==0){std::fill(stamps_.begin(),stamps_.end(),0);++stamp_;}
        binsFor(box,[&](std::size_t bin){for(auto id:bins_[bin])if(stamps_[id]!=stamp_){
            stamps_[id]=stamp_;visit(id);
        }});
    }
    void support(Point a,Point b,double padding) {
        // Each actual hit is restricted to one artwork row. Its small padded
        // envelope reaches neighboring frontier segments around corners too.
        Box box{std::min(a.x,b.x)-padding,std::min(a.y,b.y)-padding,
                std::max(a.x,b.x)+padding,std::max(a.y,b.y)+padding};
        nearby(box,[&](std::size_t id){auto& edge=edges_[id];Range r;
            if(cut(edge.a,edge.b,box,r))edge.ranges.push_back(r);
        });
    }
public:
    void clear() {
        edges_.clear();stamps_.clear();candidates_.clear();
        for(auto& bin:bins_)bin.clear();
    }
    void begin(Rect view) {
        clear();view_=view;
        columns_=std::max(0,(view.right-view.left+binSize-1)/binSize);
        rows_=std::max(0,(view.bottom-view.top+binSize-1)/binSize);
        bins_.resize(std::size_t(columns_)*rows_);
    }
    void add(Point a,Point b) {
        Range r;
        if(!columns_ || !rows_ || !cut(a,b,{double(view_.left),double(view_.top),double(view_.right-1),double(view_.bottom-1)},r))return;
        Edge edge{a,b,{}};a=at(edge,r.first);b=at(edge,r.second);
        if(hypot(b.x-a.x,b.y-a.y)<1e-6)return;
        auto id=edges_.size();edges_.push_back({a,b,{}});stamps_.push_back(0);
        binsFor({std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)},
            [&](std::size_t bin){bins_[bin].push_back(id);});
    }
    bool crosses(Rect bounds) {
        bool found=false;
        nearby({double(bounds.left),double(bounds.top),double(bounds.right),double(bounds.bottom)},[&](std::size_t id){Range r;
            if(cut(edges_[id].a,edges_[id].b,{double(bounds.left),double(bounds.top),double(bounds.right),double(bounds.bottom)},r))found=true;
        });
        return found;
    }
    void touch(const Silhouette& shape,Rect bounds,double padding=2.0) {
        candidates_.clear();
        nearby({double(bounds.left),double(bounds.top),double(bounds.right),double(bounds.bottom)},
            [&](std::size_t id){candidates_.push_back(id);});
        for(auto id:candidates_) {
            // Copy endpoints because support appends ranges on other edges.
            Edge edge{edges_[id].a,edges_[id].b,{}};Range extent;
            if(!cut(edge.a,edge.b,{double(bounds.left),double(bounds.top),double(bounds.right),double(bounds.bottom)},extent))continue;
            Point a=at(edge,extent.first),b=at(edge,extent.second);
            int first=std::max(0,int(floor(std::min(a.y,b.y)-bounds.top))-1);
            int last=std::min(int(shape.rows.size())-1,int(floor(std::max(a.y,b.y)-bounds.top)));
            for(int y=first;y<=last;++y)for(auto span:shape.rows[y]) {
                Range hit;
                if(cut(edge.a,edge.b,{double(bounds.left+span.first),double(bounds.top+y),
                                    double(bounds.left+span.second),double(bounds.top+y+1)},hit))
                    support(at(edge,hit.first),at(edge,hit.second),padding);
            }
        }
    }
    template<class Emit> void emit(Emit emit) {
        for(auto& edge:edges_) {
            auto& spans=edge.ranges;if(spans.empty())continue;
            std::sort(spans.begin(),spans.end());Range merged=spans[0];
            auto send=[&](){if(merged.second-merged.first>1e-8)emit(at(edge,merged.first),at(edge,merged.second));};
            for(std::size_t i=1;i<spans.size();++i) {
                if(spans[i].first<=merged.second+1e-8)merged.second=std::max(merged.second,spans[i].second);
                else {send();merged=spans[i];}
            }
            send();
        }
    }
    std::size_t size() const {return edges_.size();}
};
}
