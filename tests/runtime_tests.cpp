// Mocked game and renderer calls exercise native artwork clipping, texture
// coordinates, fractional lines, color restoration, and town bypasses.
// The optional local artwork check is described in ../docs/testing.md.

#include "ExplorationRuntime.cpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
static void require(bool ok){if(!ok)std::exit(1);}
static GlideVertex capturedA{},capturedB{};
static int floatCalls=0,pointCalls=0,forwardedCells=0;
static void __stdcall captureLine(const void* a,const void* b) {
    capturedA=*static_cast<const GlideVertex*>(a);capturedB=*static_cast<const GlideVertex*>(b);++floatCalls;
}
static void __stdcall capturePoint(const void*) {++pointCalls;}
static void __stdcall simulateNativeLine(int x1,int y1,int x2,int y2,DWORD color,DWORD alpha) {
    require(color==frontierColor && alpha==255);
    GlideVertex a{float(x1),float(y1),0,1,0,0,0},b{float(x2),float(y2),0,1,0,0,0};
    if(x1==x2 && y1==y2)floatPointHook(&a);else floatLineHook(&a,&b);
}
static void __stdcall captureCell(void*,int,int,NativeRect*,int) {++forwardedCells;}
static int quadCalls=0;
static GlideVertex testQuad[4]={{10,20,0xffffffff,1,64,32,0},{30,20,0xffffffff,1,128,32,0},
    {30,40,0xffffffff,1,128,96,0},{10,40,0xffffffff,1,64,96,0}};
