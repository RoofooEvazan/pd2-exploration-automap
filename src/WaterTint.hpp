// Cache color splits where native water tiles meet existing contours.
#pragma once
#include "StyledMap.hpp"
#include <array>
#include <unordered_set>

namespace exploration {
class WaterTint {
    using Tile=std::array<int,3>; // Stable automap left, bottom, width.
    struct Hash {
        std::size_t operator()(const Tile& tile) const {
            std::uint32_t h=2166136261u;
            for(auto v:tile){h^=std::uint32_t(v);h*=16777619u;h^=h>>16;}
            return h;
        }
    };
    std::unordered_set<Tile,Hash> tiles_;
    struct Bin {std::vector<Tile> tiles;std::vector<Rect> pixels;std::size_t revision=0;};
    std::map<std::pair<int,int>,Bin> bins_;
    std::set<std::array<int,4>> pixelTiles_;
    std::size_t revision_=0;
    std::size_t pixelRectangles_=0;
    std::uint64_t session_=0,level_=0;
    int divisor_=0;
    static constexpr double margin=1.5;
    static constexpr double pixelMargin=1.5;
    static int bin(double value){return int(std::floor(value/32));}
    // A supported native floor frame contains a 2:1 diamond at its foot.
    // Intersect with its four half-planes, padded for the contour's texel width.
    static bool cut(Point a,Point b,Tile tile,double& lo,double& hi) {
        const double cx=tile[0]+tile[2]*.5,cy=tile[1]-tile[2]*.25;
        const double extent=tile[2]*.5+margin;
        lo=0;hi=1;
        for(int sx:{-1,1})for(int sy:{-1,1}) {
            const double start=sx*(a.x-cx)+2*sy*(a.y-cy);
            const double delta=sx*(b.x-a.x)+2*sy*(b.y-a.y);
            if(delta==0){if(start>extent)return false;}
            else if(delta>0)hi=std::min(hi,(extent-start)/delta);
            else lo=std::max(lo,(extent-start)/delta);
            if(hi<=lo)return false;
        }
        return hi>lo;
    }
    static bool cutPixels(Point a,Point b,Rect pixels,double& lo,double& hi) {
        // Half-texel downsampling plus native placement rounding can leave the
        // collision shoreline 1.2 texels outside the visible water pixels.
        lo=0;hi=1;
        for(int axis=0;axis<2;++axis) {
            const double start=axis?a.y:a.x,delta=axis?b.y-a.y:b.x-a.x;
            const double low=(axis?pixels.top:pixels.left)-pixelMargin,high=(axis?pixels.bottom:pixels.right)+pixelMargin;
            if(delta==0){if(start<low || start>high)return false;}
            else {
                double l=(low-start)/delta,r=(high-start)/delta;if(l>r)std::swap(l,r);
                lo=std::max(lo,l);hi=std::min(hi,r);
            }
            if(hi<=lo)return false;
        }
        return hi>lo;
    }
public:
    static constexpr std::size_t tileLimit=32768;
    static constexpr std::size_t pixelRectangleLimit=131072;
    static constexpr std::size_t wallCacheLimit=16384,partCacheLimit=65536;
    struct Part {styled_map::Stroke stroke;bool water;};
private:
    std::vector<styled_map::Stroke> source_;
    std::vector<Part> parts_;
    std::size_t preparedTiles_=0,builds_=0;
    using WallKey=std::array<double,5>;
    struct CachedWall {std::size_t revision;std::vector<Part> parts;};
    std::map<WallKey,CachedWall> wallCache_;
    std::size_t cachedParts_=0,wallBuilds_=0;
public:
    void select(std::uint64_t session,std::uint64_t level,int divisor) {
        if(session==session_ && level==level_ && divisor==divisor_)return;
        session_=session;level_=level;divisor_=divisor;
        tiles_.clear();bins_.clear();source_.clear();parts_.clear();preparedTiles_=0;
        pixelTiles_.clear();pixelRectangles_=0;revision_=0;
        wallCache_.clear();cachedParts_=0;
    }
    bool add(Rect frame) {
        const int w=frame.right-frame.left,h=frame.bottom-frame.top;
        if((w!=8 && w!=16) || h!=w*2)return false;
        const Tile tile{frame.left,frame.bottom,w};
        if(tiles_.find(tile)!=tiles_.end())return true;
        if(size()>=tileLimit)return false;
        tiles_.insert(tile);++revision_;
        for(int y=bin(frame.bottom-w*.5-margin);y<=bin(frame.bottom+margin);++y)
            for(int x=bin(frame.left-margin);x<=bin(frame.right+margin);++x) {
                auto& bucket=bins_[{y,x}];bucket.tiles.push_back(tile);bucket.revision=revision_;
            }
        return true;
    }
    bool addPixels(Rect frame,unsigned artwork,const std::vector<Rect>& pixels) {
        const int w=frame.right-frame.left,h=frame.bottom-frame.top;
        if((w!=8 && w!=16) || h!=w*2 || artwork>65535 || pixels.empty())return false;
        const std::array<int,4> tile{frame.left,frame.bottom,w,int(artwork)};
        if(pixelTiles_.count(tile))return true;
        if(size()>=tileLimit || pixels.size()>pixelRectangleLimit-pixelRectangles_)return false;
        for(auto r:pixels)if(r.left<0 || r.top<0 || r.right>w || r.bottom>h || r.left>=r.right || r.top>=r.bottom)return false;
        pixelTiles_.insert(tile);pixelRectangles_+=pixels.size();++revision_;
        for(auto r:pixels) {
            r={r.left+frame.left,r.top+frame.top,r.right+frame.left,r.bottom+frame.top};
            for(int y=bin(r.top-pixelMargin);y<=bin(r.bottom+pixelMargin);++y)
                for(int x=bin(r.left-pixelMargin);x<=bin(r.right+pixelMargin);++x) {
                    auto& bucket=bins_[{y,x}];bucket.pixels.push_back(r);bucket.revision=revision_;
                }
        }
        return true;
    }
    const std::vector<Part>& prepare(const std::vector<styled_map::Stroke>& walls) {
        const auto equal=[](const styled_map::Stroke& a,const styled_map::Stroke& b){
            return a.a.x==b.a.x && a.a.y==b.a.y && a.b.x==b.b.x && a.b.y==b.b.y && a.bank==b.bank;
        };
        if(preparedTiles_==size() && source_.size()==walls.size() && std::equal(source_.begin(),source_.end(),walls.begin(),equal))return parts_;
        ++builds_;source_=walls;preparedTiles_=size();parts_.clear();
        std::vector<std::pair<double,double>> ranges;
        std::vector<const Bin*> buckets;
        for(const auto& wall:walls) {
            ranges.clear();buckets.clear();std::size_t revision=0;
            auto project=[&](Point p){return Point{16*(p.x-p.y)/divisor_,8*(p.x+p.y)/divisor_};};
            const Point a=project(wall.a),b=project(wall.b);
            const int xl=bin(std::min(a.x,b.x)),xr=bin(std::max(a.x,b.x));
            for(int y=bin(std::min(a.y,b.y));y<=bin(std::max(a.y,b.y));++y) {
                for(auto it=bins_.lower_bound({y,xl});it!=bins_.end() && it->first.first==y && it->first.second<=xr;++it) {
                    buckets.push_back(&it->second);revision=std::max(revision,it->second.revision);
                }
            }
            const WallKey key{wall.a.x,wall.a.y,wall.b.x,wall.b.y,double(wall.bank)};
            auto cached=wallCache_.find(key);
            // A new tile can affect only walls crossing its bins. Keep the
            // exact previous partitions for every other wall as exploration grows.
            if(cached!=wallCache_.end() && cached->second.revision==revision) {
                parts_.insert(parts_.end(),cached->second.parts.begin(),cached->second.parts.end());continue;
            }
            ++wallBuilds_;
            for(const auto* bucket:buckets) {
                for(auto tile:bucket->tiles){double lo=0,hi=0;if(cut(a,b,tile,lo,hi))ranges.push_back({lo,hi});}
                for(auto pixels:bucket->pixels){double lo=0,hi=0;if(cutPixels(a,b,pixels,lo,hi))ranges.push_back({lo,hi});}
            }
            const auto start=parts_.size();
            std::sort(ranges.begin(),ranges.end());
            auto at=[&](double t){return Point{wall.a.x+(wall.b.x-wall.a.x)*t,wall.a.y+(wall.b.y-wall.a.y)*t};};
            auto append=[&](double l,double r,bool water){if(r>l)parts_.push_back({{at(l),at(r),wall.bank},water});};
            double cursor=0;
            for(std::size_t i=0;i<ranges.size();) {
                double lo=ranges[i].first,hi=ranges[i++].second;
                while(i<ranges.size() && ranges[i].first<=hi){hi=std::max(hi,ranges[i].second);++i;}
                append(cursor,lo,false);append(lo,hi,true);cursor=hi;
            }
            append(cursor,1,false);
            if(cached!=wallCache_.end()){cachedParts_-=cached->second.parts.size();wallCache_.erase(cached);}
            const auto count=parts_.size()-start;
            if(wallCache_.size()<wallCacheLimit && count<=partCacheLimit-cachedParts_) {
                wallCache_.emplace(key,CachedWall{revision,std::vector<Part>(parts_.begin()+start,parts_.end())});cachedParts_+=count;
            }
        }
        return parts_;
    }
    std::size_t size() const{return tiles_.size()+pixelTiles_.size();}
    std::size_t builds() const{return builds_;}
    std::size_t wallBuilds() const{return wallBuilds_;}
    std::size_t cachedWalls() const{return wallCache_.size();}
    std::size_t cachedParts() const{return cachedParts_;}
};
}
