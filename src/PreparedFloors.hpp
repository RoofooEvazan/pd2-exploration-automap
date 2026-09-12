// Prepare floor projection and bounds on the existing worker. Screen pan only
// translates these owned doubles; crossing polygons keep the exact clipper.
#pragma once
#include "StyledProjection.hpp"
#include <memory>

namespace styled_map {
class PreparedFloors {
    struct Item {Quad q;double left,top,right,bottom;};
    std::array<std::vector<Item>,14> groups_;
public:
    static constexpr std::size_t quadLimit=65536; // At most 6 MiB of item payload.
    static std::unique_ptr<PreparedFloors> build(const Drawing& source) {
        std::size_t count=0;
        for(const auto& layer:source.layers){count+=layer.quads.size()+layer.redQuads.size();if(count>quadLimit)return {};}
        auto result=std::make_unique<PreparedFloors>();
        auto project=[](Point p){return Point{16*(p.x-p.y)/10,8*(p.x+p.y)/10};};
        for(std::size_t i=0;i<source.layers.size();++i)for(int red:{0,1}) {
            const auto& quads=red?source.layers[i].redQuads:source.layers[i].quads;
            auto& group=result->groups_[i*2+red];group.reserve(quads.size());
            for(const auto& q:quads) {
                Quad p{project(q.a),project(q.b),project(q.c),project(q.d)};
                group.push_back({p,std::min({p.a.x,p.b.x,p.c.x,p.d.x}),std::min({p.a.y,p.b.y,p.c.y,p.d.y}),
                    std::max({p.a.x,p.b.x,p.c.x,p.d.x}),std::max({p.a.y,p.b.y,p.c.y,p.d.y})});
            }
        }
        return result;
    }
    template<class Emit> void clip(std::size_t layer,bool red,int divisor,double ox,double oy,exploration::Rect view,Emit emit) const {
        if(view.left>=view.right || view.top>=view.bottom)return;
        const double scale=divisor==20?.5:1,sx=divisor==20?7:8,sy=divisor==20?-3:-8;
        auto move=[&](Point p){return Point{p.x*scale-ox+sx,p.y*scale-oy+sy};};
        for(const auto& item:groups_[layer*2+red]) {
            const double l=item.left*scale-ox+sx,r=item.right*scale-ox+sx;
            const double t=item.top*scale-oy+sy,b=item.bottom*scale-oy+sy;
            if(r<=view.left || l>=view.right || b<=view.top || t>=view.bottom)continue;
            const auto& q=item.q;Quad screen{move(q.a),move(q.b),move(q.c),move(q.d)};
            if(l>=view.left && r<=view.right && t>=view.top && b<=view.bottom)emit(screen);
            else clipQuad(screen,view,emit);
        }
    }
};
}
