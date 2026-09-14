// Viewport clipping for floor quads and fractional wall strokes.
#pragma once
#include "StyledMap.hpp"

namespace styled_map {
// Screen-space butt-ended stroke. Fractional vertices keep its width stable at
// both automap zooms; the renderer handles the resulting edge antialiasing.
inline bool strokeQuad(Point a,Point b,double width,Quad& out) {
    double dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
    if(!std::isfinite(length) || length<1e-6 || !std::isfinite(width) || width<=0)return false;
    const double nx=-dy*width/(2*length),ny=dx*width/(2*length);
    out={{a.x+nx,a.y+ny},{b.x+nx,b.y+ny},{b.x-nx,b.y-ny},{a.x-nx,a.y-ny}};return true;
}
// D2GL stores positions in binary16. Use a uniform representable grid for thin
// contours so crossing a precision boundary cannot change their thickness.
inline double strokePixelStep(exploration::Rect view) {
    double extent=std::max({std::abs(double(view.left)),std::abs(double(view.top)),
        std::abs(double(view.right)),std::abs(double(view.bottom))});
    double step=1;while(extent>2048){extent*=.5;step*=2;}return step;
}
inline bool stableStrokeQuad(Point a,Point b,double width,double pixelStep,Quad& out) {
    if(!std::isfinite(pixelStep) || pixelStep<=0 || !std::isfinite(width) || width<=0)return false;
    if(std::tie(b.x,b.y)<std::tie(a.x,a.y))std::swap(a,b);
    const double dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);
    if(!std::isfinite(length) || length<1e-6)return false;
    auto snap=[&](double v){return std::floor(v/pixelStep+.5)*pixelStep;};
    const double nx=-dy*width/length,ny=dx*width/length;
    double sx=snap(nx),sy=snap(ny);
    if(sx==0 && sy==0){if(std::abs(dx)>=std::abs(dy))sy=pixelStep;else sx=dy>0?-pixelStep:pixelStep;}
    const Point leftA{snap(a.x-nx*.5),snap(a.y-ny*.5)},leftB{snap(b.x-nx*.5),snap(b.y-ny*.5)};
    if(std::abs((leftB.x-leftA.x)*sy-(leftB.y-leftA.y)*sx)<1e-6)return false;
    out={leftA,leftB,{leftB.x+sx,leftB.y+sy},{leftA.x+sx,leftA.y+sy}};return true;
}
class WallStrokeCache {
    struct Entry {Point a{},b{};Quad casing{},core{};bool valid=false,ready=false;};
    std::vector<Entry> entries_;
    std::size_t cursor_=0,builds_=0;
    double step_=0;
    Point pan_{},phase_{};
    static Quad shifted(Quad q,Point p){
        for(auto* v:{&q.a,&q.b,&q.c,&q.d}){v->x+=p.x;v->y+=p.y;}return q;
    }
public:
    static constexpr std::size_t limit=16384;
    void begin(double step,Point pan) {
        const Point phase{pan.x-std::floor(pan.x/step)*step,pan.y-std::floor(pan.y/step)*step};
        if(step_!=step || phase.x!=phase_.x || phase.y!=phase_.y)entries_.clear();
        step_=step;phase_=phase;pan_=pan;cursor_=0;
    }
    // Sequential slots follow the existing wall order. Changing one wall does
    // not invalidate every other stroke, and screen pans need no rebuilds.
    bool query(Point a,Point b,Quad& casing,Quad& core) {
        Entry fallback{};Entry* entry=&fallback;
        if(cursor_<limit){if(cursor_==entries_.size())entries_.emplace_back();entry=&entries_[cursor_++];}
        if(!entry->ready || entry->a.x!=a.x || entry->a.y!=a.y || entry->b.x!=b.x || entry->b.y!=b.y) {
            ++builds_;entry->a=a;entry->b=b;entry->ready=true;
            a.x-=pan_.x;a.y-=pan_.y;b.x-=pan_.x;b.y-=pan_.y;
            entry->valid=stableStrokeQuad(a,b,2.5,step_,entry->casing) && stableStrokeQuad(a,b,1.,step_,entry->core);
            if(entry->valid){entry->casing=shifted(entry->casing,pan_);entry->core=shifted(entry->core,pan_);}
        }
        if(!entry->valid)return false;
        const Point offset{-pan_.x,-pan_.y};casing=shifted(entry->casing,offset);core=shifted(entry->core,offset);return true;
    }
    std::size_t builds() const{return builds_;}
    std::size_t size() const{return entries_.size();}
};
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
