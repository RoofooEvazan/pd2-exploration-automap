// Union the edges of native sewer-water diamonds, keeping internal tile seams
// out of the outline. Native bridge cells are deliberately not water tiles.
#pragma once
#include "ExplorationMask.hpp"
#include <array>
#include <cstdint>

namespace exploration {
class SewerWater {
    using Tile=std::array<int,3>;
    using Edge=std::array<int,4>;
    std::vector<Tile> tiles_;
    std::array<std::uint16_t,16384> table_{};
    mutable std::vector<Tile> ordered_,edgeTiles_;
    mutable std::vector<Edge> edges_;
    mutable bool dirty_=true;
    mutable std::size_t edgeBuilds_=0;
public:
    static constexpr std::size_t tileLimit=8192;
    void clear(){if(!tiles_.empty())table_.fill(0);tiles_.clear();dirty_=true;}
    bool add(Rect asset) {
        const int w=asset.right-asset.left,h=asset.bottom-asset.top;
        if((w!=8 && w!=16) || h!=w*2)return false;
        const Tile tile{asset.left,asset.bottom,w};
        std::uint32_t hash=2166136261u;for(auto value:tile){hash^=std::uint32_t(value);hash*=16777619u;hash^=hash>>16;}
        std::size_t slot=hash&(table_.size()-1);
        while(table_[slot]){if(tiles_[table_[slot]-1]==tile)return true;slot=(slot+1)&(table_.size()-1);}
        if(tiles_.size()>=tileLimit)return false;
        tiles_.push_back(tile);table_[slot]=std::uint16_t(tiles_.size());dirty_=true;
        return true;
    }
    const auto& tiles() const {
        if(dirty_){ordered_=tiles_;std::sort(ordered_.begin(),ordered_.end());dirty_=false;}return ordered_;
    }
    const auto& edges() const {
        const auto& current=tiles();
        if(current!=edgeTiles_){
            ++edgeBuilds_;
            edges_.clear();
            for(auto tile:current){
                int x=tile[0],b=tile[1],w=tile[2];
                std::array<std::array<int,2>,4> p{{{x,b-w/4},{x+w/2,b-w/2},{x+w,b-w/4},{x+w/2,b}}};
                for(int i=0;i<4;++i){auto a=p[i],z=p[(i+1)%4];if(z<a)std::swap(a,z);edges_.push_back({a[0],a[1],z[0],z[1]});}
            }
            std::sort(edges_.begin(),edges_.end());std::size_t keep=0;
            for(std::size_t i=0;i<edges_.size();){auto j=i+1;while(j<edges_.size() && edges_[j]==edges_[i])++j;
                if((j-i)%2)edges_[keep++]=edges_[i];i=j;}
            edges_.resize(keep);edgeTiles_=current;
        }
        return edges_;
    }
    std::size_t edgeBuilds() const{return edgeBuilds_;}
};
}
