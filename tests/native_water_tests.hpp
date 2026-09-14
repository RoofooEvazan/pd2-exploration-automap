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

    checkContext="Original keeps native reveal; Hybrid retains water; Styled removes it";
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
    wellWaterVertices.clear();wellOutlineCount=wellOutlineClips=0;auto before=forwardedCells;
    cellHook(context.data(),10,40,&viewport,5);
    require(mask.size()==0 && forwardedCells==before+1 && wellWaterVertices.size()==4);
    for(const auto& v:wellWaterVertices)require(v.x>=10 && v.x<=18 && v.y>=39 && v.y<=40);
    appearanceColors.clear();drawWellWater();require(appearanceColors==std::vector<DWORD>{0x52644070});
    require(wellWaterVertices.empty() && !styledBatch);
    mask.revealAround(inverse({20,40},t),132);
    ++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);require(waterTint.size()==0);
    for(auto style:{MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled}) {
        activeStyle=style;maskActive=styledActive=true;wellWaterVertices.clear();
        cellHook(context.data(),10,40,&viewport,5);
        require(wellWaterVertices.empty()==(style==MapStyle::Styled));wellWaterVertices.clear();
        require(waterTint.size()==std::size_t(style==MapStyle::Native?0:1));
    }
    // Only the actual water pixels tint nearby contours, not the dry half of
    // the same native diamond. The floor's bank tags remain independent.
    auto material=waterTint.prepare({{inverse({10,39.5},t),inverse({18,39.5},t)},
        {inverse({22,39.5},t),inverse({25,39.5},t)}});
    require(material.size()==2 && material[0].water && !material[1].water);
    // Multiple disjoint clips preserve the original asset origin for each part.
    const auto shape=wellFillShape(context.data());require(shape!=nullptr);
    require(queueWellFill(*shape,{10,8,26,40},{{10,39,13,40},{15,39,18,40}}));
    require(wellWaterVertices.size()==8 && wellWaterVertices[4].x==15 && wellWaterVertices[5].x==18);
    wellWaterVertices.resize(wellVertexLimit);require(!queueWellFill(*shape,{10,8,26,40},{{10,39,18,40}}));wellWaterVertices.clear();

    checkContext="shoreline color registration survives water just outside the viewport";
    activeStyle=MapStyle::Hybrid;context[0]=1693;++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    NativeRect aboveWater{0,100,0,32};
    queueWellWaterForOutline(context.data(),1693,10,40,&aboveWater);
    require(wellWaterVertices.empty() && waterTint.size()==1);
    checkContext="shoreline color registration survives water just beyond discovery";
    Mask beforeWater(.25);beforeWater.revealAround(inverse({16,18},t),12);explored=&beforeWater;
    ++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    queueWellWaterForOutline(context.data(),1693,10,40,&viewport);
    require(wellWaterVertices.empty() && waterTint.size()==1);

    checkContext="tight water bounds preserve every visible water pixel and bridge gap";
    for(int divisor:{10,20})for(auto style:{MapStyle::Original,MapStyle::Native,MapStyle::Hybrid})for(int pan:{-9,0,13}) {
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
        context[0]=1693;++gameSerial;activeStyle=style;
        Mask waterMask(.25);waterMask.revealAround(inverse({24.5,40.5},viewTransform),12);explored=&waterMask;
        NativeRect cropped{19,30,35,41};Rect asset{};require(frameBounds(context.data(),23,40,&asset));
        wellWaterVertices.clear();queueWellWaterForOutline(context.data(),1693,23,40,&cropped);
        std::set<std::pair<int,int>> expected,actual;
        for(int row=h-w/2;row<h;++row)for(int col=0;col<w;++col) {
            if(col>=w/4 && col<w*3/4)continue;
            const int x=asset.left+col,y=asset.top+row;
            if(x<cropped.left || x>=cropped.right || y<=cropped.top || y>cropped.bottom)continue;
            if(style==MapStyle::Original || waterMask.contains(inverse({x+.5,y+.5},viewTransform)))expected.insert({x,y});
        }
        for(std::size_t q=0;q<wellWaterVertices.size();q+=4) {
            const auto& a=wellWaterVertices[q];const auto& b=wellWaterVertices[q+2];
            for(int y=int(a.y);y<int(b.y);++y)for(int x=int(a.x);x<int(b.x);++x)require(actual.insert({x,y}).second);
        }
        require(actual==expected && !actual.empty());
    }
    explored=&mask;wellWaterVertices.clear();

    checkContext="dull green native edges preserve bridge colors and restore the palette";
    std::array<DWORD,256> palette{};palette.fill(0xff010203);palette[132]=0xff18fc00;palette[222]=0xffd8d8d8;
    nativePalette=palette.data();entrancePaletteDraw=captureEntrancePalette;entranceTestPalette=palette;entranceUploads=0;
    originalCell=captureWellOutline;context[0]=1693;
    require(queueWellOutline(context.data(),0,0,&viewport,5,nullptr));context[0]=1590;
    drawWellWater();require(wellTestNative==1 && entranceUploads==2 && entranceTestPalette==palette);
    context[0]=1693;require(queueWellOutline(context.data(),0,0,&viewport,5,nullptr));wellTestThrow=true;bool caught=false;
    try{drawWellWater();}catch(...){caught=true;}
    require(caught && entranceTestPalette==palette && wellOutlineCount==0 && wellWaterVertices.empty());wellTestThrow=false;
    std::istringstream protectedObject("Name\tAutoMap\nObject\t1590\n");
    require(hybridArtwork.protectObjects(protectedObject) && !hybridArtwork.poisonedWellFill(1693,202));
    std::istringstream conflict(artworkFixture()+"46\tfl\t10\t0\t0\tPW outline water=1590\t1693\t\t-1\t\t-1\t\t-1\n"
        "46\tfl\t10\t1\t1\tPW outline water=1591\t1693\t\t-1\t\t-1\t\t-1\n");
    require(hybridArtwork.load(conflict) && !hybridArtwork.poisonedWellFill(1693,202));
    wellFillShapes.clear();wellOutlineCells.clear();nativePalette=nullptr;entrancePaletteDraw=nullptr;
    overlayOpacity=oldOpacity;inPass=maskActive=styledActive=haveViewport=false;styledCurrent=nullptr;explored=&emptyMask;client=nullptr;
    std::cout<<"PASS: native map water coverage, bridge exclusions, Original reveal, Hybrid/Styled separation, disjoint clipping, budgets and palette restoration\n";
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
