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
    for(auto style:{MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled}) {
        activeStyle=style;maskActive=styledActive=true;wellWaterVertices.clear();
        cellHook(context.data(),10,40,&viewport,5);
        require(wellWaterVertices.empty()==(style==MapStyle::Styled));wellWaterVertices.clear();
    }
    // Multiple disjoint clips preserve the original asset origin for each part.
    require(queueWellFill(context.data(),{10,8,26,40},{{10,39,13,40},{15,39,18,40}}));
    require(wellWaterVertices.size()==8 && wellWaterVertices[4].x==15 && wellWaterVertices[5].x==18);
    wellWaterVertices.resize(wellVertexLimit);require(!queueWellFill(context.data(),{10,8,26,40},{{10,39,18,40}}));wellWaterVertices.clear();

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
