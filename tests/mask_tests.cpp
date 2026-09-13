// Synthetic mask geometry, movement and session tests.
#include "ExplorationMask.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <tuple>
using namespace exploration;
void check(bool value,const char* what) { if(!value) { std::cerr<<what<<'\n'; std::exit(1); } }
int main() {
    Mask m(1); m.reveal(0,0); m.reveal(1,0);
    double perimeter=0; m.frontier([&](Point a,Point b){perimeter+=hypot(a.x-b.x,a.y-b.y);}); check(perimeter==6,"shared frontier");
    check(!m.contains({-0.1,0.5}),"negative coordinate floors");
    m.reveal(-1,0); check(m.contains({-0.1,0.5}),"negative reveal");
    Mask hole(1); for(int y=0;y<3;++y)for(int x=0;x<3;++x)if(x!=1||y!=1)hole.reveal(x,y);
    perimeter=0;hole.frontier([&](Point a,Point b){perimeter+=hypot(a.x-b.x,a.y-b.y);});check(perimeter==16,"inner hole frontier");
    auto identity=[](Point p){return p;};
    int counts[25]={};
    clipPrimitive(hole,{-1,-1,4,4},{0,0,3,3},identity,[&](Rect r){
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)++counts[y*5+x];
    });
    for(int y=0;y<5;++y)for(int x=0;x<5;++x)
        check(counts[y*5+x]==((x<3&&y<3&&(x!=1||y!=1))?1:0),"coverage / no overdraw / viewport");
    Mask empty;int calls=0;
    clipPrimitive(empty,{0,0,8,8},{0,0,8,8},identity,[&](Rect){++calls;});check(calls==0,"empty suppresses all");
    Session s;s.select(1,10).revealAround({0,0},1);
    check(s.select(1,10).size()==5,"fallback disk");
    check(s.select(1,20).size()==0,"level isolation");
    check(s.select(1,10).size()==5,"return to level");
    s.select(1,10).revealAround({1000,1000},0);
    check(!s.select(1,10).contains({500,500}),"no teleport corridor");
    check(s.select(2,10).size()==0,"new game reset");s.leave();
    check(s.select(2,10).size()==0,"leave reset");
    // Isometric scale + pan: compare emitted coverage with independent algebra.
    auto inverse=[](Point p){double a=(p.x-7)/2,b=p.y+3;return Point{(a+b)/2,(b-a)/2};};
    int actual[400]={};
    clipPrimitive(hole,{-5,-5,15,15},{-5,-5,15,15},inverse,[&](Rect r){
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)++actual[(y+5)*20+x+5];
    });
    for(int y=-5;y<15;++y)for(int x=-5;x<15;++x) {
        int wx=static_cast<int>(std::floor(((x+0.5-7)/2+y+0.5+3)/2));
        int wy=static_cast<int>(std::floor((y+0.5+3-(x+0.5-7)/2)/2));
        bool wanted=wx>=0&&wx<3&&wy>=0&&wy<3&&(wx!=1||wy!=1);
        check(actual[(y+5)*20+x+5]==int(wanted),"isometric footprint clipping");
    }
    // Compare compressed fine rows and cached edges against an independent grid.
    Mask fine(0.25);std::set<std::pair<int,int>> reference;
    unsigned random=137;
    for(int step=0;step<80;++step) {
        random=random*1664525u+1013904223u;int cx=int(random%81)-40;
        random=random*1664525u+1013904223u;int cy=int(random%81)-40;
        int radius=step%9;
        fine.revealAround({cx*0.25+0.1,cy*0.25+0.1},radius);
        for(int y=-radius;y<=radius;++y)for(int x=-radius;x<=radius;++x)
            if(x*x+y*y<=radius*radius)reference.insert({cx+x,cy+y});
        check(fine.size()==reference.size(),"row union size");
        using Edge=std::tuple<int,int,int>; // Direction, x, y of unit edge.
        std::set<Edge> expectedEdges,actualEdges;
        for(auto c:reference) {
            int x=c.first,y=c.second;
            check(fine.cell(x,y),"row union membership");
            if(!reference.count({x,y-1}))expectedEdges.insert({0,x,y});
            if(!reference.count({x,y+1}))expectedEdges.insert({0,x,y+1});
            if(!reference.count({x-1,y}))expectedEdges.insert({1,x,y});
            if(!reference.count({x+1,y}))expectedEdges.insert({1,x+1,y});
        }
        fine.frontier([&](Point a,Point b){
            int x1=int(lround(a.x*4)),y1=int(lround(a.y*4)),x2=int(lround(b.x*4)),y2=int(lround(b.y*4));
            if(y1==y2)for(int x=std::min(x1,x2);x<std::max(x1,x2);++x)
                check(actualEdges.insert({0,x,y1}).second,"horizontal edge drawn twice");
            else for(int y=std::min(y1,y2);y<std::max(y1,y2);++y)
                check(actualEdges.insert({1,x1,y}).second,"vertical edge drawn twice");
        });
        check(actualEdges==expectedEdges,"cached frontier matches exact fine grid");
    }
    for(int y=-50;y<=50;++y)for(int x=-50;x<=50;++x)
        check(fine.contains({x*0.25+0.125,y*0.25+0.125})==(reference.count({x,y})!=0),"fine pixel membership");
    Mask disk(0.25);disk.revealAround({0.1,0.1},80);
    check(disk.contains({20.1,0.1}) && !disk.contains({20.3,0.1}),"unchanged reveal radius at finer resolution");
    disk.revealAround({0.35,0.1},80);
    check(disk.contains({20.3,0.1}),"quarter-subtile movement advances frontier");
    int raster[256]={};
    clipPrimitive(fine,{-8,-8,8,8},{-8,-8,8,8},identity,[&](Rect r){
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)++raster[(y+8)*16+x+8];
    });
    for(int y=-8;y<8;++y)for(int x=-8;x<8;++x)
        check(raster[(y+8)*16+x+8]==int(reference.count({x*4+2,y*4+2})!=0),"fine terrain clip exact once");
    // Independent rational oracle for both zoom levels, negative coordinates,
    // panning and raster edges; no floating-point boundary-rounding ambiguity.
    auto floorDiv=[](int a,int b){return a/b-((a%b)<0);};
    for(int divisor:{10,20})for(int pan:{-9,0,11}) {
        int coverage[3600]={};
        fine.clipProjected({-30,-30,30,30},divisor,pan,-pan,[&](Rect r){
            for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)++coverage[(y+30)*60+x+30];
        });
        for(int y=-30;y<30;++y)for(int x=-30;x<30;++x) {
            int u=x+pan,v=y-pan;
            int cx=floorDiv(divisor*(2*u+4*v+3),16),cy=floorDiv(divisor*(-2*u+4*v+1),16);
            check(coverage[(y+30)*60+x+30]==int(reference.count({cx,cy})!=0),"cached raster equals independent integer oracle");
        }
    }
    std::cout<<"PASS: frontier, holes, clipping, session/teleport, compressed fine mask versus independent grid, cached edges, fractional movement\n";
}
