// Synthetic geometry tests cover floor shades, red open edges, wall outlines,
// viewport clipping, incremental/full-build equivalence, and worker snapshots.
// Optional development-fixture input is documented in ../docs/testing.md.

#include "StyledProjection.hpp"
#include "StyledWorker.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<"\n";std::exit(1);}}while(0)
static bool has(const styled_map::Rows& rows,int x,int y) {
    for(auto s:styled_map::rowAt(rows,y))if(x>=s.first && x<s.second)return true;
    return false;
}
template<class Drawing> static auto coverage(const Drawing& drawing) {
    std::array<styled_map::Rows,14> rows;
    for(std::size_t i=0;i<7;++i)for(int red:{0,1}) {
        for(const auto& q:red?drawing.layers[i].redQuads:drawing.layers[i].quads)
            for(int y=int(lround(q.a.y*4));y<int(lround(q.c.y*4));++y)
                rows[i*2+red][y].push_back({int(lround(q.a.x*4)),int(lround(q.c.x*4))});
        for(auto& entry:rows[i*2+red]) {
            auto& spans=entry.second;std::sort(spans.begin(),spans.end());
            for(std::size_t n=1;n<spans.size();++n)CHECK(spans[n-1].second<=spans[n].first);
            styled_map::normalize(spans);
        }
    }
    return rows;
}
template<class Drawing> static auto wallCoverage(const Drawing& drawing) {
    std::set<std::tuple<int,int,int>> result;
    for(auto w:drawing.walls) {
        int ax=int(lround(w.a.x*4)),ay=int(lround(w.a.y*4)),bx=int(lround(w.b.x*4)),by=int(lround(w.b.y*4));
        if(ax==bx)for(int y=std::min(ay,by);y<std::max(ay,by);++y)CHECK(result.emplace(0,ax,y).second);
        else for(int x=std::min(ax,bx);x<std::max(ax,bx);++x)CHECK(result.emplace(1,ay,x).second);
    }
    return result;
}
template<class A,class B> static void sameDrawing(const A& a,const B& b) {
    CHECK(coverage(a)==coverage(b));CHECK(wallCoverage(a)==wallCoverage(b));
}
static void testPreparedFloors() {
    styled_map::Drawing source;
    for(int i=0;i<300;++i) {
        double x=(i%20-10)*1.25,y=(i/20-7)*.75;
        auto& layer=source.layers[i%7];auto& quads=i%2?layer.redQuads:layer.quads;
        quads.push_back({{x,y},{x+1.75,y},{x+1.75,y+2.25},{x,y+2.25}});
        quads.push_back({{x+20000,y-11000},{x+20004,y-11002},{x+20007,y-10998},{x+20001,y-10999}});
    }
    auto prepared=styled_map::PreparedFloors::build(source);CHECK(prepared);
    for(int divisor:{10,20})for(double ox:{-22.5,0.,24.,49590.,-70000.})for(double oy:{-33.,7.25,7200.}) {
        auto project=[&](exploration::Point p){return exploration::Point{
            16*(p.x-p.y)/divisor-ox+(divisor==20?7:8),8*(p.x+p.y)/divisor-oy+(divisor==20?-3:-8)};};
        for(auto view:std::vector<exploration::Rect>{{-45,-35,40,55},{0,0,1,1},{-200,-200,800,600},{0,0,0,100}})
            for(std::size_t layer=0;layer<7;++layer)for(int red:{0,1}) {
                std::vector<double> expected,actual;
                auto collect=[](auto& out,const styled_map::Quad& q){for(auto p:{q.a,q.b,q.c,q.d}){out.push_back(p.x);out.push_back(p.y);}};
                for(const auto& q:red?source.layers[layer].redQuads:source.layers[layer].quads)
                    styled_map::clipQuad({project(q.a),project(q.b),project(q.c),project(q.d)},view,[&](const auto& p){collect(expected,p);});
                prepared->clip(layer,red!=0,divisor,ox,oy,view,[&](const auto& p){collect(actual,p);});
                CHECK(expected.size()==actual.size());
                for(std::size_t n=0;n<expected.size();++n)CHECK(fabs(expected[n]-actual[n])<1e-8);
            }
    }
    source=styled_map::Drawing{};source.layers[0].quads.resize(styled_map::PreparedFloors::quadLimit+1);
    CHECK(!styled_map::PreparedFloors::build(source)); // The runtime retains its uncached drawing path.
    std::cout<<"PASS: prepared floor projection matches reference vertices/order across colors, zooms, pans, distant coordinates, clipping and empty views; bounded fallback\n";
}
int main() {
    testPreparedFloors();
    styled_map::Drawing split;
    split.walls={{{0,0},{4,0}},{{8,0},{4,0}},{{9,0},{12,0}},
        {{-2,-8},{-2,-4}},{{-2,0},{-2,-4}},{{-2,2},{-2,5}},{{20,0},{20,3}}};
    auto expectedWalls=wallCoverage(split);styled_map::compactWalls(split.walls);
    CHECK(split.walls.size()==5 && wallCoverage(split)==expectedWalls);
    styled_map::compactWalls(split.walls);CHECK(split.walls.size()==5 && wallCoverage(split)==expectedWalls);
    const exploration::Rect view{0,0,10,10};
    const styled_map::Quad diamond{{5,-2},{12,5},{5,12},{-2,5}};
    double clippedArea=0;int clippedCount=0;
    styled_map::clipQuad(diamond,view,[&](const styled_map::Quad& q){
        const exploration::Point p[]={q.a,q.b,q.c,q.d};double area=0;
        for(int i=0;i<4;++i) {
            CHECK(p[i].x>=0 && p[i].x<=10 && p[i].y>=0 && p[i].y<=10);
            area+=p[i].x*p[(i+1)%4].y-p[(i+1)%4].x*p[i].y;
        }
        CHECK(area>0);clippedArea+=area*.5;++clippedCount;
    });
    CHECK(clippedCount==6 && fabs(clippedArea-82)<1e-8);
    int outside=0;
    styled_map::clipQuad(diamond,{20,20,30,30},[&](const styled_map::Quad&){++outside;});
    styled_map::clipQuad(diamond,{0,0,0,10},[&](const styled_map::Quad&){++outside;});
    CHECK(outside==0);
    int inside=0;const styled_map::Quad contained{{2,1},{4,3},{2,5},{0,3}};
    styled_map::clipQuad(contained,view,[&](const styled_map::Quad& q){CHECK(q.a.x==2 && q.a.y==1 && q.b.x==4 && q.c.y==5 && q.d.x==0);++inside;});
    CHECK(inside==1);
    exploration::Point a{-10,4.125},b{20,4.125};
    CHECK(styled_map::clipStroke(a,b,view));CHECK(a.x==0 && b.x==9 && a.y==4.125 && b.y==4.125);
    a={-10,10};b={20,10};CHECK(!styled_map::clipStroke(a,b,view));
    exploration::Mask mask(.25);
    for(int y=-12;y<=12;++y)for(int x=-12;x<=12;++x)if(x*x+y*y<=121 && !(x>2 && y>2 && x<7 && y<7))mask.reveal(x,y);
    for(int radius:{1,2,3,5}) {
        auto erosion=styled_map::erode(mask.rows(),radius);
        for(int y=-15;y<=15;++y)for(int x=-15;x<=15;++x) {
            bool expected=true;
            for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx)
                if(dx*dx+dy*dy<=radius*radius && !mask.cell(x+dx,y+dy))expected=false;
            CHECK(has(erosion,x,y)==expected);
        }
    }
    styled_map::Level floor;std::vector<std::uint16_t> flags(21*21,1);
    for(int y=2;y<19;++y)for(int x=2;x<19;++x)flags[y*21+x]=0;
    flags[10*21+10]=0x85c0; // Transient unit/object bits are not walls.
    flags[0]=0; // Isolated bogus floor must not appear.
    floor.ingest(-10,-10,21,21,flags);CHECK(floor.connect({0,0}));CHECK(floor.contains({0,0}) && !floor.contains({-10,-10}));
    auto draw=floor.build(mask);CHECK(draw.quads>0);
    for(int y=-45;y<=45;++y)for(int x=-45;x<=45;++x) {
        int hits=0;auto p=exploration::Point{(x+.5)*.25,(y+.5)*.25};
        for(const auto& layer:draw.layers)for(const auto* quads:{&layer.quads,&layer.redQuads})for(const auto& q:*quads)
            if(p.x>=q.a.x && p.x<q.c.x && p.y>=q.a.y && p.y<q.c.y)++hits;
        CHECK(hits==int(mask.cell(x,y) && floor.contains(p)));
    }
    // Unknown adjacent room data must not create an outline along a seam.
    styled_map::Level open;open.ingest(0,0,10,10,std::vector<std::uint16_t>(100,0));CHECK(open.connect({5,5}));
    exploration::Mask all(.25);all.revealAround({5,5},80);CHECK(open.build(all).walls.empty());
    open.ingest(10,0,10,10,std::vector<std::uint16_t>(100,0));CHECK(open.connect({5,5}));CHECK(open.contains({15,5}));
    // A sealed room's wall edge stays gray. Only the open half-room frontier
    // turns red; the floor interior keeps its normal shade, without overdraw.
    styled_map::Level room;std::vector<std::uint16_t> roomFlags(12*12,1);
    for(int y=1;y<11;++y)for(int x=1;x<11;++x)roomFlags[y*12+x]=0;
    room.ingest(-1,-1,12,12,roomFlags);CHECK(room.connect({5,5}));
    exploration::Mask halfRoom(.25),fullRoom(.25);
    for(int y=0;y<40;++y){halfRoom.revealRange(y,0,20);fullRoom.revealRange(y,0,40);}
    auto half=room.build(halfRoom),full=room.build(fullRoom);std::size_t redCount=0;
    for(const auto& layer:full.layers)CHECK(layer.redQuads.empty());
    for(const auto& layer:half.layers)for(const auto& q:layer.redQuads){CHECK(q.a.x>=1.75 && q.c.x<=5);++redCount;}
    CHECK(redCount>0 && half.layers.back().redQuads.empty());
    // A flood of replacements must converge to the latest complete snapshot,
    // retain every room and keep independent level/session results identifiable.
    {
        styled_map::Worker worker;styled_map::FloorCopies copies;
        copies.ingest(-10,-10,21,21,flags);
        for(int n=1;n<=60;++n) {
            auto request=std::make_unique<styled_map::BuildRequest>();
            request->session=n<30?1:2;request->level=203;request->maskSize=n;
            request->player={0,0};request->visible.spans=mask.rows();request->rooms=copies.rooms;
            worker.submit(std::move(request));
        }
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        bool received=false;
        while(std::chrono::steady_clock::now()<deadline) {
            if(auto result=worker.take())if(result->maskSize==60) {
                CHECK(result->session==2 && result->level==203 && result->success);
                CHECK(result->floorCells==floor.size());sameDrawing(result->drawing,draw);received=true;break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(received);
    }
    {
        styled_map::ChunkedMap cached;
        std::vector<std::uint16_t> cells(80*64,0);
        for(int y=0;y<64;++y)for(int x=0;x<80;++x)if(x==0 || x==79 || y==0 || y==63 || (x==40 && y%16!=8))cells[y*80+x]=1;
        cached.floor().ingest(-32,-32,80,64,cells);CHECK(cached.floor().connect({0,0}));
        exploration::Mask growing(.25);
        for(auto p:std::vector<exploration::Point>{{-16,-16},{-16.25,-16},{-8,-8},{0,0},{8,0},{16,0},{16,8},{32,16},{-24,16}}) {
            growing.revealAround(p,30);
            auto chunked=cached.build(growing);auto reference=cached.floor().build(growing);sameDrawing(chunked,reference);
        }
        auto unchanged=cached.build(growing);CHECK(cached.rebuiltChunks==0);sameDrawing(unchanged,cached.floor().build(growing));
        // Replacing the explored snapshot must remove old quads too.
        exploration::Mask reset(.25);reset.revealAround({0,0},20);sameDrawing(cached.build(reset),cached.floor().build(reset));
        cached.floor().ingest(48,-32,20,64,std::vector<std::uint16_t>(20*64,0));
        CHECK(cached.floor().connect({50,0}));growing.revealAround({50,0},60);
        sameDrawing(cached.build(growing),cached.floor().build(growing));
    }
    // Optional original development fixture; synthetic coverage above is mandatory.
    char* probeValue=nullptr;std::size_t probeLength=0;
    CHECK(_dupenv_s(&probeValue,&probeLength,"PD2_FLOOR_PROBE")==0);
    const std::string probePath=probeValue?probeValue:"";
    std::free(probeValue);
    if(!probePath.empty()) {
        std::ifstream input(probePath,std::ios::binary);CHECK(bool(input));
        styled_map::Level actual;std::uint32_t header[6];
        while(input.read(reinterpret_cast<char*>(header),sizeof(header))) {
            CHECK(header[0]==0x31524c46);std::vector<std::uint16_t> grid(std::size_t(header[4])*header[5]);
            CHECK(bool(input.read(reinterpret_cast<char*>(grid.data()),static_cast<std::streamsize>(grid.size()*2))));
            actual.ingest(int(header[2]),int(header[3]),int(header[4]),int(header[5]),grid);
        }
        CHECK(actual.connect({20070,10450}));
        exploration::Mask visible(.25);
        for(int x=20065;x<20280;x+=5)visible.revealAround({double(x),10470},80);
        auto start=std::chrono::steady_clock::now();auto geometry=actual.build(visible);
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout<<"Actual Endgame map: "<<actual.roomCount()<<" rooms, "<<actual.size()<<" connected floor cells, "<<geometry.quads<<" quads, "<<geometry.walls.size()<<" outlines, build "<<ms<<"ms\n";
    }
    std::cout<<"PASS: independent disk erosion, exact single-band floor coverage, clipped polygon area, red open frontiers versus gray sealed rooms, transient flags, isolated cells, room seams, incremental chunk/full geometry equivalence, negative-coordinate joins, teleports, reset/removal, unchanged-cache reuse, worker coalescing/session isolation and clean shutdown\n";
}
