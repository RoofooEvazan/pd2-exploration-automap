static unsigned wellTestNative=0;
static bool wellTestThrow=false;
static void __stdcall captureWellOutline(void* context,int,int,NativeRect*,int) {
    require(read<DWORD>(context)==1693 && entranceTestPalette[132]==0xff708860);
    require(entranceTestPalette[222]==0xffd8d8d8);++wellTestNative;
    if(wellTestThrow)throw 1;
}
static void testNativeMapWater() {
    checkContext="explicit water fills exclude bridge pixels at both native zooms";
    for(int w:{8,16}) {
        const int h=w*2;std::vector<unsigned char> bytes;
        bytes.push_back(static_cast<unsigned char>(w));
        for(int x=0;x<w;++x)bytes.push_back(static_cast<unsigned char>(x<w/2?151:255));
        for(int y=0;y<h;++y)bytes.push_back(128);
        std::vector<Rect> rects;
        require(exploration::nativeWaterFill(bytes.data(),bytes.size(),w,h,rects) && rects.size()==1);
        require(rects[0].left==0 && rects[0].right==w/2 && rects[0].top==h-1 && rects[0].bottom==h);
        require(!exploration::nativeWaterFill(bytes.data(),bytes.size()-1,w,h,rects));
        require(!exploration::nativeWaterFill(bytes.data(),bytes.size(),w+1,h,rects));
        require(!exploration::nativeWaterFill(nullptr,bytes.size(),w,h,rects));
    }
    std::istringstream definitions(artworkFixture()+"46\tfl\t10\t0\t0\tPW outline water=1590\t1693\t\t-1\t\t-1\t\t-1\n");
    require(hybridArtwork.load(definitions) && hybridArtwork.hasPoisonedWellFill());
    require(hybridArtwork.poisonedWellFill(1693,202)==1590 && hybridArtwork.poisonedWellOutline(1693,202));
    for(unsigned level:{2u,92u,203u})require(!hybridArtwork.poisonedWellFill(1693,level) && !hybridArtwork.poisonedWellOutline(1693,level));
    require(!hybridArtwork.blueTerrainTile(1693) && !hybridArtwork.styledDetail(1693));
    for(const char* label:{"Water1","Water4","PD2 FullWater"})require(exploration::HybridArtwork::describe(label)==exploration::HybridArtwork::Role::Water);

    checkContext="water fill is removed in all styles; only Hybrid registers shore colors";
    const auto oldOpacity=overlayOpacity;overlayOpacity=100;
    std::vector<unsigned char> memory(0x11c8bc);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);put(memory,0xf16b0,10);
    put(memory,0x11c1f8,0);put(memory,0x11c1fc,0);
    std::vector<DWORD> frame(100),file(1986),context(18);
    file[0]=6;file[5]=1980;for(unsigned i=0;i<1980;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    context[0]=1693;context[13]=reinterpret_cast<DWORD>(file.data());frame[1]=16;frame[2]=32;
    std::vector<unsigned char> bytes{16};
    for(int x=0;x<16;++x)bytes.push_back(static_cast<unsigned char>(x<8?151:255));
    for(int y=0;y<32;++y)bytes.push_back(128);
    frame[7]=DWORD(bytes.size());memcpy(frame.data()+8,bytes.data(),bytes.size());
    originalLine=appearanceNativeLine;styledColor=captureColor;styledArray=captureAppearanceArray;originalCell=captureCell;
    NativeRect viewport{0,100,0,99};Transform t{10,0,0};Mask mask(.25);explored=&mask;
    StyledState state;state.floorCells=1;styledCurrent=&state;
    enabled=inPass=haveViewport=true;nativeTownActive=false;observedLevel=202;passViewport={0,0,100,100};
    activeStyle=MapStyle::Original;maskActive=styledActive=false;
    wellOutlineCount=wellOutlineClips=0;auto before=forwardedCells;
    appearanceColors.clear();appearancePositions.clear();wellFillShapes.clear();
    cellHook(context.data(),10,40,&viewport,5);
    require(mask.size()==0 && forwardedCells==before+1 && wellFillShapes.empty());
    drawWellOutlines();require(appearanceColors.empty() && appearancePositions.empty() && !styledBatch);
    mask.revealAround(inverse({20,40},t),132);
    for(auto style:{MapStyle::Original,MapStyle::Native,MapStyle::Styled,MapStyle::Hybrid}) {
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);wellFillShapes.clear();
        activeStyle=style;maskActive=styledActive=style!=MapStyle::Original;
        appearanceColors.clear();appearancePositions.clear();
        cellHook(context.data(),10,40,&viewport,5);drawWellOutlines();
        require(appearanceColors.empty() && appearancePositions.empty());
        require(waterTint.size()==std::size_t(style==MapStyle::Hybrid));
        require(wellFillShapes.empty()==(style!=MapStyle::Hybrid));
    }
    // Removing the fill must not broaden shoreline classification onto dry heads.
    auto material=waterTint.prepare({{inverse({10,39.5},t),inverse({18,39.5},t)},
        {inverse({22,39.5},t),inverse({25,39.5},t)}});
    require(material.size()==2 && material[0].water && !material[1].water);

    checkContext="discovered water tiles avoid repeated visibility and artwork work while panning";
    const auto queries=rasterClips.hits()+rasterClips.misses();const auto registrations=wellShoreRegistrations;
    for(int repeat=0;repeat<100;++repeat) {
        const int pan=(repeat%3)*7-9;put(memory,0x11c1f8,pan);
        mask.revealRange(1000+repeat,0,4);
        registerWellShore(context.data(),1693,10-pan,40,&viewport);
    }
    require(rasterClips.hits()+rasterClips.misses()==queries && wellShoreRegistrations==registrations && waterTint.size()==1);
    put(memory,0x11c1f8,0);
    checkContext="shoreline registration waits for discovery and refreshes a hidden tile";
    Mask unseen(.25);explored=&unseen;++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    registerWellShore(context.data(),1693,10,40,&viewport);require(waterTint.size()==0);
    unseen.revealAround(inverse({20,40},t),132);
    registerWellShore(context.data(),1693,10,40,&viewport);require(waterTint.size()==1);
    explored=&mask;

    checkContext="shoreline color registration survives water just outside the viewport";
    activeStyle=MapStyle::Hybrid;context[0]=1693;++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    NativeRect aboveWater{0,100,0,32};
    registerWellShore(context.data(),1693,10,40,&aboveWater);require(waterTint.size()==1);
    checkContext="shoreline color registration survives water just beyond discovery";
    Mask beforeWater(.25);beforeWater.revealAround(inverse({16,18},t),12);explored=&beforeWater;
    ++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    registerWellShore(context.data(),1693,10,40,&viewport);require(waterTint.size()==1);

    checkContext="both zooms and offset frames retain shore colors without fill submissions";
    for(int divisor:{10,20})for(auto style:{MapStyle::Original,MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled})for(int pan:{-9,0,13}) {
        const int w=divisor==10?16:8,h=w*2;const Transform viewTransform{double(divisor),double(pan),-3};
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,pan);put(memory,0x11c1fc,-3);
        frame[1]=w;frame[2]=h;frame[3]=DWORD(-3);frame[4]=2;
        bytes.clear();
        for(int row=0;row<h;++row) {
            if(row<w/2) {
                bytes.push_back(static_cast<unsigned char>(w));
                for(int col=0;col<w;++col)bytes.push_back(static_cast<unsigned char>((col<w/4 || col>=w*3/4)?151:255));
            }
            bytes.push_back(128);
        }
        frame[7]=DWORD(bytes.size());memcpy(frame.data()+8,bytes.data(),bytes.size());
        context[0]=1693;++gameSerial;activeStyle=style;waterTint.select(gameSerial,lastLevelKey,divisor);
        Mask waterMask(.25);waterMask.revealAround(inverse({24.5,40.5},viewTransform),12);explored=&waterMask;
        NativeRect cropped{19,30,35,41};
        appearanceColors.clear();appearancePositions.clear();
        registerWellShore(context.data(),1693,23,40,&cropped);drawWellOutlines();
        require(waterTint.size()==std::size_t(style==MapStyle::Hybrid));
        require(appearanceColors.empty() && appearancePositions.empty());
    }
    explored=&mask;

    checkContext="dull green native edges preserve bridge colors and restore the palette";
    std::array<DWORD,256> palette{};palette.fill(0xff010203);palette[132]=0xff18fc00;palette[222]=0xffd8d8d8;
    nativePalette=palette.data();entrancePaletteDraw=captureEntrancePalette;entranceTestPalette=palette;entranceUploads=0;
    originalCell=captureWellOutline;context[0]=1693;
    require(queueWellOutline(context.data(),0,0,&viewport,5,nullptr));context[0]=1590;
    drawWellOutlines();require(wellTestNative==1 && entranceUploads==2 && entranceTestPalette==palette);
    context[0]=1693;require(queueWellOutline(context.data(),0,0,&viewport,5,nullptr));wellTestThrow=true;bool caught=false;
    try{drawWellOutlines();}catch(...){caught=true;}
    require(caught && entranceTestPalette==palette && wellOutlineCount==0);wellTestThrow=false;
    std::istringstream protectedObject("Name\tAutoMap\nObject\t1590\n");
    require(hybridArtwork.protectObjects(protectedObject) && !hybridArtwork.poisonedWellFill(1693,202));
    std::istringstream conflict(artworkFixture()+"46\tfl\t10\t0\t0\tPW outline water=1590\t1693\t\t-1\t\t-1\t\t-1\n"
        "46\tfl\t10\t1\t1\tPW outline water=1591\t1693\t\t-1\t\t-1\t\t-1\n");
    require(hybridArtwork.load(conflict) && !hybridArtwork.poisonedWellFill(1693,202));
    wellFillShapes.clear();wellOutlineCells.clear();nativePalette=nullptr;entrancePaletteDraw=nullptr;
    overlayOpacity=oldOpacity;inPass=maskActive=styledActive=haveViewport=false;styledCurrent=nullptr;explored=&emptyMask;client=nullptr;
    std::cout<<"PASS: no added water fill, preserved shoreline coverage and bridge exclusions, Original reveal, all styles/zooms, panning cache reuse and palette restoration\n";
}
static void __stdcall captureOriginalWall(void* context,int x,int y,NativeRect*,int mode) {
    require(read<DWORD>(context)==1572 && x==10 && y==40 && mode==2);
    require(entranceTestPalette[220]==0xff999999 && entranceTestPalette[221]==0xff7f7f7f);
    require(entranceTestPalette[151]==0xff708860 && entranceTestPalette[1]==0xff202020);
    ++wellTestNative;if(wellTestThrow)throw 1;
}
static void testOriginalWallBrightness() {
    checkContext="Original fullscreen white walls dim independently of icons, water and minimap";
    std::istringstream definitions(artworkFixture()+"46\tfl\t10\t0\t0\tPW wall\t1572\t\t-1\t\t-1\t\t-1\n");
    require(hybridArtwork.load(definitions));
    std::vector<unsigned char> memory(0x11c8bc);client=memory.data();put(memory,0x11c8b8,1);
    std::array<DWORD,256> palette{};palette.fill(0xff102040);palette[220]=0xffffffff;palette[221]=0xffd4d4d4;
    palette[151]=0xff708860;palette[1]=0xff202020;
    nativePalette=palette.data();entrancePaletteDraw=captureEntrancePalette;entranceTestPalette=palette;entranceUploads=0;
    DWORD context[18]{};context[0]=1572;NativeRect viewport{0,100,0,99};
    inPass=enabled=true;maskActive=styledActive=nativeTownActive=false;activeStyle=MapStyle::Original;observedLevel=202;
    originalCell=captureOriginalWall;wellTestNative=0;
    cellHook(context,10,40,&viewport,2);require(originalWallCount==1 && !wellTestNative);
    context[0]=308;require(!queueOriginalWall(context,10,40,&viewport,2));
    context[0]=1572;drawOriginalWalls();require(wellTestNative==1 && !originalWallCount && entranceUploads==2 && entranceTestPalette==palette);
    // Same native sprite/placement/mode, two palette uploads for a whole batch.
    for(int i=0;i<20;++i)require(queueOriginalWall(context,10,40,&viewport,2));
    drawOriginalWalls();require(wellTestNative==21 && entranceUploads==4 && entranceTestPalette==palette);
    put(memory,0x11c8b8,0);require(!queueOriginalWall(context,10,40,&viewport,2));
    put(memory,0x11c8b8,1);require(queueOriginalWall(context,10,40,&viewport,2));
    wellTestThrow=true;bool caught=false;try{drawOriginalWalls();}catch(...){caught=true;}
    require(caught && !originalWallCount && entranceTestPalette==palette);wellTestThrow=false;
    originalWallCells.clear();nativePalette=nullptr;entrancePaletteDraw=nullptr;client=nullptr;inPass=false;
    hybridArtwork=exploration::HybridArtwork{};
    std::cout<<"PASS: Original fullscreen white-only 40% dimming, untouched colors/icons/minimap, batched uploads and palette restoration\n";
}
