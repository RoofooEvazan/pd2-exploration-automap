// Loaded neighboring walls must be available before the player's area changes.
static void testAreaEntryWalls() {
    checkContext="adjoining wall capture and replacement readiness";
    std::istringstream layers("Id\tAct\tLayer\n1\t0\t0\n5\t0\t0\n6\t0\t0\n7\t0\t0\n9\t0\t1\n76\t2\t0\n");
    require(campaignLayers.load(layers));
    std::vector<unsigned char> memory(0x135000),unit(0x40),act(0x50),roomA(0x80),roomB(0x80);
    std::vector<unsigned char> room2A(0x60),room2B(0x60),levelA(0x1d4),levelB(0x1d4),gridA(0x24),gridB(0x24);
    std::vector<std::uint16_t> flagsA(400),flagsB(400);
    for(int x=0;x<20;++x){flagsB[x]=1;flagsB[19*20+x]=1;}
    DWORD layer=0;client=memory.data();
    put(memory,0x11bbfc,unit.data());put(memory,0x11c1c4,&layer);put(unit,0x1c,act.data());
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);
    put(memory,0xf16b0,10);put(memory,0x11c1f8,-42);put(memory,0x11c1fc,118);
    put(act,0x10,roomA.data());
    put(roomA,0x10,room2A.data());put(roomA,0x20,gridA.data());put(room2A,0x58,levelA.data());
    put(roomB,0x10,room2B.data());put(roomB,0x20,gridB.data());put(room2B,0x58,levelB.data());
    put(levelA,0x1d0,DWORD(6));put(levelB,0x1d0,DWORD(7));
    put(gridA,0,100);put(gridA,4,100);put(gridA,8,20);put(gridA,12,20);put(gridA,0x20,flagsA.data());
    put(gridB,0,120);put(gridB,4,100);put(gridB,8,20);put(gridB,12,20);put(gridB,0x20,flagsB.data());
    styled_map::Worker worker;styledWorker=&worker;styledArray=captureAppearanceArray;styledColor=captureColor;
    originalLine=appearanceNativeLine;enabled=true;campaignStyle=MapStyle::Hybrid;
    sessionIdentity=exploration::SessionIdentity{};InterlockedExchange(&menuEpoch,0);
    PlayerState player{110,110,6,4321,9876,reinterpret_cast<uintptr_t>(act.data()),0};
    require(updateForPlayer(player,300000));const auto key=lastLevelKey;
    auto finish=[&](std::size_t cells,std::size_t rooms) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        do {
            updateStyled(player);
            if(styledActive && wallSnapshotReady() && styledCurrent->floorCells==cells && styledCurrent->drawingRooms==rooms)return;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }while(std::chrono::steady_clock::now()<deadline);
        require(false);
    };
    finish(400,1);auto& state=*styledCurrent;const auto discovered=explored->size();
    // A newly loaded neighbor arrives while the player stays in Black Marsh.
    put(roomA,0x7c,roomB.data());captureStyledRooms(player,key,state);
    require(state.floor.rooms.size()==2 && !state.captureIncomplete && !wallSnapshotReady());
    require(state.floorCells==400 && explored->size()==discovered && observedLevel==6);

    std::istringstream artwork(artworkFixture());require(hybridArtwork.load(artwork));
    std::vector<DWORD> frame(8),file(30),context(18);frame[1]=4;frame[2]=4;file[5]=24;
    for(int i=0;i<24;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    context[0]=10;context[13]=reinterpret_cast<DWORD>(file.data());
    originalCell=captureStyleCell;styleTransform={10,-42,118};NativeRect viewport{0,100,0,99};
    nativeTownActive=false;inPass=haveViewport=true;
    for(auto style:{MapStyle::Hybrid,MapStyle::Styled}) {
        activeStyle=style;styleFrames.clear();cellHook(context.data(),48,52,&viewport,5);
        require(styleFrames==std::vector<DWORD>{10}); // Keep native walls until this room is built.
    }
    state.submitted=0;finish(760,2);
    require(observedLevel==6 && lastLevelKey==key && explored->size()==discovered);
    require(std::any_of(state.drawing.walls.begin(),state.drawing.walls.end(),[](const styled_map::Stroke& w){
        return w.a.y==101 && w.b.y==101 && std::min(w.a.x,w.b.x)<=125 && std::max(w.a.x,w.b.x)>=125;
    }));
    require(std::none_of(state.drawing.walls.begin(),state.drawing.walls.end(),[](const styled_map::Stroke& w){
        return w.a.x==120 && w.b.x==120 && std::min(w.a.y,w.b.y)<110 && std::max(w.a.y,w.b.y)>110;
    })); // No invented wall across the open crossing.
    for(auto style:{MapStyle::Hybrid,MapStyle::Styled}) {
        activeStyle=style;styleFrames.clear();cellHook(context.data(),48,52,&viewport,5);require(styleFrames.empty());
        state.captureIncomplete=true;styleFrames.clear();cellHook(context.data(),48,52,&viewport,5);
        require(styleFrames==std::vector<DWORD>{10});state.captureIncomplete=false;
    }
    // Current-area collision disappears temporarily; old floor count must not
    // hide native terrain. A successful later sample restores readiness.
    put(act,0x10,roomB.data());captureStyledRooms(player,key,state);
    require(state.captureIncomplete && !wallSnapshotReady());
    player.level=7;player.x=122;require(updateForPlayer(player,301000));finish(760,2);
    require(lastLevelKey==key && explored->contains({110,110}));
    put(act,0x10,roomA.data());player.level=6;player.x=110;
    require(updateForPlayer(player,302000));finish(760,2);

    // Only the selected layer/act joins; towns, dungeons and endgame IDs do not.
    require(roomSharesMap(player,key,5) && roomSharesMap(player,key,7));
    for(DWORD other:{1u,9u,76u,203u,999u})require(!roomSharesMap(player,key,other));
    layer=1;require(!roomSharesMap(player,key,7));layer=0;
    player.level=203;const auto endgame=mapKey(player);
    require(roomSharesMap(player,endgame,203) && !roomSharesMap(player,endgame,204) && !roomSharesMap(player,endgame,6));
    player.level=6;
    StyledState unavailable;
    put(gridB,0x20,reinterpret_cast<void*>(1));captureStyledRooms(player,key,unavailable);
    require(unavailable.captureIncomplete && unavailable.floor.rooms.size()==1);
    put(gridB,0x20,flagsB.data());captureStyledRooms(player,key,unavailable);
    require(!unavailable.captureIncomplete && unavailable.floor.rooms.size()==2);
    captureStyledRooms(player,key,unavailable);require(unavailable.floor.rooms.size()==2);
    require(!unavailable.floor.contains(140,100,20,20)); // No forced loading.

    styledWorker=nullptr;styledCurrent=nullptr;styledLevels.clear();preparedFloors.reset();preparedOwner=nullptr;
    client=nullptr;explored=&emptyMask;maskActive=styledActive=inPass=haveViewport=false;
    campaignLayers=exploration::CampaignLayers{};hybridArtwork=exploration::HybridArtwork{};activeStyle=MapStyle::Styled;
    std::cout<<"PASS: walls captured before adjoining-area entry, native fallback while pending/failed, return trips, fixed discovery and layer/act/town/endgame isolation\n";
}
