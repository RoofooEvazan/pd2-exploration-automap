// Caches the exploration mask as screen-space row spans for native artwork.
// Reuses the projected coverage while the viewport pans, so each sprite can
// obtain visible clipping regions without retesting every explored cell.

#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace exploration {
// Sparse, half-open spans in stable automap raster coordinates, before pan.
// Updated on exploration changes rather than tested pixel-by-pixel per sprite.
class ProjectedMask {
    using Span=std::pair<int,int>;
    std::map<int,std::vector<Span>> rows_;
    static std::int64_t floorDiv(std::int64_t a,std::int64_t b) {
        auto q=a/b;return q-((a%b)<0);
    }
    static std::int64_t ceilDiv(std::int64_t a,std::int64_t b) {return -floorDiv(-a,b);}
    void add(int y,int left,int right) {
        if(left>=right)return;
        auto& row=rows_[y];
        auto first=std::lower_bound(row.begin(),row.end(),left,[](Span s,int x){return s.second<x;});
        auto last=first;
        int l=left,r=right;
        while(last!=row.end() && last->first<=r){l=std::min(l,last->first);r=std::max(r,last->second);++last;}
        if(first!=last && std::next(first)==last && first->first==l && first->second==r)return;
        first=row.erase(first,last);row.insert(first,{l,r});
    }
public:
    // Rasterize a whole quarter-cell rectangle without expanding each row.
    // Town footprints use this to retain native art without rebuilding a mask.
    void revealQuarterRect(int left,int top,int right,int bottom,int divisor) {
        if(left>=right || top>=bottom)return;
        const std::int64_t d=divisor,l=left,r=right,t=top,b=bottom;
        int firstY=int(ceilDiv(4*(l+t)-d,2*d));
        int lastY=int(ceilDiv(4*(r+b)-d,2*d));
        for(int j=firstY;j<lastY;++j) {
            const std::int64_t row=j;
            auto low=std::max(ceilDiv(16*l-d*(4*row+3),2*d),floorDiv(d*(4*row+1)-16*b,2*d)+1);
            auto high=std::min(ceilDiv(16*r-d*(4*row+3),2*d),floorDiv(d*(4*row+1)-16*t,2*d)+1);
            add(j,int(low),int(high));
        }
    }
    // Exact rational rasterization for quarter-subtile world spans and D2's
    // divisors 10/20. Pixel centers and exclusive world edges match contains().
    void revealQuarterSpan(int left,int right,int y,int divisor) {
        const std::int64_t d=divisor,l=left,r=right,t=y;
        int firstY=int(ceilDiv(4*(l+t)-d,2*d));
        int lastY=int(ceilDiv(4*(r+t+1)-d,2*d));
        for(int j=firstY;j<lastY;++j) {
            const std::int64_t row=j;
            auto low=std::max(ceilDiv(16*l-d*(4*row+3),2*d),floorDiv(d*(4*row+1)-16*(t+1),2*d)+1);
            auto high=std::min(ceilDiv(16*r-d*(4*row+3),2*d),floorDiv(d*(4*row+1)-16*t,2*d)+1);
            add(j,int(low),int(high));
        }
    }
    template<class Emit> void clip(int left,int top,int right,int bottom,Emit emit) const {
        if(left>=right || top>=bottom)return;
        for(auto row=rows_.lower_bound(top);row!=rows_.end() && row->first<bottom;++row) {
            auto first=std::upper_bound(row->second.begin(),row->second.end(),left,[](int x,Span s){return x<s.second;});
            for(auto s=first;s!=row->second.end() && s->first<right;++s)
                emit(std::max(left,s->first),row->first,std::min(right,s->second),row->first+1);
        }
    }
};
}
