// D2Client's Fade modes, evaluated in logical screen coordinates.
#pragma once
#include "StyledMap.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace exploration {
struct NativeFade {
    unsigned mode=0;
    bool idle=false;
    Point center{};
    unsigned alpha(Point p) const {
        if(mode==2)return 128;
        if(mode==3)return idle?192:255;
        if(mode!=1)return 255;
        const double x=std::abs(p.x-center.x),y=std::abs(p.y-center.y);
        if(x>140 || y>140)return 255;
        const double distance=std::max(x,y)+std::min(x,y)*.5;
        return distance<50?64:distance<100?128:distance<150?192:255;
    }
    static unsigned scale(unsigned alpha,unsigned fade) {return (alpha*fade+127)/255;}
    struct Plane {double x,y,limit;};
    struct Polygon {std::array<Point,40> points{};unsigned count=0;};
    static double distance(Plane plane,Point point) {return plane.limit-plane.x*point.x-plane.y*point.y;}
    static void split(const Polygon& source,Plane plane,Polygon& inside,Polygon& outside) {
        inside.count=outside.count=0;
        for(unsigned i=0;i<source.count;++i) {
            const Point a=source.points[i],b=source.points[(i+1)%source.count];
            const double da=distance(plane,a),db=distance(plane,b);
            (da>=0?inside:outside).points[(da>=0?inside:outside).count++]=a;
            if((da>=0)!=(db>=0)) {
                const double t=da/(da-db);const Point hit{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
                inside.points[inside.count++]=hit;outside.points[outside.count++]=hit;
            }
        }
    }
    template<class Emit> static void emitPolygon(const Polygon& polygon,unsigned fade,Emit emit) {
        if(polygon.count<3)return;
        double area=0;
        for(unsigned i=0;i<polygon.count;++i) {
            const auto a=polygon.points[i],b=polygon.points[(i+1)%polygon.count];area+=a.x*b.y-b.x*a.y;
        }
        if(std::abs(area)<1e-8)return;
        const auto& p=polygon.points;
        if(polygon.count==4){emit(styled_map::Quad{p[0],p[1],p[2],p[3]},fade);return;}
        for(unsigned i=1;i+1<polygon.count;++i) {
            const auto a=p[0],b=p[i],c=p[i+1];
            if(std::abs((b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y))>1e-8)
                emit(styled_map::Quad{a,b,c,c},fade);
        }
    }
    template<class Each> void planes(Each each) const {
        for(const auto p:std::array<Plane,4>{{{1,0,140},{-1,0,140},{0,1,140},{0,-1,140}}})each(p,255u);
        for(unsigned ring=0;ring<3;++ring)for(int x:{-1,1})for(int y:{-1,1}) {
            const double limit=300.0-100*ring;const unsigned outside=ring==0?255:ring==1?192:128;
            each(Plane{double(2*x),double(y),limit},outside);
            each(Plane{double(x),double(2*y),limit},outside);
        }
    }
    template<class Emit> void partition(styled_map::Quad quad,Emit emit) const {
        if(mode!=1){emit(quad,alpha({}));return;}
        const double left=std::min({quad.a.x,quad.b.x,quad.c.x,quad.d.x});
        const double right=std::max({quad.a.x,quad.b.x,quad.c.x,quad.d.x});
        const double top=std::min({quad.a.y,quad.b.y,quad.c.y,quad.d.y});
        const double bottom=std::max({quad.a.y,quad.b.y,quad.c.y,quad.d.y});
        if(right<=center.x-140 || left>=center.x+140 || bottom<=center.y-140 || top>=center.y+140) {
            emit(quad,255);return;
        }
        const double minX=std::max({left-center.x,center.x-right,0.0}),minY=std::max({top-center.y,center.y-bottom,0.0});
        const double maxX=std::max(std::abs(left-center.x),std::abs(right-center.x));
        const double maxY=std::max(std::abs(top-center.y),std::abs(bottom-center.y));
        const double nearDistance=std::max(minX,minY)+std::min(minX,minY)*.5;
        const double farDistance=std::max(maxX,maxY)+std::min(maxX,maxY)*.5;
        if(nearDistance>=150){emit(quad,255);return;}
        if(maxX<140 && maxY<140) {
            if(farDistance<50){emit(quad,64);return;}
            if(nearDistance>=50 && farDistance<100){emit(quad,128);return;}
            if(nearDistance>=100 && farDistance<150){emit(quad,192);return;}
        }
        Polygon remaining;remaining.count=4;remaining.points[0]=quad.a;remaining.points[1]=quad.b;
        remaining.points[2]=quad.c;remaining.points[3]=quad.d;
        for(unsigned i=0;i<4;++i){remaining.points[i].x-=center.x;remaining.points[i].y-=center.y;}
        auto output=[&](styled_map::Quad q,unsigned fade){
            for(auto p:{&q.a,&q.b,&q.c,&q.d}){p->x+=center.x;p->y+=center.y;}emit(q,fade);
        };
        planes([&](Plane plane,unsigned fade){
            if(remaining.count<3)return;
            Polygon inside,outside;split(remaining,plane,inside,outside);
            emitPolygon(outside,fade,output);remaining=inside;
        });
        emitPolygon(remaining,64,output);
    }
    template<class Emit> void line(Point a,Point b,Emit emit) const {
        if(mode!=1){emit(a,b,alpha({}));return;}
        std::array<double,30> cuts{};unsigned count=2;cuts[0]=0;cuts[1]=1;
        planes([&](Plane plane,unsigned){
            const double da=distance(plane,{a.x-center.x,a.y-center.y}),db=distance(plane,{b.x-center.x,b.y-center.y});
            if((da<0)!=(db<0))cuts[count++]=da/(da-db);
        });
        std::sort(cuts.begin(),cuts.begin()+count);
        auto at=[&](double t){return Point{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};};
        for(unsigned i=1;i<count;++i)if(cuts[i]-cuts[i-1]>1e-9)
            emit(at(cuts[i-1]),at(cuts[i]),alpha(at((cuts[i-1]+cuts[i])*.5)));
    }
};
}
