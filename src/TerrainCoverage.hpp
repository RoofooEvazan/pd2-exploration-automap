// Immutable coverage of the collision snapshot used by a completed drawing.
#pragma once
#include "StyledMap.hpp"

namespace styled_map {
class TerrainCoverage {
    exploration::ProjectedMask raster_[2];
public:
    explicit TerrainCoverage(const Rows& known) {
        // Keep a one-subtile rim where an unloaded neighbor can still affect
        // the contour. Erode the union, leaving no seams between known rooms.
        std::vector<Quad> rectangles;
        mergeQuads(erode(known,1),rectangles);
        for(auto rectangle:rectangles) {
            // mergeQuads scales grid coordinates by 1/4. Here the input grid
            // is whole subtiles, so multiply by 16 to get quarter-subtile units.
            for(int zoom=0;zoom<2;++zoom)raster_[zoom].revealQuarterRect(
                int(lround(rectangle.a.x*16)),int(lround(rectangle.a.y*16)),
                int(lround(rectangle.c.x*16)),int(lround(rectangle.c.y*16)),zoom?20:10);
        }
    }
    template<class Emit> void uncovered(exploration::Rect stable,int divisor,Emit emit) const {
        raster_[divisor==20?1:0].clipOutside(stable.left,stable.top,stable.right,stable.bottom,
            [&](int l,int t,int r,int b){emit(exploration::Rect{l,t,r,b});});
    }
};
}
