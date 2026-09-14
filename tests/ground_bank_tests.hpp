static void testGroundBanks() {
    using namespace floor_reader;
    using namespace styled_map;
    checkContext="loaded grassy bank tiles, without automap symbols";
    require(grassyBankLibrary("data\\global\\tiles\\ACT1\\Outdoors\\Pond.dt1"));
    require(grassyBankLibrary("Act1/Outdoors/puddle.dt1") && grassyBankLibrary("Act1/Outdoors/swamp.dt1"));
    for(const char* name:{"Act1/Outdoors/stonewall.dt1","Act1/Outdoors/cliff1.dt1","Act3/Jungle/pond.dt1",
        "customact1/outdoors/pond.dt1","Act1/Outdoors/pond.dt1.extra",""})require(!grassyBankLibrary(name));
    std::array<DWORD,32> rawRoom{};std::array<DWORD,6> lists{};std::array<DWORD,24> tiles{};
    std::array<DWORD,24> entry{};std::array<char,260> library{};
    strcpy_s(library.data(),library.size(),"data\\global\\tiles\\Act1\\Outdoors\\pond.dt1");
    rawRoom[2]=reinterpret_cast<DWORD>(lists.data());lists[2]=reinterpret_cast<DWORD>(tiles.data());lists[3]=2;
    auto floorFlags=reinterpret_cast<std::uint8_t*>(entry.data())+0x28;
    memset(floorFlags,1,25);
    entry[22]=reinterpret_cast<DWORD>(library.data());entry[6]=16;
    tiles[2]=1;tiles[3]=1;tiles[6]=reinterpret_cast<DWORD>(entry.data());
    tiles[14]=2;tiles[15]=1;tiles[18]=reinterpret_cast<DWORD>(entry.data());
    Room room{rawRoom.data(),nullptr,nullptr,2,100,200,15,15};
    std::vector<std::uint16_t> grid(225);
    for(int y=5;y<10;++y)for(int x=5;x<10;++x)grid[y*15+x]=1;
    grid[5*15+6]=5;grid[5*15+7]=3;grid[5*15+8]=0x21; // Architecture/blank on the same floor.
    grid[6*15+6]=0x181; // A transient occupant cannot change material classification.
    BankRead stats{};auto banks=copyBanks(room,grid,stats);
    require(stats.tiles==2 && stats.banks==2 && !stats.failures && banks.size()==225);
    entry[22]=0;BankRead missingLibrary{};
    require(copyBanks(room,grid,missingLibrary).empty() && missingLibrary.failures==2);
    entry[22]=reinterpret_cast<DWORD>(library.data());
    for(int y=0;y<15;++y)for(int x=0;x<15;++x)
        require(bool(banks[y*15+x])==(x>=5 && x<10 && y>=5 && y<10 && (grid[y*15+x]&0x27)==1));
    lists[3]=16385;BankRead invalid{};require(copyBanks(room,grid,invalid).empty() && invalid.failures==1);
    lists[3]=2;tiles[2]=100;BankRead outside{};copyBanks(room,grid,outside);require(outside.failures==1);
    tiles[2]=1;library.fill('x');BankRead unterminated{};require(copyBanks(room,grid,unterminated).empty() && unterminated.failures==2);
    library.fill(0);strcpy_s(library.data(),library.size(),"Act1/Outdoors/stonewall.dt1");
    BankRead stone{};require(copyBanks(room,grid,stone).empty() && stone.tiles==2 && stone.banks==0);
    strcpy_s(library.data(),library.size(),"data/global/tiles/PD2assets/psnwell/used/rivbank.dt1");
    room.level=202;BankRead well{};const auto wellBanks=copyBanks(room,grid,well);
    require(well.banks==2 && !well.failures && wellBanks.size()==banks.size());
    for(int y=0;y<15;++y)for(int x=0;x<15;++x)
        require(bool(wellBanks[y*15+x])==(x>=5 && x<15 && y>=5 && y<10 && (grid[y*15+x]&0x26)==0));
    // A head adds low collision to dry ground on a partial river tile.
    // Asymmetric rows also verify DT1's bottom-to-top collision ordering.
    floorFlags[20]=0;floorFlags[4]=0;
    BankRead dryProps{};const auto classified=copyBanks(room,grid,dryProps);
    require(!classified[5*15+5] && !classified[9*15+9]);
    require(classified[9*15+5] && classified[5*15+9]);
    require(classified[6*15+6]); // A monster on actual water retains its material.
    memset(floorFlags,1,25);
    strcpy_s(library.data(),library.size(),"data/global/tiles/PD2assets/dtprivate/oasis.dt1");
    room.level=203;BankRead oasis{};require(copyBanks(room,grid,oasis)==wellBanks && oasis.banks==2);
    strcpy_s(library.data(),library.size(),"data/global/tiles/PD2assets/dtprivate/walls.dt1");
    BankRead architecture{};require(copyBanks(room,grid,architecture).empty() && architecture.banks==0);
    require(!mapWaterLibrary("custompd2assets/dtprivate/oasis.dt1") && !mapWaterLibrary("PD2assets/a5_river.dt1.extra"));

    checkContext="bank material survives owned snapshots, room seams and clipped geometry";
    FloorCopies copies;copies.ingest(100,200,15,15,grid,banks);
    banks.clear();grid.assign(225,0); // The worker owns its immutable material copy.
    ChunkedMap cached;const auto& saved=*copies.rooms[0];
    cached.floor().ingest(saved.x,saved.y,saved.w,saved.h,saved.flags,saved.banks);
    require(cached.floor().connect({101,201}));
    exploration::Mask visible(.25);visible.revealAround({107,207},132);
    auto drawing=cached.build(visible);const auto reference=cached.floor().build(visible);
    auto coverage=[](const Drawing& d) {
        std::map<std::tuple<int,int,int,int>,bool> edges;
        for(auto w:d.walls) {
            const int n=int(std::abs(w.b.x-w.a.x)+std::abs(w.b.y-w.a.y));
            for(int i=0;i<n;++i){
                int ax=int(w.a.x+(w.b.x-w.a.x)*i/n),ay=int(w.a.y+(w.b.y-w.a.y)*i/n);
                int bx=int(w.a.x+(w.b.x-w.a.x)*(i+1)/n),by=int(w.a.y+(w.b.y-w.a.y)*(i+1)/n);
                if(std::tie(ax,ay)>std::tie(bx,by)){std::swap(ax,bx);std::swap(ay,by);}
                require(edges.emplace(std::make_tuple(ax,ay,bx,by),w.bank).second);
            }
        }
        return edges;
    };
    const auto expected=coverage(reference);require(coverage(drawing)==expected && !expected.empty());
    bool blue=false,wall=false;
    for(const auto& [edge,bank]:expected) {
        auto [ax,ay,bx,by]=edge;
        auto cell=[&](int x,int y){return x>=100 && x<115 && y>=200 && y<215 && saved.banks[(y-200)*15+x-100];};
        const bool material=ax==bx?(cell(ax-1,ay)||cell(ax,ay)):(cell(ax,ay-1)||cell(ax,ay));
        require(bank==material);blue|=bank;wall|=!bank;
    }
    require(blue && wall);compactWalls(drawing.walls);require(coverage(drawing)==expected);
    require(coverage(cached.build(visible))==expected && cached.rebuiltChunks==0);
    auto request=std::make_unique<BuildRequest>();request->session=10;request->level=2;
    request->player={101,201};request->visible.spans=visible.rows();request->rooms=copies.rooms;
    Worker worker;worker.submit(std::move(request));bool done=false;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<deadline) {
        if(auto result=worker.take()){require(result->success && coverage(result->drawing)==expected);done=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(done);

    checkContext="bank contours use light blue only in Hybrid at both zooms";
    const auto oldStyle=activeStyle;const auto oldWall=wallColor;
    StyledState state;styledCurrent=&state;state.drawing=drawing;explored=&visible;
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    for(int divisor:{10,20}) {
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);riverTint.select(gameSerial,lastLevelKey,divisor);
        const Transform t{double(divisor),-200,80};
        for(auto style:{MapStyle::Hybrid,MapStyle::Styled}) {
            activeStyle=style;wallColor=exploration::BoundaryColor::Orange;
            appearanceColors.clear();appearancePositions.clear();drawHybridWalls(t,{0,0,500,500});
            std::vector<DWORD> colors{overlayColor(0x181818c0),overlayColor(exploration::wallRGBA(wallColor,224))};
            if(style==MapStyle::Hybrid)colors.push_back(overlayColor(waterEdgeColor));
            require(appearanceColors==colors);
        }
    }
    activeStyle=oldStyle;wallColor=oldWall;styledCurrent=nullptr;explored=&emptyMask;
    std::cout<<"PASS: grassy ground banks without automap symbols, bounded native reads, wall exclusions, owned worker material, cached edge coverage and Hybrid-only color at both zooms\n";
}
static void testOverlaidWaterBanks() {
    using namespace floor_reader;
    using namespace styled_map;
    checkContext="overlaid river water colors both sides of collision contours without coloring dry props";
    std::array<DWORD,32> rawRoom{};std::array<DWORD,6> lists{};std::array<DWORD,24> tiles{},waterEntry{},detailEntry{};
    const char library[]="data/global/tiles/PD2assets/psnwell/used/rivbank.dt1";
    rawRoom[2]=reinterpret_cast<DWORD>(lists.data());lists[2]=reinterpret_cast<DWORD>(tiles.data());lists[3]=2;
    waterEntry[22]=detailEntry[22]=reinterpret_cast<DWORD>(library);
    tiles[6]=reinterpret_cast<DWORD>(waterEntry.data());tiles[18]=reinterpret_cast<DWORD>(detailEntry.data());
    // Actual river floor (0,10,45): its dry left column sits beneath wall
    // (1,48,2). Decorative floor (0,59,10) contributes zero collision.
    auto flags=reinterpret_cast<std::uint8_t*>(waterEntry.data())+0x28;
    for(int y=0;y<5;++y)for(int x=1;x<5;++x)flags[y*5+x]=1;
    Room room{rawRoom.data(),nullptr,nullptr,202,100,200,10,10};
    std::vector<std::uint16_t> grid(100);
    for(int y=0;y<5;++y)grid[y*10]=7;
    // A separate head, on dry floor, must retain its wall color.
    grid[7*10+7]=1;
    BankRead stats;auto banks=copyBanks(room,grid,stats);
    require(!stats.failures && stats.tiles==2 && std::count(banks.begin(),banks.end(),1)==20);
    FloorCopies copies;copies.ingest(100,200,10,10,grid,banks);
    ChunkedMap map;const auto& copy=*copies.rooms[0];map.floor().ingest(copy.x,copy.y,copy.w,copy.h,copy.flags,copy.banks);
    require(map.floor().connect({102,202}));
    exploration::Mask visible(.25);visible.revealAround({105,205},132);
    auto drawing=map.build(visible);bool foundShore=false,foundHead=false;
    for(const auto& stroke:drawing.walls) {
        if(stroke.a.x==101 && stroke.b.x==101 && std::min(stroke.a.y,stroke.b.y)<205){require(stroke.bank);foundShore=true;}
        if(std::min(stroke.a.x,stroke.b.x)>=107 && std::max(stroke.a.x,stroke.b.x)<=108 &&
           std::min(stroke.a.y,stroke.b.y)>=207 && std::max(stroke.a.y,stroke.b.y)<=208){require(!stroke.bank);foundHead=true;}
    }
    require(foundShore && foundHead);
    // Material only: the correction must not alter traversability or geometry.
    Level plain;plain.ingest(100,200,10,10,grid);require(plain.connect({102,202}));
    const auto plainDrawing=plain.build(visible);
    auto lengths=[](const Drawing& d){double n=0;for(auto s:d.walls)n+=std::hypot(s.b.x-s.a.x,s.b.y-s.a.y);return n;};
    require(lengths(drawing)==lengths(plainDrawing));
    for(int y=200;y<210;++y)for(int x=100;x<110;++x)require(map.floor().contains({double(x),double(y)})==plain.contains({double(x),double(y)}));
    std::cout<<"PASS: layered river shoreline, dry prop exclusion, owned worker material and unchanged floor/contour coverage\n";
}
