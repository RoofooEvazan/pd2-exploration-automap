// Clips projected floor quads and wall strokes to the automap viewport.
// Quads wholly inside or outside take fast paths; crossing polygons are split
// into renderer-ready pieces without drawing beyond the viewport boundary.

#pragma once
#include "StyledMap.hpp"

namespace styled_map {
inline bool clipStroke(Point& a,Point& b,exploration::Rect view) {
    double lo=0,hi=1,dx=b.x-a.x,dy=b.y-a.y;
    double p[]={-dx,dx,-dy,dy};
    double q[]={a.x-view.left,view.right-1-a.x,a.y-view.top,view.bottom-1-a.y};
    if(view.left>=view.right || view.top>=view.bottom)return false;
    for(int i=0;i<4;++i) {
        if(p[i]==0){if(q[i]<0)return false;}
        else {double t=q[i]/p[i];if(p[i]<0)lo=std::max(lo,t);else hi=std::min(hi,t);if(lo>hi)return false;}
    }
    b={a.x+hi*dx,a.y+hi*dy};a={a.x+lo*dx,a.y+lo*dy};
    return hypot(b.x-a.x,b.y-a.y)>1e-6;
}
// Clip isometric quads without changing the viewport or scissor state owned
// by the game. Triangles are submitted as degenerate quads to D2GL's fixed
// four-vertex index layout.
template<class Emit> void clipQuad(Quad q,exploration::Rect view,Emit emit) {
    if(view.left>=view.right || view.top>=view.bottom)return;
    const double left=std::min({q.a.x,q.b.x,q.c.x,q.d.x}),right=std::max({q.a.x,q.b.x,q.c.x,q.d.x});
    const double top=std::min({q.a.y,q.b.y,q.c.y,q.d.y}),bottom=std::max({q.a.y,q.b.y,q.c.y,q.d.y});
    if(right<=view.left || left>=view.right || bottom<=view.top || top>=view.bottom)return;
    if(left>=view.left && right<=view.right && top>=view.top && bottom<=view.bottom){emit(q);return;}
    Point buffers[2][16]={{q.a,q.b,q.c,q.d},{}};int count=4,current=0;
    for(int side=0;side<4;++side) {
        int next=1-current,n=0;
        auto distance=[&](Point p){switch(side){
            case 0:return p.x-view.left;case 1:return view.right-p.x;
            case 2:return p.y-view.top;default:return view.bottom-p.y;}};
        for(int i=0;i<count;++i) {
            Point a=buffers[current][i],b=buffers[current][(i+1)%count];
            double da=distance(a),db=distance(b);
            if(da>=0)buffers[next][n++]=a;
            if((da>=0)!=(db>=0)) {
                double t=da/(da-db);buffers[next][n++]={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
            }
        }
        count=n;current=next;if(count<3)return;
    }
    auto* points=buffers[current];
    if(count==4){emit(Quad{points[0],points[1],points[2],points[3]});return;}
    for(int i=1;i+1<count;++i)emit(Quad{points[0],points[i],points[i+1],points[i+1]});
}
}
