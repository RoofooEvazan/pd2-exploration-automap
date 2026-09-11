// Mocked game and renderer calls exercise native artwork clipping, texture
// coordinates, fractional lines, color restoration, and town bypasses.
// The optional local artwork check is described in ../docs/testing.md.

#include "ExplorationRuntime.cpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
static const char* checkContext="runtime";
static void require(bool ok){if(!ok){std::cerr<<"FAILED: "<<checkContext<<"\n";std::exit(1);}}
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
            if(i>=283 && i<=288) {
                using Form=exploration::HybridArtwork::SewerShape;
                auto form=i==283 || i==285?Form::Down:i==284 || i==286?Form::Up:i==287?Form::Peak:Form::Cap;
                exploration::NativeWallTrace trace;require(trace.buildSewer(shape,int(word(offset+4)),int(word(offset+8)),form));
                for(auto line:trace.lines)for(int sample=0;sample<=20;++sample) {
                    Point p{line.first.x+(line.second.x-line.first.x)*sample/20.0,line.first.y+(line.second.y-line.first.y)*sample/20.0};
                    double closest=1e9;
                    for(std::size_t y=0;y<shape.rows.size();++y)for(auto span:shape.rows[y])for(int x=span.first;x<span.second;++x)
                        closest=std::min(closest,hypot(p.x-x-.5,p.y-double(y)-.5));
                    if(closest>2.0)std::cout<<"TRACE SUPPORT FAILED "<<name<<" frame "<<i<<" distance "<<closest<<"\n";
                    require(closest<=2.0);
                }
            }
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
static void testTownProjection() {
    for(int divisor:{10,20})for(Rect quarter:std::vector<Rect>{{-13,-11,9,17},{0,0,1,1},{20000,40000,20800,40400}}) {
        exploration::ProjectedMask raster;
        raster.revealQuarterRect(quarter.left,quarter.top,quarter.right,quarter.bottom,divisor);
        int left=4*(quarter.left-quarter.top)/divisor-30,top=2*(quarter.left+quarter.top)/divisor-30;
        std::vector<int> coverage(10000);
        raster.clip(left,top,left+100,top+100,[&](int l,int y,int r,int b){
            require(b==y+1 && l>=left && r<=left+100 && y>=top && y<top+100);
            for(int x=l;x<r;++x)++coverage[(y-top)*100+x-left];
        });
        for(int y=top;y<top+100;++y)for(int x=left;x<left+100;++x) {
            double wx=(x+.5)*divisor/8+(y+.5)*divisor/4;
            double wy=(y+.5)*divisor/4-(x+.5)*divisor/8;
            require(coverage[(y-top)*100+x-left]==int(wx>=quarter.left && wx<quarter.right && wy>=quarter.top && wy<quarter.bottom));
        }
    }
}
static int townPixels[400]{},townArrayCalls=0;
static void __stdcall captureTownQuad(DWORD mode,DWORD count,const void* data,DWORD stride) {
    captureQuad(mode,count,data,stride);
    const auto* v=static_cast<const GlideVertex*>(data);
    for(int y=int(v[0].y);y<int(v[2].y);++y)for(int x=int(v[0].x);x<int(v[2].x);++x)++townPixels[(y-20)*20+x-10];
}
static void __stdcall captureTownArray(DWORD mode,DWORD count,const void* data) {
    require(mode==5 && count==4 && data);++townArrayCalls;
}
static void __stdcall townLine(int x1,int y1,int x2,int y2,DWORD,DWORD) {
    GlideVertex a{float(x1),float(y1),0,1,0,0,0},b{float(x2),float(y2),0,1,0,0,0};
    floatLineHook(&a,&b);
}
template<class T> static void put(std::vector<unsigned char>& data,size_t offset,T value) {
    require(offset+sizeof(T)<=data.size());memcpy(data.data()+offset,&value,sizeof(T));
}
static void testTownBoundary() {
    campaignStyle=MapStyle::Styled; // Keep the v0.1.1 town regression as well.
    std::istringstream townLayers("Id\tAct\tLayer\n109\t0\t5\n110\t0\t5\n111\t0\t5\n");
    require(campaignLayers.load(townLayers));
    testTownProjection();
    std::vector<unsigned char> memory(0x11c210),unit(0x40),path(0x40),act(0x50),townRoom(0x80),outRoom(0x80);
    std::vector<unsigned char> townRoom2(0x60),outRoom2(0x60),townLevel(0x1d4),outLevel(0x1d4),townGrid(0x24),outGrid(0x24);
    std::vector<std::uint16_t> townFlags(55*50),outFlags(20*20);
    DWORD layer=5;client=memory.data();
    put(memory,0x11bbfc,unit.data());put(memory,0x11c1c4,&layer);
    put(memory,0xf16b0,10);put(memory,0x11c1f8,36);put(memory,0x11c1fc,106);
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    put(unit,0x1c,act.data());put(unit,0x2c,path.data());put(unit,0xc,DWORD(777));
    put(path,0x1c,townRoom.data());put(act,0x10,townRoom.data());put(act,0xc,DWORD(444));
    put(townRoom,0x10,townRoom2.data());put(townRoom,0x20,townGrid.data());put(townRoom,0x7c,outRoom.data());
    put(outRoom,0x10,outRoom2.data());put(outRoom,0x20,outGrid.data());
    put(townRoom2,0x58,townLevel.data());put(outRoom2,0x58,outLevel.data());
    put(townLevel,0x1d0,DWORD(109));put(outLevel,0x1d0,DWORD(110));
    put(townLevel,0x1c,10);put(townLevel,0x20,10);put(townLevel,0x24,11);put(townLevel,0x28,10);
    put(townGrid,0,50);put(townGrid,4,50);put(townGrid,8,55);put(townGrid,12,50);put(townGrid,0x20,townFlags.data());
    put(outGrid,0,105);put(outGrid,4,65);put(outGrid,8,20);put(outGrid,12,20);put(outGrid,0x20,outFlags.data());
    floor_reader::AreaBounds checked;
    require(floor_reader::currentAreaBounds(client,&checked) && checked.x==50 && checked.width==55);
    put(townLevel,0x1c,50);require(!floor_reader::currentAreaBounds(client,&checked));put(townLevel,0x1c,10);
    styled_map::Worker worker;styledWorker=&worker;styledArray=captureTownArray;styledColor=captureColor;
    originalLine=townLine;originalQuad=captureTownQuad;originalCell=prepareAndDrawCell;
    enabled=true;townBoundary=TownBoundary{};
    PlayerState player{100.125,75.125,109,444,777,reinterpret_cast<uintptr_t>(act.data())};
    updateForPlayer(player,5000);
    auto outdoor=updateTownBoundary(player,5000);
    require(nativeTownActive && maskActive && outdoor.level==110 && townBoundary.outside==110);
    require(outdoor.x>=105 && outdoor.x<125 && outdoor.y>=65 && outdoor.y<85);
    auto* previewMask=explored;auto previewSize=explored->size();require(previewSize>0);
    const auto key=mapKey(outdoor);
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    do {
        updateStyled(outdoor,key);if(styledActive)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<deadline);
    require(styledActive && styledCurrent && styledCurrent->floorCells==400);
    player.level=110;player.x=105.125;
    updateForPlayer(player,5100);outdoor=updateTownBoundary(player,5100);updateStyled(outdoor,key);
    require(nativeTownActive && styledActive && explored==previewMask && explored->size()>=previewSize);
    player.level=109;player.x=100.125;
    updateForPlayer(player,5200);outdoor=updateTownBoundary(player,5200);
    require(nativeTownActive && explored==previewMask && outdoor.level==110);
    // While an outdoor build is pending, every native pixel must belong to town.
    std::vector<DWORD> frame(8),file(7),ctx(14);
    frame[1]=20;frame[2]=20;file[5]=1;file[6]=reinterpret_cast<DWORD>(frame.data());ctx[13]=reinterpret_cast<DWORD>(file.data());
    NativeRect viewport{0,100,0,99};Transform t{10,36,106};
    styledActive=false;haveViewport=false;inPass=true;memset(townPixels,0,sizeof(townPixels));
    cellHook(ctx.data(),10,40,&viewport,0);
    int inside=0,outside=0;
    for(int y=20;y<40;++y)for(int x=10;x<30;++x) {
        bool expected=inTownBounds(inverse({x+.5,y+.5},t));
        require(townPixels[(y-20)*20+x-10]==int(expected));expected?++inside:++outside;
    }
    require(inside>0 && outside>0);
    // Native fallback combines town and explored outdoor spans without overlap.
    Mask small(.25);small.revealAround({105.5,75.5},8);explored=&small;
    observedLevel=110;maskActive=true;haveViewport=false;memset(townPixels,0,sizeof(townPixels));
    cellHook(ctx.data(),10,40,&viewport,0);
    for(int y=20;y<40;++y)for(int x=10;x<30;++x) {
        auto world=inverse({x+.5,y+.5},t);
        require(townPixels[(y-20)*20+x-10]==int(inTownBounds(world)||small.contains(world)));
    }
    // A finished outdoor drawing is submitted once, with only town art forwarded.
    StyledState drawing;drawing.drawing.layers[0].quads.push_back({{105,75},{110,75},{110,80},{105,80}});
    drawing.drawing.quads=1;styledCurrent=&drawing;styledActive=true;haveViewport=false;
    townArrayCalls=0;cellHook(ctx.data(),10,40,&viewport,0);cellHook(ctx.data(),10,40,&viewport,0);
    require(townArrayCalls==1 && haveViewport);
    // Far from town, reject its viewport bounds before touching native frames.
    put(memory,0x11c1f8,10000);haveViewport=false;auto forwardedBefore=forwardedCells;
    cellHook(nullptr,10,40,&viewport,0);
    require(!townInViewport && enabled && forwardedCells==forwardedBefore);
    put(memory,0x11c1f8,36);
    player.level=111;updateForPlayer(player,5250);updateTownBoundary(player,5250);require(nativeTownActive);
    player.level=203;updateForPlayer(player,5275);updateTownBoundary(player,5275);require(!nativeTownActive);
    player.level=109;
    layer=6;updateForPlayer(player,5300);updateTownBoundary(player,5300);require(!nativeTownActive);
    layer=5;player.level=110;player.act+=16;updateForPlayer(player,5400);updateTownBoundary(player,5400);
    require(!nativeTownActive && !townBoundary.ready);
    campaignStyle=MapStyle::Native;
    styledWorker=nullptr;styledCurrent=nullptr;styledActive=false;nativeTownActive=false;inPass=false;client=nullptr;
    explored=&emptyMask;townBoundary=TownBoundary{};
    campaignLayers=exploration::CampaignLayers{};
    std::cout<<"PASS: town rectangle rasterization at both zooms, guarded level bounds, outdoor preview, crossing/reentry persistence, exact town-only coverage, duplicate-free fallback union, once-per-pass mixed drawing and layer/act isolation\n";
}
static void testCampaignArtwork() {
    require(parseStyle("NATIVE",MapStyle::Original)==MapStyle::Native);
    require(parseStyle("styled",MapStyle::Original)==MapStyle::Styled);
    require(parseStyle("original",MapStyle::Styled)==MapStyle::Original);
    require(parseStyle("invalid",MapStyle::Native)==MapStyle::Native);
    campaignStyle=MapStyle::Native;mapsStyle=MapStyle::Styled;
    require(styleForLevel(2)==MapStyle::Native && styleForLevel(132)==MapStyle::Native);
    require(styleForLevel(133)==MapStyle::Styled && styleForLevel(203)==MapStyle::Styled);
    std::vector<unsigned char> memory(0x11c210);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    std::vector<DWORD> frame(8),file(9),ctx(14);
    frame[1]=20;frame[2]=20;file[5]=3;
    for(int i=0;i<3;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    ctx[13]=reinterpret_cast<DWORD>(file.data());
    NativeRect viewport{0,100,0,99};StyledState drawing;
    styledCurrent=&drawing;styledArray=captureTownArray;styledColor=captureColor;
    originalLine=townLine;originalQuad=captureTownQuad;originalCell=prepareAndDrawCell;
    for(int divisor:{10,20}) {
        Transform t{double(divisor),36,106};
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,36);put(memory,0x11c1fc,106);
        const auto center=inverse({20.5,30.5},t);
        Mask mask(.25);mask.revealAround(center,16);explored=&mask;
        drawing.drawing=styled_map::Drawing{};
        drawing.drawing.layers[0].quads.push_back({inverse({18,28},t),inverse({22,28},t),inverse({22,32},t),inverse({18,32},t)});
        drawing.drawing.layers[0].redQuads=drawing.drawing.layers[0].quads;
        drawing.drawing.walls.push_back({center,{center.x+1,center.y+1}});
        drawing.drawing.quads=2;
        activeStyle=MapStyle::Native;styledActive=true;maskActive=true;inPass=true;enabled=true;nativeTownActive=false;haveViewport=false;
        const auto nativeBefore=nativeCellCalls,decodedBefore=shapesDecoded;
        const auto lineBefore=floatCalls;
        townArrayCalls=0;
        // Multiple artwork IDs all pass through, with exact mask clipping.
        for(DWORD id=0;id<3;++id) {
            ctx[0]=id;memset(townPixels,0,sizeof(townPixels));
            cellHook(ctx.data(),10,40,&viewport,0);
            int covered=0;
            for(int y=20;y<40;++y)for(int x=10;x<30;++x) {
                const bool expected=mask.contains(inverse({x+.5,y+.5},t));
                require(townPixels[(y-20)*20+x-10]==int(expected));covered+=expected;
            }
            require(covered>0 && covered<400);
        }
        require(townArrayCalls==2 && nativeCellCalls==nativeBefore+3 && shapesDecoded==decodedBefore && floatCalls==lineBefore);
        // Native campaign drawing at a gate unions full town and explored pixels.
        nativeTownActive=true;observedLevel=109;
        setTownBounds({center.x,center.y,109,1,1},5,{int(center.x)-6,int(center.y)-6,6,12});
        haveViewport=false;memset(townPixels,0,sizeof(townPixels));
        cellHook(ctx.data(),10,40,&viewport,0);
        for(int y=20;y<40;++y)for(int x=10;x<30;++x) {
            const auto world=inverse({x+.5,y+.5},t);
            require(townPixels[(y-20)*20+x-10]==int(mask.contains(world)||inTownBounds(world)));
        }
        // Original mode bypasses both shading and mask, including town gates.
        campaignStyle=MapStyle::Original;originalCell=captureCell;
        updateForPlayer({100,100,109,55,987},1000);
        auto player=updateTownBoundary({100,100,109,55,987},1000);
        require(player.level==109 && !nativeTownActive && !maskActive);
        auto before=forwardedCells;cellHook(nullptr,0,0,&viewport,0);require(forwardedCells==before+1);
        require(styleForLevel(203)==MapStyle::Styled);
        campaignStyle=MapStyle::Native;originalCell=prepareAndDrawCell;styledCurrent=&drawing;
    }
    styledCurrent=nullptr;styledActive=false;nativeTownActive=false;inPass=false;client=nullptr;explored=&emptyMask;
    townBoundary=TownBoundary{};activeStyle=MapStyle::Styled;
    std::cout<<"PASS: independent style selection, native artwork IDs, both zooms, exact explored/town coverage, one shading pass, no duplicate walls or added marker draws, no silhouette decoding and original-mode bypass\n";
}
static void testSessionLifetime() {
    using exploration::SessionChange;
    exploration::MenuObserver observer;
    require(!observer.sample(true,false));
    require(!observer.sample(false,false)); // Loading with temporarily absent unit.
    require(!observer.sample(true,true));   // In-game controls, e.g. a popup.
    require(observer.sample(false,true));
    require(!observer.sample(false,true)); // One epoch per menu visit.
    require(!observer.sample(true,false));
    require(observer.sample(false,true));
    exploration::SessionIdentity identity;
    require(identity.observe(7,0,111,1)==SessionChange::FirstPlayer);
    require(identity.observe(7,0,111,1)==SessionChange::None);
    require(identity.observe(7,4,222,1)==SessionChange::None);
    require(identity.observe(7,0,111,1)==SessionChange::None);
    require(identity.observe(7,0,111,2)==SessionChange::MenuReturn); // Same player/map seed on a new offline game.
    require(identity.observe(8,0,111,2)==SessionChange::PlayerChanged);
    require(identity.observe(8,0,333,2)==SessionChange::SeedChanged);
    require(identity.observe(9,5,333,2)==SessionChange::None); // Reject invalid native act data.
    require(identity.observe(8,0,333,2)==SessionChange::None);
    sessionIdentity=exploration::SessionIdentity{};InterlockedExchange(&menuEpoch,0);
    campaignStyle=MapStyle::Native;enabled=true;
    PlayerState player{100,100,2,111,7,0,0};
    updateForPlayer(player,100);const auto serial=gameSerial;
    player.x=200;updateForPlayer(player,15000);
    require(gameSerial==serial && explored->contains({100,100}) && explored->contains({200,100}));
    player.level=109;player.actNumber=4;player.seed=222;updateForPlayer(player,45000);
    player.level=2;player.actNumber=0;player.seed=111;updateForPlayer(player,95000);
    require(gameSerial==serial && explored->contains({100,100}));
    client=nullptr;update(); // Failed player/room read retains session and mask.
    updateForPlayer(player,195000);require(gameSerial==serial && explored->contains({100,100}));
    InterlockedIncrement(&menuEpoch);updateForPlayer(player,196000);
    require(gameSerial==serial+1 && !explored->contains({100,100}) && explored->contains({200,100}));
    player.seed=444;updateForPlayer(player,197000);require(gameSerial==serial+2);
    std::cout<<"PASS: long map gaps, transient reads, cross-act travel, menu epochs, same-seed new games, player/seed changes and explicit reset reasons\n";
}
static bool drawingCovers(const styled_map::Drawing& drawing,Point point) {
    for(const auto& layer:drawing.layers)for(const auto* quads:{&layer.quads,&layer.redQuads})
        for(const auto& q:*quads)if(point.x>=q.a.x && point.x<q.c.x && point.y>=q.a.y && point.y<q.c.y)return true;
    return false;
}
static void testJoinedCampaignAreas() {
    std::istringstream layers("Id\tAct\tLayer\n76\t2\t57\n78\t2\t57\n92\t2\t66\n");
    require(campaignLayers.load(layers));
    std::vector<unsigned char> memory(0x11c210),unit(0x40),act(0x50),roomA(0x80),roomB(0x80);
    std::vector<unsigned char> room2A(0x60),room2B(0x60),levelA(0x1d4),levelB(0x1d4),gridA(0x24),gridB(0x24);
    std::vector<std::uint16_t> flagsA(400),flagsB(400);
    DWORD layer=57;client=memory.data();
    put(memory,0x11bbfc,unit.data());put(memory,0x11c1c4,&layer);put(unit,0x1c,act.data());
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    put(memory,0xf16b0,10);put(memory,0x11c1f8,-20);put(memory,0x11c1fc,134);
    put(act,0x10,roomA.data());
    put(roomA,0x10,room2A.data());put(roomA,0x20,gridA.data());put(room2A,0x58,levelA.data());
    put(roomB,0x10,room2B.data());put(roomB,0x20,gridB.data());put(room2B,0x58,levelB.data());
    put(levelA,0x1d0,DWORD(76));put(levelB,0x1d0,DWORD(78));
    put(gridA,0,100);put(gridA,4,100);put(gridA,8,20);put(gridA,12,20);put(gridA,0x20,flagsA.data());
    put(gridB,0,120);put(gridB,4,100);put(gridB,8,20);put(gridB,12,20);put(gridB,0x20,flagsB.data());
    styled_map::Worker worker;styledWorker=&worker;styledArray=captureTownArray;styledColor=captureColor;
    campaignStyle=MapStyle::Native;mapsStyle=MapStyle::Styled;enabled=true;
    sessionIdentity=exploration::SessionIdentity{};InterlockedExchange(&menuEpoch,0);
    PlayerState player{105,110,76,123,456,reinterpret_cast<uintptr_t>(act.data()),2};
    updateForPlayer(player,200000);
    const auto key=lastLevelKey;auto* joinedMask=explored;
    auto finish=[&](std::size_t cells) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        do {
            updateStyled(player);
            if(styledActive && styledCurrent->floorCells==cells && drawingCovers(styledCurrent->drawing,{player.x,player.y}))return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }while(std::chrono::steady_clock::now()<deadline);
        require(false);
    };
    finish(400);auto* joinedDrawing=styledCurrent;
    require(drawingCovers(styledCurrent->drawing,{105,110}));
    // The first area's native room is no longer loaded. Its owned capture and
    // explored geometry must survive crossing to another area on this layer.
    put(act,0x10,roomB.data());player.level=78;player.x=135;
    updateForPlayer(player,201000);
    require(lastLevelKey==key && explored==joinedMask && explored->contains({105,110}));
    finish(800);
    require(styledCurrent==joinedDrawing && styledCurrent->floor.rooms.size()==2);
    require(drawingCovers(styledCurrent->drawing,{105,110}) && drawingCovers(styledCurrent->drawing,{135,110}));
    Mask currentOnly(.25);currentOnly.revealAround({135,110},80);require(!currentOnly.contains({105,110}));
    // Previously explored native details still pass through the joined mask.
    std::vector<DWORD> frame(8),file(7),ctx(14);frame[1]=20;frame[2]=20;file[5]=1;
    file[6]=reinterpret_cast<DWORD>(frame.data());ctx[13]=reinterpret_cast<DWORD>(file.data());
    NativeRect viewport{0,100,0,99};Transform t{10,-20,134};
    originalCell=prepareAndDrawCell;originalQuad=captureTownQuad;
    haveViewport=true;inPass=true;nativeTownActive=false;memset(townPixels,0,sizeof(townPixels));
    auto before=forwardedCells;cellHook(ctx.data(),10,40,&viewport,0);require(forwardedCells==before+1);
    for(int y=20;y<40;++y)for(int x=10;x<30;++x)
        require(townPixels[(y-20)*20+x-10]==int(explored->contains(inverse({x+.5,y+.5},t))));
    // Different native layers, acts and endgame level IDs must not merge.
    // Live transitions can report the new area with the old automap layer for
    // one callback. That callback must not contaminate either map's history.
    player.level=92;player.x=500;auto previousSize=joinedMask->size();
    require(!updateForPlayer(player,201500) && lastLevelKey==key && joinedMask->size()==previousSize);
    require(!joinedMask->contains({500,110}) && !maskActive && !styledActive);
    player.x=135;layer=66;require(updateForPlayer(player,202000));
    require(lastLevelKey!=key && !explored->contains({105,110}));
    layer=57;player.level=78;updateForPlayer(player,203000);require(lastLevelKey==key && explored==joinedMask);
    player.actNumber=1;require(mapKey(player)!=key);player.actNumber=2;
    player.level=204;auto map204=mapKey(player);player.level=205;require(mapKey(player)!=map204 && mapKey(player)!=key);
    player.level=78;layer=0xffffffffu;require(mapKey(player)==((std::uint64_t(player.seed)<<32)|78));
    layer=57;player.level=76;player.x=105;updateForPlayer(player,204000);require(lastLevelKey==key && explored->contains({135,110}));
    // A new game still clears the whole shared layer, including the old area.
    player.level=78;player.x=135;InterlockedIncrement(&menuEpoch);updateForPlayer(player,205000);
    require(!explored->contains({105,110}));
    styledWorker=nullptr;styledCurrent=nullptr;styledActive=false;nativeTownActive=false;inPass=false;
    client=nullptr;explored=&emptyMask;townBoundary=TownBoundary{};activeStyle=MapStyle::Styled;
    campaignLayers=exploration::CampaignLayers{};
    std::cout<<"PASS: adjoining campaign areas retain joined masks, old room captures, styled geometry and native details after room unload; stale native layer rejected, layer/act/endgame isolation, return trips, invalid layer fallback and new-game reset\n";
}
static std::string artworkFixture() {
    return "LevelName\tTileName\tStyle\tStartSequence\tEndSequence\tType1\tCel1\tType2\tCel2\tType2\tCel3\tType4\tCel4\n"
        "Example\twl\t0\t0\t0\tWR A\t10\tFloor Path A\t11\tRiver M A\t12\tWaypoint\t13\n"
        "Example\twl\t0\t0\t0\tWR A\t14\tWall Unknown\t14\tWL A\t15\tWRE 1\t16\n"
        "3 Sewer\twr\t45\t-1\t-1\t3Sewer WR\t283\t3Sewer WL\t284\t3Sewer WBL\t285\t3Sewer WTR\t286\n"
        "3 Sewer\twr\t45\t-1\t-1\t3Sewer WTLL\t287\t3Sewer WBR\t288\t3Sewer Drain\t289\t3Sewer Bridge\t290\n";
}
static void testHybridClassification() {
    using A=exploration::HybridArtwork;
    for(const char* label:{"WR A","Tmb_WL A","Cave Ltop","baal low rw 1 0-3"})require(A::describe(label)==A::Role::Wall);
    for(const char* label:{"Waypoint","Floor Path A","TomeWall_utlr80","FTwr_wl15_0","Tmb_STR R1","3S exit wr01",
        "baal wtll tran","pens wl","Cave M RWS D","CDwn_0","Crypt_11","baal wre","unrecognized"})require(A::describe(label)==A::Role::Detail);
    for(const char* label:{"3Sewer WR","3Sewer WL","3Sewer WBL","3Sewer WTR","3Sewer WTLL","3Sewer WBR"})
        require(A::describe(label)==A::Role::SewerWall);
    for(const char* label:{"River B A","pool fl 7 3","D_Oasis_a","Water_"})require(A::describe(label)==A::Role::Water);
    std::istringstream table(artworkFixture());require(hybridArtwork.load(table));
    require(hybridArtwork.walls()==2 && hybridArtwork.role(10)==A::Role::Wall);
    require(hybridArtwork.role(11)==A::Role::Detail && hybridArtwork.role(12)==A::Role::Water);
    require(hybridArtwork.role(14)==A::Role::Detail && hybridArtwork.role(65536)==A::Role::Detail);
    for(DWORD id:{283u,284u,285u,286u,287u,288u})require(hybridArtwork.role(id)==A::Role::SewerWall);
    require(hybridArtwork.role(289)==A::Role::SewerWater && hybridArtwork.role(290)==A::Role::Detail);
    require(hybridArtwork.sewerShape(283)==A::SewerShape::Down && hybridArtwork.sewerShape(284)==A::SewerShape::Up);
    A conflict;std::istringstream conflictTable(artworkFixture()),conflictObjects("Name\tAutoMap\nIcon\t283\nWater icon\t289\n");
    require(conflict.load(conflictTable) && conflict.protectObjects(conflictObjects) && conflict.role(283)==A::Role::Detail && conflict.role(289)==A::Role::Detail);
    std::istringstream objects("Name\tAutoMap\nImportant object\t15\nEmpty\t0\n");
    require(hybridArtwork.protectObjects(objects) && hybridArtwork.walls()==1 && hybridArtwork.role(15)==A::Role::Detail);
    A malformed;std::istringstream bad("Invalid header\n");require(!malformed.load(bad) && malformed.role(10)==A::Role::Detail);
    std::istringstream broken("Name\tMissingColumn\n");require(!malformed.protectObjects(broken));
    DWORD index=0;require(!frameIndex(nullptr,&index) && !frameIndex(reinterpret_cast<void*>(1),&index));
    require(parseStyle("hybrid",MapStyle::Native)==MapStyle::Hybrid);
}
static int casingCalls=0,coreCalls=0;static Transform hybridTransform{};
static int sewerShadeCalls=0;
static void __stdcall captureSewerShade(DWORD mode,DWORD count,const void* data) {
    require(mode==5 && count==4 && data && batchColor==0x34343446);++sewerShadeCalls;
}
static void __stdcall captureHybridArray(DWORD mode,DWORD count,const void* data) {
    require(mode==5 && count>0 && count%4==0 && data);
    require(batchColor==0x181818c0 || batchColor==0x949494e0);
    if(batchColor==0x181818c0)++casingCalls;else ++coreCalls;
    auto vertices=static_cast<const GlideVertex* const*>(data);
    for(DWORD i=0;i<count;i+=4) {
        Point center{};
        for(DWORD j=0;j<4;++j) {
            auto v=vertices[i+j];require(v->x>=0 && v->x<=100 && v->y>=0 && v->y<=100);
            center.x+=v->x*.25;center.y+=v->y*.25;
        }
        // Clipping uses the same pixel-center raster as native artwork.
        Point pixelCenter{floor(center.x)+.5,floor(center.y)+.5};
        require(explored->contains(inverse(pixelCenter,hybridTransform)));
    }
}
static void testHybridRendering() {
    testHybridClassification();
    styled_map::Quad quad;
    require(!styled_map::strokeQuad({0,0},{0,0},2.5,quad));
    require(styled_map::strokeQuad({0,0},{10,0},2.5,quad));
    require(quad.a.y==1.25 && quad.b.y==1.25 && quad.c.y==-1.25 && quad.d.y==-1.25);
    require(!styled_map::strokeQuad({0,0},{10,0},0,quad));
    std::vector<unsigned char> memory(0x11c210);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    std::vector<DWORD> frame(8),file(306),ctx(14);frame[1]=20;frame[2]=20;file[5]=300;
    for(int i=0;i<300;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    ctx[13]=reinterpret_cast<DWORD>(file.data());NativeRect viewport{0,100,0,99};
    StyledState drawing;styledCurrent=&drawing;styledArray=captureHybridArray;styledColor=captureColor;
    originalLine=townLine;originalQuad=captureTownQuad;originalCell=prepareAndDrawCell;
    for(int divisor:{10,20}) {
        hybridTransform={double(divisor),36,106};auto t=hybridTransform;
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,36);put(memory,0x11c1fc,106);
        Mask mask(.25);mask.revealAround(inverse({20.5,30.5},t),16);explored=&mask;
        drawing.drawing=styled_map::Drawing{};drawing.drawing.quads=1;
        drawing.drawing.walls.push_back({inverse({14,30},t),inverse({27,30},t)});
        activeStyle=MapStyle::Hybrid;styledActive=true;maskActive=true;inPass=true;enabled=true;nativeTownActive=false;haveViewport=false;
        observedLevel=2;
        casingCalls=coreCalls=0;ctx[0]=10;auto before=forwardedCells;
        cellHook(ctx.data(),10,40,&viewport,0);
        require(forwardedCells==before && casingCalls==1 && coreCalls==1 && !styledBatch && !fractionalFrontier);
        // Roads, water, icons and unknown art retain the exact native mask route.
        for(DWORD id:{11u,12u,13u,99u,283u,284u,285u,286u,287u,288u}) {
            ctx[0]=id;memset(townPixels,0,sizeof(townPixels));
            cellHook(ctx.data(),10,40,&viewport,0);
            for(int y=20;y<40;++y)for(int x=10;x<30;++x)
                require(townPixels[(y-20)*20+x-10]==int(mask.contains(inverse({x+.5,y+.5},t))));
        }
        require(forwardedCells==before+10 && casingCalls==1 && coreCalls==1);
        // Sewers retain one native wall set and the modern floor shading,
        // without a second offset contour alongside the original artwork.
        drawing.drawing.layers[0].quads.push_back({inverse({18,28},t),inverse({22,28},t),inverse({22,32},t),inverse({18,32},t)});
        styledArray=captureSewerShade;sewerShadeCalls=0;
        for(DWORD sewer:{92u,93u}) {
            observedLevel=sewer;haveViewport=false;ctx[0]=283;auto cellsBefore=forwardedCells;
            memset(townPixels,0,sizeof(townPixels));cellHook(ctx.data(),10,40,&viewport,0);
            require(casingCalls==1 && coreCalls==1 && forwardedCells==cellsBefore+1);
            for(int y=20;y<40;++y)for(int x=10;x<30;++x)
                require(townPixels[(y-20)*20+x-10]==int(mask.contains(inverse({x+.5,y+.5},t))));
        }
        require(sewerShadeCalls==2);drawing.drawing.layers[0].quads.clear();styledArray=captureHybridArray;observedLevel=2;
        // Town portions of ordinary wall sprites still use their native art.
        auto center=inverse({20.5,30.5},t);
        setTownBounds({center.x,center.y,109,1,1},5,{int(center.x)-6,int(center.y)-6,6,12});
        nativeTownActive=true;haveViewport=false;ctx[0]=10;memset(townPixels,0,sizeof(townPixels));
        cellHook(ctx.data(),10,40,&viewport,0);
        for(int y=20;y<40;++y)for(int x=10;x<30;++x)
            require(townPixels[(y-20)*20+x-10]==int(inTownBounds(inverse({x+.5,y+.5},t))));
        // A loaded floor result can have no contour for an exposed sewer wall.
        // Keep the native wall even though styled mode is fully active.
        drawing.drawing.walls.clear();nativeTownActive=false;haveViewport=false;
        ctx[0]=283;before=forwardedCells;memset(townPixels,0,sizeof(townPixels));
        cellHook(ctx.data(),10,40,&viewport,0);require(forwardedCells==before+1);
        for(int y=20;y<40;++y)for(int x=10;x<30;++x)
            require(townPixels[(y-20)*20+x-10]==int(mask.contains(inverse({x+.5,y+.5},t))));
        // When replacement geometry is unavailable, ordinary wall art returns.
        styledActive=false;nativeTownActive=false;haveViewport=false;before=forwardedCells;ctx[0]=10;
        cellHook(ctx.data(),10,40,&viewport,0);require(forwardedCells==before+1);
    }
    client=nullptr;styledCurrent=nullptr;styledActive=false;nativeTownActive=false;inPass=false;explored=&emptyMask;
    townBoundary=TownBoundary{};activeStyle=MapStyle::Styled;
    std::cout<<"PASS: conservative hybrid classification, object conflicts, malformed definitions, guarded frame IDs, fractional contour widths, clipped casing/core, preserved roads/water/icons and sewer walls without matching or duplicate contours, retained sewer shading, town coverage and native fallback\n";
}
static void testNativeWallTrace() {
    checkContext="trace shape extraction";
    exploration::Silhouette silhouette;exploration::NativeWallTrace trace;
    silhouette.rows.resize(10);for(int i=0;i<10;++i)silhouette.rows[i]={{i,i+1}};
    require(trace.build(silhouette,10,10) && trace.lines.size()==1);
    require(trace.lines[0].first.x==0 && trace.lines[0].first.y==0 && trace.lines[0].second.x==10 && trace.lines[0].second.y==10);
    silhouette.rows.assign(1,{{0,2},{8,10}});require(trace.build(silhouette,10,1) && trace.lines.size()==2);
    require(trace.lines[0].second.x==2 && trace.lines[1].first.x==8); // Never join an empty gap.
    silhouette.rows.assign(10,{{4,5}});require(trace.build(silhouette,10,10) && trace.lines.size()==1);
    require(trace.lines[0].first.x==4.5 && trace.lines[0].second.x==4.5);
    silhouette.rows.assign(10,{});require(!trace.build(silhouette,10,10));
    silhouette.rows[0]={{-1,2}};require(!trace.build(silhouette,10,10) && trace.lines.empty());
    using Form=exploration::HybridArtwork::SewerShape;
    for(int w:{8,16}) {
        silhouette.rows.assign(w*2,{{0,w}});
        require(trace.buildSewer(silhouette,w,w*2,Form::Down) && trace.lines.size()==1);
        auto down=trace.lines[0];require(down.second.x-down.first.x==w*.5 && down.second.y-down.first.y==w*.25);
        require(down.second.x==down.first.x+w*.5 && down.second.y==down.first.y+w*.25); // Exact neighboring-tile join.
        require(trace.buildSewer(silhouette,w,w*2,Form::Up) && trace.lines.size()==1);
        auto up=trace.lines[0];require(up.second.x-up.first.x==w*.5 && up.second.y-up.first.y==-w*.25);
        require(trace.buildSewer(silhouette,w,w*2,Form::Peak) && trace.lines.size()==2);
        require(trace.lines[0].second.x==trace.lines[1].first.x && trace.lines[0].second.y==trace.lines[1].first.y);
    }

    testHybridClassification();
    std::vector<unsigned char> memory(0x11c210);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    std::vector<DWORD> frame(256),file(306),ctx(14);
    file[0]=6;file[5]=300;for(int i=0;i<300;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    ctx[0]=283;ctx[13]=reinterpret_cast<DWORD>(file.data());NativeRect viewport{0,100,0,99};
    StyledState drawing;drawing.drawing.quads=1;styledCurrent=&drawing;
    styledArray=captureHybridArray;styledColor=captureColor;originalLine=townLine;originalCell=captureCell;
    sewerWallTraces.clear();
    for(int divisor:{10,20}) {
        checkContext="trace queue and cache";
        const int w=divisor==20?8:16,h=w*2,drawX=18-w/2,drawY=29+w/2;
        std::vector<unsigned char> encoded;
        for(int y=h-1;y>=0;--y) {
            int first=-1,count=0;for(int x=w/2;x<w;++x)if(y==h-w/2+(x-w/2)/2){if(first<0)first=x;++count;}
            if(count){if(first)encoded.push_back(static_cast<unsigned char>(128+first));encoded.push_back(static_cast<unsigned char>(count));
                for(int i=0;i<count;++i)encoded.push_back(29);}
            encoded.push_back(128);
        }
        frame[1]=w;frame[2]=h;frame[7]=DWORD(encoded.size());memcpy(frame.data()+8,encoded.data(),encoded.size());
        hybridTransform={double(divisor),36,106};auto t=hybridTransform;
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,36);put(memory,0x11c1fc,106);
        Mask mask(.25);mask.revealAround(inverse({20.5,30.5},t),16);explored=&mask;
        activeStyle=MapStyle::Hybrid;observedLevel=92;styledActive=true;maskActive=true;inPass=true;enabled=true;nativeTownActive=false;haveViewport=false;
        sewerCasing.clear();sewerCore.clear();casingCalls=coreCalls=0;auto before=forwardedCells;
        cellHook(ctx.data(),drawX,drawY,&viewport,0);cellHook(ctx.data(),drawX+2,drawY,&viewport,0);
        require(forwardedCells==before && !sewerCore.empty() && !sewerCasing.empty() && casingCalls==0 && coreCalls==0);
        require(sewerWallTraces.size()==std::size_t(divisor==10?1:2));checkContext="trace batch clipping and submission";endPass();
        require(casingCalls==1 && coreCalls==1 && sewerCore.empty() && sewerCasing.empty() && !styledBatch);
        endPass();require(casingCalls==1 && coreCalls==1); // No repeated submission.
        // An unsupported texture or exhausted batch budget keeps the wall native.
        checkContext="trace unsupported artwork fallback";
        inPass=true;file[0]=0;before=forwardedCells;
        cellHook(ctx.data(),drawX,drawY,&viewport,0);require(forwardedCells==before+1 && sewerCore.empty());file[0]=6;
        checkContext="trace budget fallback";sewerCasing.resize(sewerVertexLimit);before=forwardedCells;
        cellHook(ctx.data(),drawX,drawY,&viewport,0);require(forwardedCells==before+1 && sewerCasing.size()==sewerVertexLimit && sewerCore.empty());
        sewerCasing.clear();
    }
    sewerCore.clear();sewerCasing.clear();sewerWallTraces.clear();client=nullptr;styledCurrent=nullptr;styledActive=false;inPass=false;explored=&emptyMask;
    activeStyle=MapStyle::Styled;
    std::cout<<"PASS: artwork-aligned sewer traces, empty-gap preservation, both zooms, clipped contrast strokes, two batches per pass, owned bounded cache and native fallback for unsupported/budget-limited cells\n";
}
static int waterFillCalls=0;
static void __stdcall captureWaterArray(DWORD mode,DWORD count,const void* data) {
    if(batchColor!=0x56606438){captureHybridArray(mode,count,data);return;}
    require(mode==5 && count>0 && count%4==0 && data);++waterFillCalls;
    const auto vertices=static_cast<const GlideVertex* const*>(data);
    for(DWORD i=0;i<count;i+=4) {
        Point center{};for(DWORD j=0;j<4;++j){center.x+=vertices[i+j]->x*.25;center.y+=vertices[i+j]->y*.25;}
        require(explored->contains(inverse({floor(center.x)+.5,floor(center.y)+.5},hybridTransform)));
    }
}
static void testSewerWater() {
    checkContext="sewer water union and native-preserving rendering";
    for(int w:{8,16}) {
        exploration::SewerWater water;
        require(water.add({0,0,w,w*2}) && water.add({w/2,w/4,w+w/2,w*2+w/4}));
        require(water.tiles().size()==2 && water.edges().size()==6);
        require(!std::binary_search(water.edges().begin(),water.edges().end(),std::array<int,4>{w/2,w*2,w,w*2-w/4})); // Shared seam is absent.
        require(water.add({0,0,w,w*2}) && water.edges().size()==6); // Duplicate cell cannot cancel the boundary.
        require(!water.add({0,0,20,20}) && water.tiles().size()==2);
    }
    testHybridClassification();
    std::vector<unsigned char> memory(0x11c210);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);put(memory,0xf16b0,10);put(memory,0x11c1f8,36);put(memory,0x11c1fc,106);
    hybridTransform={10,36,106};Mask mask(.25);mask.revealAround(inverse({22,30},hybridTransform),24);explored=&mask;
    std::vector<DWORD> frame(8),file(306),ctx(14);frame[1]=16;frame[2]=32;file[5]=300;
    for(int i=0;i<300;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());ctx[13]=reinterpret_cast<DWORD>(file.data());ctx[0]=289;
    StyledState drawing;drawing.drawing.quads=1;styledCurrent=&drawing;styledActive=true;activeStyle=MapStyle::Hybrid;observedLevel=92;
    maskActive=true;inPass=true;enabled=true;nativeTownActive=false;haveViewport=true;passViewport={0,0,100,100};
    NativeRect viewport{0,100,0,99};originalCell=captureCell;originalLine=townLine;styledArray=captureWaterArray;styledColor=captureColor;
    sewerWater.clear();sewerWaterFill.clear();sewerCasing.clear();sewerCore.clear();casingCalls=coreCalls=waterFillCalls=0;
    auto before=forwardedCells;cellHook(ctx.data(),10,32,&viewport,0);cellHook(ctx.data(),18,36,&viewport,0);
    require(forwardedCells==before+2 && sewerWater.tiles().size()==2 && sewerWater.edges().size()==6);
    ctx[0]=290;cellHook(ctx.data(),10,32,&viewport,0);require(forwardedCells==before+3 && sewerWater.tiles().size()==2); // Bridge stays native.
    endPass();require(waterFillCalls==1 && casingCalls==1 && coreCalls==1 && sewerWater.tiles().empty());
    require(sewerWaterFill.empty() && sewerCasing.empty() && sewerCore.empty() && !styledBatch);
    styledCurrent=nullptr;styledActive=false;inPass=false;client=nullptr;explored=&emptyMask;activeStyle=MapStyle::Styled;
    std::cout<<"PASS: straight joined sewer wall profiles; water tile union without interior seams, duplicate rejection, clipped fill/border, native water/bridge preservation and three bounded batches\n";
}
static void testLocalHybridTables() {
    checkContext="local hybrid artwork and campaign layer tables";
    char* value=nullptr;std::size_t length=0;
    require(_dupenv_s(&value,&length,"PD2_HYBRID_TABLE_DIR")==0);
    std::string directory=value?value:"";std::free(value);if(directory.empty())return;
    std::ifstream table(directory+"/automap.txt"),objects(directory+"/Objects.txt");
    exploration::HybridArtwork policy;
    require(table && objects && policy.load(table) && policy.protectObjects(objects));
    std::ifstream layers(directory+"/Levels.txt");exploration::CampaignLayers layout;unsigned layer=0;
    require(layers && layout.load(layers) && layout.size()==132);
    require(layout.lookup(76,2,layer) && layer==57 && layout.lookup(78,2,layer) && layer==57);
    require(layout.lookup(92,2,layer) && layer==66 && layout.lookup(93,2,layer) && layer==67);
    require(layout.lookup(1,0,layer) && layer==0 && !layout.lookup(76,1,layer) && !layout.lookup(203,4,layer));
    exploration::CampaignLayers invalid;std::istringstream bad("Id\tAct\tLayer\n76\t2\t57\n76\t2\t66\n");
    require(!invalid.load(bad) && !invalid.lookup(76,2,layer));
    require(policy.walls()>25);
    using Role=exploration::HybridArtwork::Role;
    for(DWORD id:{0u,1u,2u,3u,307u,312u,426u,432u,251u,265u,1354u,1373u,1403u,1497u})require(policy.role(id)==Role::Detail);
    for(DWORD id:{283u,284u,285u,286u,287u,288u})require(policy.role(id)==Role::SewerWall);
    require(policy.role(289)==Role::SewerWater && policy.role(290)==Role::Detail);
    for(DWORD id:{4u,5u,6u,7u,8u,266u,520u})require(policy.role(id)==Role::Water);
    std::cout<<"PASS: local hybrid tables protect roads, water, waypoint, quest artwork, cages, entrances and Act 3 sewer walls; "<<policy.walls()<<" ordinary wall IDs eligible\n";
}
static std::vector<DWORD> opacityColors;
static DWORD opacityLineAlpha=0;
static void __stdcall opacityCaptureColor(DWORD color){opacityColors.push_back(color);}
static void __stdcall opacityCaptureArray(DWORD mode,DWORD count,const void*){require(mode==5 && count==4);}
static void __stdcall opacityCaptureLine(int,int,int,int,DWORD,DWORD alpha){opacityLineAlpha=alpha;}
static void testOverlayOpacity() {
    checkContext="overlay alpha and native color restoration";
    require(overlayOpacity==80 && overlayAlpha(255)==204 && overlayColor(0x949494e0)==0x949494b3);
    GlideVertex q[4]{};std::vector<const void*> p{q,q+1,q+2,q+3};
    styledBatch=&p;styledBatchColor=0x56606438;styledRestoreColor=0x84848438;
    styledColor=opacityCaptureColor;styledArray=opacityCaptureArray;
    floatLineHook(q,q+1);floatPointHook(q);
    require(opacityColors==std::vector<DWORD>{0x5660642d,0x84848438,0x5660642d,0x84848438});
    styledBatch=nullptr;originalLine=opacityCaptureLine;drawFrontier({0,0},{1,1});require(opacityLineAlpha==204);
    overlayOpacity=100;require(overlayColor(0x949494e0)==0x949494e0);
    styledColor=nullptr;styledArray=nullptr;originalLine=nullptr;
    std::cout<<"PASS: overlay alpha scaling preserves RGB, submits through line/point paths and restores native color\n";
}
static void testRasterCache() {
    checkContext="bounded raster cache exact coverage and invalidation";
    auto cache=std::make_unique<exploration::RasterClipCache>();int builds=0;
    std::vector<Rect> result;
    auto build=[&](auto emit){++builds;emit(Rect{-5,-4,6,-3});emit(Rect{-5,-3,6,-2});};
    auto collect=[&](Rect r){result.push_back(r);};
    for(int i=0;i<2;++i){result.clear();cache->query({-5,-4,6,-2,0},build,collect);require(result.size()==1 && result[0].top==-4 && result[0].bottom==-2);}
    require(builds==1 && cache->hits()==1);cache->invalidate();cache->query({-5,-4,6,-2,0},build,collect);require(builds==2);
    auto complex=[&](auto emit){++builds;for(int i=0;i<40;++i)emit(Rect{0,i*2,2,i*2+1});};
    for(int i=0;i<2;++i){result.clear();cache->query({0,0,2,80,0},complex,collect);require(result.size()==40);}
    require(builds==4); // Over-budget entries draw completely and are rebuilt.
    for(int i=0;i<20000;++i){result.clear();cache->query({i,0,i+1,1,0},[&](auto emit){emit(Rect{i,0,i+1,1});},collect);
        require(result.size()==1 && result[0].left==i);}
    for(int divisor:{10,20}) {
        ++gameSerial;Mask mask(.25);explored=&mask;
        for(int y=-50;y<70;++y){mask.revealRange(y,-60,5);if(y%8<5)mask.revealRange(y,20,80);}
        townBoundary=TownBoundary{};townBoundary.bounds={-10,-30,50,50};
        townBoundary.raster[divisor==20?1:0].revealQuarterRect(-10,-30,50,50,divisor);nativeTownActive=true;
        for(int grown=0;grown<2;++grown) {
            if(grown)for(int y=-60;y<65;++y)mask.revealRange(y,-70,90);
            for(int pan:{0,-17,23,0})for(int mode:{0,1,2})for(int repeat=0;repeat<2;++repeat) {
                Rect bounds{-40-pan,-40+pan,60-pan,60+pan};std::set<std::pair<int,int>> expected,actual;
                auto add=[&](Rect r){for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)expected.insert({x,y});};
                if(mode!=1)mask.clipProjected(bounds,divisor,pan,-pan,add);
                if(mode!=0)townBoundary.raster[divisor==20?1:0].clip(-40,-40,60,60,[&](int l,int t,int r,int b){add({l-pan,t+pan,r-pan,b+pan});});
                cachedRasterClips(bounds,divisor,pan,-pan,mode,[&](Rect r){for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)require(actual.insert({x,y}).second);});
                require(actual==expected);
            }
        }
    }
    checkContext="cache context shared across distinct drawing paths";
    nativeTownActive=false;townBoundary=TownBoundary{};
    Mask leftMask(.25),rightMask(.25);
    leftMask.revealAround({-5,0},20);rightMask.revealAround({5,0},20);
    std::set<std::pair<int,int>> pathPixels;
    auto fromWalls=[&]{pathPixels.clear();cachedRasterClips({-40,-40,40,40},10,0,0,0,[&](Rect r){
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)pathPixels.insert({x,y});});};
    auto fromCells=[&]{pathPixels.clear();cachedRasterClips({-40,-40,40,40},10,0,0,0,[&](Rect r){
        for(int y=r.top;y<r.bottom;++y)for(int x=r.left;x<r.right;++x)pathPixels.insert({x,y});});};
    explored=&leftMask;fromWalls();auto leftPixels=pathPixels;
    explored=&rightMask;fromCells();require(pathPixels!=leftPixels);
    explored=&leftMask;fromWalls();require(pathPixels==leftPixels);
    explored=&emptyMask;nativeTownActive=false;townBoundary=TownBoundary{};rasterClips.invalidate();
    std::cout<<"PASS: exact cached clipping, holes, negative pan, zoom, town unions, growth invalidation, collisions and uncached complex-cell fallback\n";
}
static void testWaterReuse() {
    checkContext="water edge reuse matches independent set union";
    exploration::SewerWater water;
    for(int iteration=0;iteration<12;++iteration){
        std::set<std::array<int,3>> referenceTiles;std::set<std::array<int,4>> referenceEdges;water.clear();
        int w=iteration<6?16:8;
        for(int n=0;n<240;++n){int i=iteration%2?239-n:n,x=(i%20-10)*w/2,b=(i/20-6)*w+(i%20)*w/4;
            if(i%7==0)continue;
            if(iteration==3 && i==1)continue;
            require(water.add({x,b-2*w,x+w,b}) && water.add({x,b-2*w,x+w,b}));
            referenceTiles.insert({x,b,w});
        }
        for(auto tile:referenceTiles){int x=tile[0],b=tile[1];
            std::array<std::array<int,2>,4> p{{{x,b-w/4},{x+w/2,b-w/2},{x+w,b-w/4},{x+w/2,b}}};
            for(int i=0;i<4;++i){auto a=p[i],z=p[(i+1)%4];if(z<a)std::swap(a,z);std::array<int,4> edge{a[0],a[1],z[0],z[1]};
                if(!referenceEdges.erase(edge))referenceEdges.insert(edge);}
        }
        auto builds=water.edgeBuilds();
        require(std::vector<std::array<int,3>>(referenceTiles.begin(),referenceTiles.end())==water.tiles());
        require(std::vector<std::array<int,4>>(referenceEdges.begin(),referenceEdges.end())==water.edges());
        if(iteration==1 || iteration==2 || iteration==5 || iteration>=7)require(water.edgeBuilds()==builds);
    }
    water.clear();for(int i=0;i<8192;++i)require(water.add({i*16,0,i*16+16,32}));
    require(water.add({0,0,16,32}) && !water.add({-16,0,0,32}) && water.tiles().size()==8192);
    water.clear();require(water.tiles().empty() && water.edges().empty());
    std::cout<<"PASS: water perimeter reuse across reordered frames, changed tiles, zooms, duplicates and capacity limits\n";
}
int main() {
    testOverlayOpacity(); // Existing rendering contracts then run at 100%.
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
    testTownBoundary();
    testCampaignArtwork();testSessionLifetime();
    testJoinedCampaignAreas();
    testHybridRendering();
    testNativeWallTrace();
    testSewerWater();
    testLocalHybridTables();
    testRasterCache();testWaterReuse();
    return 0;
}
