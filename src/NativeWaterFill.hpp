// Extract water coverage from the map's explicitly labeled native fill frames.
#pragma once
#include "FrontierContacts.hpp"
#include "StyledMap.hpp"

namespace exploration {
inline bool nativeWaterFill(const unsigned char* bytes,std::size_t length,int width,int height,std::vector<Rect>& rectangles) {
    rectangles.clear();
    if(!bytes || (width!=8 && width!=16) || height!=width*2 || length>2048)return false;
    std::vector<unsigned char> mask(bytes,bytes+length);
    std::size_t pos=0;int x=0,y=height-1;
    while(pos<length && y>=0) {
        unsigned code=mask[pos++];
        if(code==128){x=0;--y;continue;}
        unsigned count=code&127;if(!count || x+int(count)>width)return false;
        if(code<128) {
            if(count>length-pos)return false;
            // These source frames separate water (151) from bridges (255).
            // Only explicit Poisoned Well fill references use this palette.
            for(unsigned i=0;i<count;++i) {
                const auto value=mask[pos+i];if(value!=0 && value!=151 && value!=255)return false;
                mask[pos+i]=value==151?1:0;
            }
            pos+=count;
        }
        x+=count;
    }
    if(y!=-1 || pos!=length)return false;
    Silhouette shape;if(!shape.decode(mask.data(),mask.size(),width,height))return false;
    styled_map::Rows rows;
    for(int row=0;row<height;++row)if(!shape.rows[row].empty())rows.emplace(row,shape.rows[row]);
    std::vector<styled_map::Quad> quads;styled_map::mergeQuads(rows,quads);
    for(const auto& q:quads)rectangles.push_back({int(q.a.x*4),int(q.a.y*4),int(q.c.x*4),int(q.c.y*4)});
    return true;
}
}
