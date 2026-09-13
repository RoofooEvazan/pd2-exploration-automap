// Real runtime queue/clipping route with mocked native drawing and palettes.
static std::array<DWORD,256> entranceTestPalette{};
static unsigned entranceUploads=0,entranceTestDraws=0;
static bool entranceThrow=false;
static DWORD entranceExpectedAlpha=0,entranceExpectedRGB=0;
static void __stdcall captureEntrancePalette(DWORD type,const void* data) {
    require(type==2 && data);memcpy(entranceTestPalette.data(),data,1024);++entranceUploads;
}
static void __stdcall captureEntranceQuad(DWORD mode,DWORD count,const void* data,DWORD stride) {
    require(batchColor==entranceExpectedAlpha && entranceTestPalette[1]==entranceExpectedRGB);
    captureQuad(mode,count,data,stride); // Checks unchanged coverage range, UVs and vertex colors.
    ++entranceTestDraws;if(entranceThrow)throw 1;
}
static void __stdcall prepareEntrance(void* context,int,int,NativeRect*,int mode) {
    require(context && read<DWORD>(context)==308);
    styledColor(DWORD(nativeCellAlpha(mode)));quadHook(5,4,testQuad,28);
}
static FARPROC WINAPI resolveEntrance(HMODULE module,LPCSTR name) {
    require(std::string(name)=="_grTexDownloadTable@8");
    return reinterpret_cast<FARPROC>(reinterpret_cast<unsigned char*>(module)+0xb45c0);
}
static void testEntranceVisibility() {
    checkContext="entrance classification and unchanged geometry roles";
    using A=exploration::HybridArtwork;
    for(auto label:{"Cave exit","3S exit wr01","CExt21_0","CDwn_0","S Edwn in Twn","temple str dwn",
        "Crypt_11","Den of Evil","FTwr_marker","ice cave rt ent","stairsr","stairsl","TrapDoor"})require(A::entranceLabel(label));
    for(auto label:{"WR A","Water","Road","Waypoint","Shrine","event marker","Custom Fake-Stairs","Arcane StairsLeft",
        "FTwr_wl15_0","TomeWall_utlr80","door","Cave Ltop"})require(!A::entranceLabel(label));
    auto policy=std::make_unique<A>();
    std::istringstream table("LevelName\tDescription\tCel1\tDescription\tCel2\tDescription\tCel3\tDescription\tCel4\n"
        "1\tCave exit\t42\tWater\t43\tstairs\t44\tWall\t45\n"
        "1\tOther\t44\t\t0\t\t0\t\t0\n");
    require(policy->load(table) && policy->entrance(42) && policy->entrance(308));
    for(DWORD id:{0u,43u,44u,65536u})require(!policy->entrance(id));
    const auto role=policy->role(42);
    std::istringstream objects("Name\tAutoMap\nstairsr\t693\nShrine\t42\n");
    require(policy->protectObjects(objects) && policy->entrance(693) && !policy->entrance(42) && policy->role(42)==role);
    std::istringstream bad("Missing header\n");require(!policy->load(bad) && !policy->entrance(308) && !policy->entrance(693));
    require(visibilityBoost(0)==0 && visibilityBoost(64)==112 && visibilityBoost(128)==224 &&
        visibilityBoost(192)==255 && visibilityBoost(255)==255);
    require(brighterPaletteColor(0xab8040ff)==0xabe070ff && brighterPaletteColor(0)==0);

    checkContext="optional native palette binding signatures";
    styledColor=captureColor;
    std::vector<unsigned char> glide(0x1a000);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(glide.data());dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(glide.data()+0x100);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->FileHeader.TimeDateStamp=0x4b95c1e2;nt->OptionalHeader.SizeOfImage=0x1a000;
    auto fakeWrapper=reinterpret_cast<HMODULE>(0x400000);
    glide[0xcf91]=0xbe;put(glide,0xcf92,glide.data()+0x15b0a);
    glide[0xcfa4]=0xba;put(glide,0xcfa5,glide.data()+0x15b08);
    glide[0xd03c]=0x68;put(glide,0xd03d,glide.data()+0x15b08);
    glide[0xd041]=0x6a;glide[0xd042]=2;glide[0xd043]=0xe8;put(glide,0xd044,int(0x5fa2-0xd048));
    glide[0x5fa2]=0xff;glide[0x5fa3]=0x25;put(glide,0x5fa4,glide.data()+0x111f8);
    put(glide,0x111f8,resolveEntrance(fakeWrapper,"_grTexDownloadTable@8"));
    require(bindEntrancePalette(glide.data(),fakeWrapper,resolveEntrance));
    for(auto offset:{0xcf91,0xcf92,0xcfa4,0xcfa5,0xd03c,0xd03d,0xd041,0xd042,0xd043,0xd044,0x5fa2,0x5fa3,0x5fa4,0x111f8}) {
        glide[offset]^=1;require(!bindEntrancePalette(glide.data(),fakeWrapper,resolveEntrance));glide[offset]^=1;
        require(!entrancePaletteDraw && !nativePalette);
    }
    nt->FileHeader.TimeDateStamp^=1;require(!bindEntrancePalette(glide.data(),fakeWrapper,resolveEntrance));
    require(!bindEntrancePalette(nullptr,nullptr));

    checkContext="entrance opacity, fixed-alpha minimap brightness, queue bounds and restoration";
    std::vector<unsigned char> memory(0x11c8bc);client=memory.data();
    std::array<DWORD,256> palette{};palette.fill(0xff804020);nativePalette=palette.data();
    entrancePaletteDraw=captureEntrancePalette;originalCell=prepareEntrance;originalQuad=captureEntranceQuad;
    NativeRect viewport{0,100,0,100};DWORD context[18]{};context[0]=308;
    for(DWORD view:{0u,1u})for(int mode:{0,1,2,5}) {
        put(memory,0x11c8b8,view);entranceUploads=entranceTestDraws=0;entranceTestPalette=palette;
        std::vector<Rect> clips{{12,20,28,24},{12,28,28,32}};
        require(queueEntrance(context,1,2,&viewport,mode,&clips));
        clips.clear();context[0]=999; // Queue owns both context and clipping.
        const bool brightness=view==0 || mode==5;
        entranceExpectedAlpha=brightness?DWORD(nativeCellAlpha(mode)):visibilityBoost(DWORD(nativeCellAlpha(mode)));
        entranceExpectedRGB=brightness?0xffe07038:palette[1];
        drawEntrances();require(entranceTestDraws==2 && entranceUploads==(brightness?2u:0u));
        require(entranceTestPalette==palette && palette[1]==0xff804020 && batchColor==DWORD(nativeCellAlpha(mode)));
        require(!terrainClips && entranceAlpha==-1 && entranceCount==0 && entranceClipCount==0);context[0]=308;
    }
    // Group cost stays at two palette changes regardless of entrance count.
    put(memory,0x11c8b8,0u);entranceExpectedAlpha=255;entranceExpectedRGB=0xffe07038;
    entranceUploads=entranceTestDraws=0;
    for(int i=0;i<64;++i)require(queueEntrance(context,0,0,&viewport,5,nullptr));
    require(!queueEntrance(context,0,0,&viewport,5,nullptr));drawEntrances();
    require(entranceUploads==2 && entranceTestDraws==64);
    std::vector<Rect> tooMany(513);require(!queueEntrance(context,0,0,&viewport,5,&tooMany));
    tooMany.resize(512);for(int i=0;i<16;++i)require(queueEntrance(context,0,0,&viewport,5,&tooMany));
    require(!queueEntrance(context,0,0,&viewport,5,&tooMany));entranceCount=entranceClipCount=0;
    require(!queueEntrance(context,0,0,&viewport,3,nullptr) && !queueEntrance(context,0,0,&viewport,4,nullptr));
    require(!queueEntrance(nullptr,0,0,&viewport,5,nullptr));

    // Fallback still draws the original cell; exceptions unwind renderer state.
    require(queueEntrance(context,0,0,&viewport,5,nullptr));nativePalette=nullptr;
    entranceExpectedRGB=palette[1];entranceUploads=0;drawEntrances();require(entranceUploads==0);
    nativePalette=palette.data();put(memory,0x11c8b8,99u);
    require(queueEntrance(context,0,0,&viewport,5,nullptr));drawEntrances();require(entranceUploads==0);
    put(memory,0x11c8b8,0u);entranceExpectedRGB=0xffe07038;entranceThrow=true;
    require(queueEntrance(context,0,0,&viewport,5,nullptr));bool caught=false;
    try {drawEntrances();}catch(...){caught=true;}
    require(caught && entranceTestPalette==palette && entranceCount==0 && entranceAlpha==-1 && !terrainClips);
    put(memory,0x11c8b8,1u);entranceExpectedRGB=palette[1];entranceExpectedAlpha=224;
    require(queueEntrance(context,0,0,&viewport,1,nullptr));caught=false;
    try {drawEntrances();}catch(...){caught=true;}
    require(caught && batchColor==128 && entranceAlpha==-1 && !terrainClips);entranceThrow=false;

    checkContext="hybrid entrances use real mask clipping in campaign and endgame, without changing other details";
    std::istringstream definitions(artworkFixture());require(hybridArtwork.load(definitions));
    std::vector<DWORD> frame(8),file(406);frame[1]=20;frame[2]=20;file[5]=400;
    for(unsigned i=0;i<400;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    context[13]=reinterpret_cast<DWORD>(file.data());
    put(memory,0xf16b0,10);put(memory,0x11c1f8,0);put(memory,0x11c1fc,0);
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    Transform t{10,0,0};Mask mask(.25);mask.revealAround(inverse({20,30},t),20);explored=&mask;
    StyledState entranceFloor;entranceFloor.floorCells=1;styledCurrent=&entranceFloor;
    enabled=maskActive=styledActive=inPass=haveViewport=true;activeStyle=MapStyle::Hybrid;
    nativeTownActive=false;
    entranceExpectedAlpha=255;entranceExpectedRGB=0xffe07038;
    for(LONG level:{2L,92L,203L}) {
        observedLevel=level;++gameSerial;context[0]=308;entranceTestDraws=0;
        cellHook(context,10,40,&viewport,5);
        require(entranceCount==1 && !entranceCells[0].clips.empty() && entranceTestDraws==0);
        for(const auto& clip:entranceCells[0].clips)for(int y=clip.top;y<clip.bottom;++y)for(int x=clip.left;x<clip.right;++x)
            require(mask.contains(inverse({x+.5,y+.5},t)));
        drawEntrances();require(entranceTestDraws>0 && entranceTestPalette==palette);
    }
    // Waypoints/ordinary details bypass the entrance queue, and hidden exits stay hidden.
    originalCell=captureCell;context[0]=307;auto forwarded=forwardedCells;
    cellHook(context,10,40,&viewport,5);require(entranceCount==0 && forwardedCells==forwarded+1);
    context[0]=308;Mask hidden(.25);explored=&hidden;++gameSerial;
    cellHook(context,10,40,&viewport,5);require(entranceCount==0 && forwardedCells==forwarded+1);
    styledCurrent=nullptr;explored=&emptyMask;enabled=true;maskActive=styledActive=inPass=haveViewport=false;activeStyle=MapStyle::Styled;
    entrancePaletteDraw=nullptr;nativePalette=nullptr;client=nullptr;styledColor=nullptr;
    std::cout<<"PASS: entrance classification, palette signatures, +75% capped gain, both views, owned clips/UVs, bounded grouping and exception/fallback restoration\n";
}
