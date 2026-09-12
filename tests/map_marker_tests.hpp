// Synthetic loaded-room snapshots and the actual end-of-pass marker route.
static std::string markerObjectsFixture() {
    return "Id\tSubClass\tAutoMap\n2\t1\t310\n593\t2\t1499\n594\t4\t1499\n611\t0\t1499\n"
        "625\t1\t0\n10\t0\t427\n11\t0\t119\n12\t1\t999999\n";
}
static unsigned markerCalls=0;
static std::vector<unsigned> markerFrames;
static Transform markerTransform{};
static void __stdcall captureMarker(void* context,int x,int y,NativeRect*,int) {
    ++markerCalls;markerFrames.push_back(read<DWORD>(context));Rect bounds{};require(frameBounds(context,x,y,&bounds));
    auto pixels=[&](Rect r){for(int py=r.top;py<r.bottom;++py)for(int px=r.left;px<r.right;++px)
        require(explored->contains(inverse({px+.5,py+.5},markerTransform)));};
    if(terrainClips)for(auto r:*terrainClips)pixels(r);else pixels(bounds);
}
static void testMapMarkers() {
    checkContext="native shrine/event marker tables, loaded units, bounds, visibility and duplicate suppression";
    auto definitions=std::make_unique<exploration::MapMarkerDefinitions>();
    std::istringstream objects(markerObjectsFixture());require(definitions->loadObjects(objects));
    require(definitions->objects()==4 && definitions->frame(2,2)==310 && definitions->frame(2,594)==1499);
    for(unsigned id:{625u,10u,11u,12u,65536u})require(!definitions->frame(2,id));
    require(!definitions->frame(0,2) && definitions->artwork(310) && !definitions->artwork(427));
    std::istringstream stats("hcIdx\tMonStatsEx\n919\teventA\n945\teventB\n12\tordinary\n13\ttower\n"),
        extra("Id\tautomapCel\neventA\t1499\neventB\t1499\nordinary\t0\ntower\t1258\n");
    require(definitions->loadMonsters(stats,extra));require(definitions->monsters()==2);
    require(definitions->frame(1,945)==1499 && !definitions->frame(1,12) && !definitions->frame(1,13));
    std::istringstream bad("bad header\n");require(!definitions->loadObjects(bad));
    require(!definitions->artwork(310) && definitions->artwork(1499));
    std::istringstream good(markerObjectsFixture());require(definitions->loadObjects(good));mapMarkerDefinitions=*definitions;

    std::vector<unsigned char> memory(0x11c210),player(0xec),act(0x50),room(0x80),room2(0x60),level(0x1d4),
        shrine(0xec),shrinePath(0x24),event(0xec),eventPath(0x24);
    client=memory.data();put(memory,0x11bbfc,player.data());put(player,0x1c,act.data());put(act,0x10,room.data());
    put(room,0x10,room2.data());put(room2,0x58,level.data());put(level,0x1d0,202u);put(room,0x74,shrine.data());
    put(shrine,0,2u);put(shrine,4,2u);put(shrine,0x2c,shrinePath.data());put(shrine,0xe8,event.data());
    put(shrinePath,0,room.data());put(shrinePath,4,0);put(shrinePath,8,1600);put(shrinePath,0xc,100u);put(shrinePath,0x10,100u);
    put(event,0,1u);put(event,4,945u);put(event,0x10,1u);put(event,0x2c,eventPath.data());
    put(eventPath,0,110u*65536);put(eventPath,4,100u*65536);put(eventPath,8,160);put(eventPath,12,1680);put(eventPath,0x1c,room.data());
    auto markers=exploration::marker_reader::capture(client,202,*definitions);
    require(markers.size()==2 && markers[0].mapX==1 && markers[0].mapY==157 && markers[1].mapX==17);
    put(event,0xc4,0x20000000u);require(exploration::marker_reader::capture(client,202,*definitions)[1].nativeRegistered);
    put(event,0xc4,0u);
    require(exploration::marker_reader::capture(client,203,*definitions).empty());
    put(event,0x10,12u);require(exploration::marker_reader::capture(client,202,*definitions).size()==1);put(event,0x10,1u);
    put(eventPath,0x1c,static_cast<void*>(nullptr));require(exploration::marker_reader::capture(client,202,*definitions).size()==1);
    put(eventPath,0x1c,room.data());put(shrine,0xe8,shrine.data());
    require(exploration::marker_reader::capture(client,202,*definitions).size()==256); // corrupt cycles bounded
    put(shrine,0xe8,event.data());put(event,0x2c,reinterpret_cast<void*>(1));
    require(exploration::marker_reader::capture(client,202,*definitions).size()==1);put(event,0x2c,eventPath.data());
    activeStyle=MapStyle::Hybrid;enabled=maskActive=true;observedLevel=202;gameSerial+=1;lastLevelKey=202;
    sampleMapMarkers(1000);require(mapMarkers.size()==2);
    put(room,0x74,static_cast<void*>(nullptr));sampleMapMarkers(1100);require(mapMarkers.size()==2);
    sampleMapMarkers(1200);require(mapMarkers.empty());put(room,0x74,shrine.data());
    sampleMapMarkers(1400);require(mapMarkers.size()==2);observedLevel=2;sampleMapMarkers(1401);require(mapMarkers.empty());

    std::vector<DWORD> frame(8),file(1510);frame[1]=10;frame[2]=10;file[5]=1500;
    for(int i=0;i<1500;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    originalCell=captureMarker;put(memory,0xdbc48,300);put(memory,0xdbc4c,300);
    for(int divisor:{10,20})for(auto style:{MapStyle::Hybrid,MapStyle::Styled,MapStyle::Native}) {
        activeStyle=style;observedLevel=202;inPass=maskActive=enabled=haveViewport=true;nativeTownActive=false;
        StyledState state;styledCurrent=&state;styledActive=style!=MapStyle::Native;
        markerTransform={double(divisor),-100,0};put(memory,0xf16b0,divisor);put(memory,0x11c1f8,-100);put(memory,0x11c1fc,0);
        Mask mask(.25);mask.revealAround({100,100},124);explored=&mask;
        mapMarkers=markers;mapMarkers.push_back({1499,200,200,1,317}); // Undiscovered, even if the icon overlaps a known pixel.
        passViewport={0,0,300,300};markerArtworkFile=file.data();nativeMarkers.clear();markerCalls=0;markerFrames.clear();
        drawMapMarkers();require(markerCalls==2 && markerFrames==std::vector<unsigned>({310,1499}));
        drawMapMarkers();require(markerCalls==2); // existing native cells or repeated samples never double draw
        nativeMarkers.clear();mapMarkers[1].nativeRegistered=true;markerCalls=0;markerFrames.clear();
        drawMapMarkers();require(markerCalls==1 && markerFrames[0]==310);mapMarkers[1].nativeRegistered=false;
        nativeMarkers.clear();nativeMarkers.emplace(310,100+10/divisor,1570/divisor);
        markerCalls=0;markerFrames.clear();drawMapMarkers();require(markerCalls==1 && markerFrames[0]==1499);
        for(LONG area:{1,40,75,103,109,132}){observedLevel=area;nativeMarkers.clear();drawMapMarkers();require(markerCalls==1);}
        observedLevel=202;activeStyle=MapStyle::Original;drawMapMarkers();require(markerCalls==1);
        activeStyle=style;nativeMarkers.clear();file[5]=300;drawMapMarkers();require(markerCalls==1);file[5]=1500;
    }
    char* dir=nullptr;std::size_t n=0;require(_dupenv_s(&dir,&n,"PD2_HYBRID_TABLE_DIR")==0);
    std::string directory=dir?dir:"";std::free(dir);
    if(!directory.empty()) {
        std::ifstream objectTable(directory+"/Objects.txt"),statsTable(directory+"/MonStats.txt"),extraTable(directory+"/MonStats2.txt");
        require(definitions->loadObjects(objectTable) && definitions->loadMonsters(statsTable,extraTable));
        require(definitions->frame(2,2)==310 && definitions->frame(2,594)==1499 && definitions->frame(2,611)==1499);
        require(definitions->frame(1,919)==1499 && definitions->frame(1,1182)==1499 && !definitions->frame(2,625));
        std::cout<<"PASS: active shrine/event definitions; objects="<<definitions->objects()<<" event actors="<<definitions->monsters()<<"\n";
    }
    client=nullptr;styledCurrent=nullptr;styledActive=inPass=false;explored=&emptyMask;markerArtworkFile=nullptr;
    mapMarkers.clear();nativeMarkers.clear();mapMarkerDefinitions=exploration::MapMarkerDefinitions{};
    std::cout<<"PASS: shrine/event icons, no quest/exit additions, bounded loaded-unit snapshots, expired events, explored-only drawing, native duplicates, both sizes and styles\n";
}