static void __stdcall captureQuad(DWORD mode,DWORD count,const void* data,DWORD stride) {
    require(mode==5 && count==4 && stride==28);++quadCalls;
    const auto* v=static_cast<const GlideVertex*>(data);
    for(int i=0;i<4;++i) {
        require(v[i].x>=10 && v[i].x<=30 && v[i].y>=20 && v[i].y<=40);
        require(fabs(v[i].s-(64+(v[i].x-10)*3.2))<0.0001 && fabs(v[i].t-(32+(v[i].y-20)*3.2))<0.0001);
        require(v[i].color==0xffffffff && v[i].scale==1);
    }
}
static void __stdcall prepareAndDrawCell(void*,int,int,NativeRect*,int) {
    ++forwardedCells;quadHook(5,4,testQuad,sizeof(GlideVertex));
}
static void testContacts() {
    exploration::Silhouette shape;
    const unsigned char encoded[]={0x82,3,10,0,20,0x80,1,30,0x80};
    require(shape.decode(encoded,sizeof(encoded),6,2));
    require(shape.rows[0]==std::vector<exploration::Silhouette::Span>{{0,1}});
    require(shape.rows[1]==std::vector<exploration::Silhouette::Span>{{2,3},{4,5}});
    require(!shape.decode(encoded,sizeof(encoded)-1,6,2) && shape.rows.empty());
    const unsigned char badWidth[]={0x87,0x80},badRun[]={0,0x80},missingPixels[]={3,1};
    require(!shape.decode(badWidth,sizeof(badWidth),6,1));
    require(!shape.decode(badRun,sizeof(badRun),6,1));
    require(!shape.decode(missingPixels,sizeof(missingPixels),6,1));
    exploration::FrontierContacts contacts;contacts.begin({-50,-50,100,100});
    contacts.add({-20,5.5},{5,5.5});contacts.add({5,5.5},{20,5.5});
    contacts.add({-40,5.5},{-30,5.5});
    double length=0;
    auto measure=[&](Point a,Point b){require(a.x>=2 && b.x<=8 && a.y==5.5 && b.y==5.5);length+=hypot(a.x-b.x,a.y-b.y);};
    contacts.emit(measure);require(length==0); // Empty space has no line.
    shape.rows.resize(10);shape.rows[6]={{4,6}};
    contacts.touch(shape,{0,0,10,10});contacts.emit(measure);require(length==0); // Nearby is insufficient: must intersect.
    shape.rows[6].clear();shape.rows[5]={{4,6}};
    contacts.touch(shape,{0,0,10,10});contacts.touch(shape,{0,0,10,10});
    contacts.emit(measure);require(length==6); // Two-pixel ends, including the neighboring segment, no duplicate overdraw.
    contacts.begin({-50,-50,100,100});contacts.add({0,0},{10,10});
    shape.rows.assign(10,{});shape.rows[5]={{5,6}};
    contacts.touch(shape,{0,0,10,10});int count=0;
    contacts.emit([&](Point a,Point b){require(a.x==3 && a.y==3 && b.x==8 && b.y==8);++count;});require(count==1);
    contacts.begin({-10,-10,0,0});contacts.add({-20,-4.5},{20,-4.5});
    shape.rows.assign(2,{});shape.rows[1]={{1,2}};
    contacts.touch(shape,{-5,-6,-3,-4});count=0;
    contacts.emit([&](Point a,Point b){require(a.x==-6 && b.x==-1 && a.y==-4.5 && b.y==-4.5);++count;});require(count==1);
    contacts.begin({0,0,10,10});count=0;contacts.emit([&](Point,Point){++count;});require(count==0);
    // Exercise the guarded native metadata route, owned cache, and area reset.
    std::vector<DWORD> frame(16),file(7),ctx(14);
    const unsigned char pixel[]={0x80,0x80,0x80,0x80,0x84,1,29,0x80,0x80,0x80,0x80,0x80,0x80};
    frame[1]=10;frame[2]=10;frame[7]=sizeof(pixel);memcpy(frame.data()+8,pixel,sizeof(pixel));
    file[0]=6;file[5]=1;file[6]=reinterpret_cast<DWORD>(frame.data());ctx[13]=reinterpret_cast<DWORD>(file.data());
    contactFrontier.begin({0,0,100,100});contactFrontier.add({0,5.5},{20,5.5});
    auto decodedBefore=shapesDecoded;
    recordAssetContact(ctx.data(),{0,0,10,10});recordAssetContact(ctx.data(),{0,0,10,10});
    require(shapesDecoded==decodedBefore+1 && shapeFailures==0 && silhouetteCache.size()==1);
    count=0;contactFrontier.emit([&](Point a,Point b){require(a.x==2 && b.x==7);++count;});require(count==1);
    updateForPlayer({100,100,202,123,999},1000);require(silhouetteCache.empty());
    FrameSource invalid{};require(!frameSource(nullptr,&invalid));
}
static void testInstalledArtwork() {
    // Optional read-only integration check; no game assets are distributed.
    char* artworkValue=nullptr;std::size_t artworkLength=0;
    require(_dupenv_s(&artworkValue,&artworkLength,"PD2_AUTOMAP_ARTWORK_DIR")==0);
    const std::string artworkDirectory=artworkValue?artworkValue:"";
    std::free(artworkValue);
    if(artworkDirectory.empty()) {
        std::cout<<"SKIP: optional artwork integration (set PD2_AUTOMAP_ARTWORK_DIR)\n";
        return;
    }
    for(auto name:{"MaxiMap.dc6","MaxiMapS.dc6"}) {
        std::ifstream input(std::string(artworkDirectory)+"/"+name,std::ios::binary);
        require(bool(input));std::vector<unsigned char> data((std::istreambuf_iterator<char>(input)),{});
        auto word=[&](size_t offset){require(offset+4<=data.size());DWORD n;memcpy(&n,data.data()+offset,4);return n;};
        require(word(0)==6 && word(8)==0);DWORD total=word(16)*word(20);
        for(DWORD i=0;i<total;++i) {
            auto offset=word(24+4*i);DWORD size=word(offset+28);require(word(offset)==0 && offset+32+size<=data.size());
            exploration::Silhouette shape;require(shape.decode(data.data()+offset+32,size,int(word(offset+4)),int(word(offset+8))));
        }
        std::cout<<"PASS: "<<total<<" installed artwork silhouettes in "<<name<<"\n";
    }
}
static DWORD batchColor=0;
static unsigned arrayCalls=0,colorCalls=0;
static bool throwArray=false;
static const DWORD expectedStyledColors[]={0x34343446,0x55181446,0x747474af,0x60606096,0x41414173,0x26262650,0x17171737,0x0e0e0e28};
static void __stdcall captureColor(DWORD value){batchColor=value;++colorCalls;}
static void __stdcall captureArray(DWORD mode,DWORD count,const void* data) {
    require(mode==5 && count==4 && data && styledCurrent);
    require(batchColor==expectedStyledColors[arrayCalls%8]);
    const auto* pointers=static_cast<const void* const*>(data);
    for(DWORD i=0;i<count;++i){
        const auto& v=*static_cast<const GlideVertex*>(pointers[i]);
        require(v.x>=0 && v.x<=100 && v.y>=0 && v.y<=100 && v.scale==1 && v.color==0xffffffff);
    }
    if(throwArray)throw 1;
    ++arrayCalls;
}
static void __stdcall simulateStyledLine(int x1,int y1,int x2,int y2,DWORD color,DWORD alpha) {
    require(color==frontierColor);
    if(styledBatch)require(alpha==(expectedStyledColors[arrayCalls%8]&255));
    else require(alpha==255);
    GlideVertex a{float(x1),float(y1),0,1,0,0,0},b{float(x2),float(y2),0,1,0,0,0};
    floatLineHook(&a,&b);
    if(!fractionalFrontier)require(batchColor==(0x84848400u|alpha));
}
static void testStyledBatches() {
    StyledState state;
    for(auto& layer:state.drawing.layers)layer.quads.push_back({{0,0},{10,0},{10,10},{0,10}});
    state.drawing.layers[0].redQuads.push_back({{10,0},{20,0},{20,10},{10,10}});
    state.drawing.walls.push_back({{0,0},{10,0}});
    styledCurrent=&state;styledArray=captureArray;styledColor=captureColor;originalLine=simulateStyledLine;
    Transform t{10,-40,-20};
    drawStyled(t,{0,0,100,100});
    require(arrayCalls==8 && colorCalls==16 && !styledBatch && !frontierBatch && !fractionalFrontier);
    throwArray=true;
    try {drawStyled(t,{0,0,100,100});require(false);}catch(int){}
    require(!styledBatch && !frontierBatch && !fractionalFrontier);
    throwArray=false;
    // The whole replacement is emitted once per terrain pass. Later native
    // cells are suppressed; town cells still pass through even with stale data.
    std::vector<unsigned char> memory(0x11c210);
    client=memory.data();
    *reinterpret_cast<int*>(client+0xf16b0)=10;
    *reinterpret_cast<int*>(client+0x11c1f8)=-40;
    *reinterpret_cast<int*>(client+0x11c1fc)=-20;
    *reinterpret_cast<int*>(client+0xdbc48)=100;
    *reinterpret_cast<int*>(client+0xdbc4c)=100;
    NativeRect viewport{0,100,0,99};
    inPass=true;maskActive=true;styledActive=true;haveViewport=false;originalCell=captureCell;
    auto before=forwardedCells;
    cellHook(nullptr,0,0,&viewport,0);cellHook(nullptr,0,0,&viewport,0);
    require(arrayCalls==16 && forwardedCells==before && haveViewport);
    for(DWORD town:{1u,40u,75u,103u,109u}){
        updateForPlayer({5,5,town,999,77},1100);cellHook(nullptr,0,0,&viewport,0);
    }
    require(forwardedCells==before+5 && arrayCalls==16);
    // Missing renderer support selects the existing artwork path.
    styledArray=nullptr;maskActive=true;updateStyled({5,5,203,999,77});require(!styledActive);
    styledCurrent=nullptr;client=nullptr;
    std::cout<<"PASS: styled gray/red vertex batches, constant-color restoration, exception cleanup, once-per-pass submission, five town bypasses and renderer fallback\n";
}
int main() {
    testContacts();testInstalledArtwork();
    originalQuad=captureQuad;
    std::vector<Rect> strips;
    for(int y=20;y<40;y+=2)strips.push_back({12,y,28,y+1});
    originalCell=prepareAndDrawCell;
    drawPreparedCell(nullptr,0,0,nullptr,0,&strips);
    require(forwardedCells==1 && nativeCellCalls==1 && quadCalls==10 && !terrainClips);
    int pixels[400]={};
    require(clipQuad(testQuad,strips,[&](const GlideVertex* v){
        for(int y=int(v[0].y);y<int(v[2].y);++y)for(int x=int(v[0].x);x<int(v[2].x);++x)++pixels[(y-20)*20+x-10];
    }));
    for(int y=0;y<20;++y)for(int x=0;x<20;++x)require(pixels[y*20+x]==int(y%2==0 && x>=2 && x<18));
    for(int y=20;y<40;++y)for(int x=10;x<30;++x)
        require(clipQuad(testQuad,{{x,y,x+1,y+1}},[&](const GlideVertex* v){captureQuad(5,4,v,28);require(v[0].x==x && v[0].y==y && v[2].x==x+1 && v[2].y==y+1);}));
    bool same=false;require(clipQuad(testQuad,{{0,0,100,100}},[&](const GlideVertex* v){same=v==testQuad;}));require(same);
    int before=quadCalls;quadHook(5,4,testQuad,28);require(quadCalls==before+1);
    GlideVertex mirrored[4];memcpy(mirrored,testQuad,sizeof(testQuad));
    for(auto& v:mirrored)v.s=192-v.s;
    require(clipQuad(mirrored,{{15,25,25,35}},[&](const GlideVertex* v){require(v[0].s==112 && v[2].s==80 && v[0].t==48 && v[2].t==80);}));
    mirrored[0].x+=1;require(!clipQuad(mirrored,strips,[](const GlideVertex*){}));
    forwardedCells=0;
    Transform t{10,39,72};Point p{100,200};Point q=inverse(project(p,t),t);
    require(fabs(p.x-q.x)<1e-8&&fabs(p.y-q.y)<1e-8);
    originalFloatLine=captureLine;originalFloatPoint=capturePoint;originalLine=simulateNativeLine;
    drawFrontier({10.125,20.25},{10.375,20.375}); // Native route collapses to a point.
    require(floatCalls==1 && pointCalls==0 && capturedA.x==10.125f && capturedB.x==10.375f && capturedB.y==20.375f);
    require(!fractionalFrontier);
    drawFrontier({10.25,20.5},{40.125,45.75});
    require(floatCalls==2 && capturedA.y==20.5f && capturedB.x==40.125f);
    GlideVertex a{1,2,0,1,0,0,0},b{3,4,0,1,0,0,0};
    floatLineHook(&a,&b);floatPointHook(&a);
    require(floatCalls==3 && pointCalls==1 && capturedA.x==1 && capturedB.x==3);
    std::vector<std::pair<Point,Point>> batch={{{1.125,2.25},{4.5,5.75}},{{6.25,7.5},{8.75,9.125}}};
    frontierBatch=&batch;drawFrontier(batch[0].first,batch[0].second);frontierBatch=nullptr;
    require(floatCalls==5 && capturedA.x==6.25f && capturedB.y==9.125f && !fractionalFrontier);
    PlayerState state{100.125,100.125,202,999,77};
    updateForPlayer(state,1000);require(maskActive && explored->cellSize()==0.25);
    require(explored->contains({120.125,100.125}) && !explored->contains({120.375,100.125}));
    state.x+=0.25;updateForPlayer(state,1010);require(explored->contains({120.375,100.125}));
    auto wildernessSize=explored->size();
    originalCell=captureCell;inPass=true;
    for(DWORD level:{1u,40u,75u,103u,109u}) {
        state.level=level;updateForPlayer(state,1020);
        require(!maskActive && explored->size()==0);
        NativeRect viewport{0,100,0,100};cellHook(nullptr,0,0,&viewport,0);
    }
    require(forwardedCells==5);
    state.level=202;updateForPlayer(state,1030);
    require(maskActive && explored->size()==wildernessSize);
    state.level=203;state.x=1000;updateForPlayer(state,1040);
    require(maskActive && !explored->contains({100,100}));
    state.level=0;updateForPlayer(state,1050);require(!maskActive);
    std::cout<<"PASS: artwork-contact accents, transparency, padding across segments, duplicate union, malformed data, cache reset, one native preparation, exact-once coverage, UVs/flips, fractional lines, town bypass/persistence\n";
    testStyledBatches();
    return 0;
}
