static std::vector<DWORD> styleFrames;
static Transform styleTransform{};
static void __stdcall captureStyleCell(void* context,int x,int y,NativeRect*,int) {
    styleFrames.push_back(read<DWORD>(context));
    if(activeStyle==MapStyle::Original){require(!terrainClips);return;}
    Rect bounds{};require(frameBounds(context,x,y,&bounds));
    auto check=[&](Rect r){for(int py=r.top;py<r.bottom;++py)for(int px=r.left;px<r.right;++px)
        require(explored->contains(inverse({px+.5,py+.5},styleTransform)));};
    if(terrainClips)for(auto clip:*terrainClips)check(clip);else check(bounds);
}
static void testMapStyles() {
    checkContext="group style persistence, legacy migration and thickness bounds";
    namespace ui=exploration::boundary_menu;
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"ems",0,file)!=0);
    const auto oldPath=settingsPath;settingsPath=file;
    require(WritePrivateProfileStringA("Automap","BoundaryColor","cyan",file)!=0);
    require(WritePrivateProfileStringA("Automap","WallColor","white",file)!=0);
    require(WritePrivateProfileStringA("Automap","OverlayOpacity","63",file)!=0);
    require(WritePrivateProfileStringA("Automap","MapStyle","native",file)!=0);
    loadAppearanceSettings(file);require(campaignStyle==MapStyle::Original && mapsStyle==MapStyle::Original);
    for(bool maps:{true,false}) {
        ui::editingMaps=maps;const auto other=maps?campaignStyle:mapsStyle;
        for(unsigned choice=0;choice<4;++choice) {
            require(selectMapStyle(choice));loadAppearanceSettings(file);
            require(currentMapStyle()==choice && (maps?mapsStyle:campaignStyle)==menuStyles[choice]);
            require((maps?campaignStyle:mapsStyle)==other);
            require(currentBoundaryColor()==exploration::BoundaryColor::Teal && currentWallColor()==exploration::BoundaryColor::White);
        }
    }
    for(const char* width:{"0.5","1","1.5","2"}) {
        require(WritePrivateProfileStringA("Automap","BoundaryThickness",width,file)!=0);loadAppearanceSettings(file);
        require(boundaryThickness==atof(width) && boundaryBandWidth==int(lround(12*atof(width))));
    }
    for(const char* width:{"0","-1","99","NaN","Inf","1.2bad",""}) {
        require(WritePrivateProfileStringA("Automap","BoundaryThickness",width,file)!=0);loadAppearanceSettings(file);
        require(boundaryThickness==1 && boundaryBandWidth==12);
    }
    ui::editingMaps=true;ui::selectStyle=selectMapStyle;ui::currentStyle=currentMapStyle;
    ui::Entry* active=ui::stylingEntries.data();ui::Menu* descriptor=&ui::stylingMenu;DWORD selected=ui::styleRow,last=ui::stylingBackRow;
    ui::activeEntries=&active;ui::activeMenu=&descriptor;ui::selection=&selected;ui::escapeSelection=&last;
    auto& row=ui::stylingEntries[ui::styleRow];
    ui::selectThickness=selectBoundaryThickness;ui::currentThickness=currentBoundaryThickness;
    checkContext="independent thickness cycle, immediate group activation and persistence";
    auto& thicknessEntry=ui::stylingEntries[ui::thicknessRow];
    for(bool maps:{false,true}) {
        ui::editingMaps=maps;activateColors(maps);
        const auto other=maps?campaignThickness:mapsThickness;
        for(unsigned choice:{2u,3u,0u,1u}) {
            require(thicknessEntry.press(&thicknessEntry,nullptr) && !ui::thicknessSaveFailed);
            require(currentBoundaryThickness()==ui::thicknessValues[choice] && boundaryThickness==currentBoundaryThickness());
            require(boundaryBandWidth==int(lround(12*boundaryThickness)) && (maps?campaignThickness:mapsThickness)==other);
            menuTextCalls.clear();ui::drawRow(nullptr,400,int(thicknessEntry.y+ui::stylingMenu.textHeight),1,5,-1);
            require(menuTextCalls.size()==2 && menuTextCalls[0].text==L"Boundary Thickness");
            const std::string key=ui::thicknessKeys[choice];require(menuTextCalls[1].text==std::wstring(key.begin(),key.end()));
            loadAppearanceSettings(file);require(currentBoundaryThickness()==ui::thicknessValues[choice]);
        }
    }
    require(WritePrivateProfileStringA("Campaign","WaterColor","invalid",file)!=0);
    require(WritePrivateProfileStringA("Maps","WaterColor","magenta",file)!=0);
    require(WritePrivateProfileStringA("Campaign","BoundaryThickness","1.25",file)!=0);
    loadAppearanceSettings(file);ui::editingMaps=false;activateColors(false);
    require(waterColor==exploration::BoundaryColor::LightBlue && currentBoundaryThickness()==1.25);
    require(thicknessEntry.press(&thicknessEntry,nullptr) && currentBoundaryThickness()==1.5);
    activateColors(true);require(waterColor==exploration::BoundaryColor::Magenta && waterEdgeColor==0xff00ffe0 && boundaryThickness==1.0);
    settingsPath.clear();const auto beforeWidth=currentBoundaryThickness();
    require(thicknessEntry.press(&thicknessEntry,nullptr) && ui::thicknessSaveFailed && currentBoundaryThickness()==beforeWidth && boundaryThickness==1.0);
    require(!selectBoundaryThickness(4) && !ui::cycleThickness(&ui::stylingEntries[ui::stylingBackRow],nullptr));
    settingsPath=file;ui::thicknessSaveFailed=false;ui::editingMaps=true;
    require(WritePrivateProfileStringA("Maps","WaterColor",nullptr,file)!=0);loadAppearanceSettings(file);
    checkContext="style cycle, save failure and unchanged discovery";
    for(unsigned choice:{0u,1u,2u,3u}) {
        require(row.press(&row,nullptr) && !ui::styleSaveFailed && currentMapStyle()==choice);
        menuTextCalls.clear();ui::drawRow(nullptr,400,int(row.y+ui::stylingMenu.textHeight),1,5,-1);
        require(menuTextCalls.size()==2 && menuTextCalls[0].text==L"Stylization" && menuTextCalls[1].text==ui::styleLabels[choice]);
    }
    settingsPath.clear();const auto before=mapsStyle;
    require(row.press(&row,nullptr) && ui::styleSaveFailed && mapsStyle==before);
    require(!selectMapStyle(4) && !ui::cycleStyle(&ui::stylingEntries[ui::stylingBackRow],nullptr));
    settingsPath=file;require(selectMapStyle(2));ui::styleSaveFailed=false;
    ui::activeEntries=nullptr;ui::activeMenu=nullptr;ui::selection=ui::escapeSelection=nullptr;
    checkContext="style changes preserve discovery and completed geometry; Original bypasses rendering";
    std::vector<unsigned char> memory(0x11c8bc);client=memory.data();campaignLayers=exploration::CampaignLayers{};
    PlayerState player{100,100,203,123456,987654};enabled=true;
    updateForPlayer(player,900000);auto* history=explored;const auto serial=gameSerial,key=lastLevelKey;
    auto& cached=styledLevels[key];cached.floorCells=77;cached.drawing.quads=1;
    require(selectMapStyle(0));player.x=115;updateForPlayer(player,900010);updateStyled(player);
    require(activeStyle==MapStyle::Original && !maskActive && !styledActive && explored==history);
    require(history->contains({148,100}) && cached.floorCells==77 && gameSerial==serial);
    NativeRect viewport{0,100,-1,99};originalCell=captureStyleCell;
    DWORD context[18]{};context[0]=10;inPass=true;styleFrames.clear();
    cellHook(context,0,0,&viewport,5);require(styleFrames==std::vector<DWORD>{10} && entranceCount==0);
    require(selectMapStyle(3));updateForPlayer(player,900020);
    require(maskActive && explored==history && gameSerial==serial && cached.floorCells==77 && history->contains({100,100}));
    require(selectMapStyle(2));updateForPlayer(player,900030);require(explored==history && history->contains({148,100}));

    checkContext="Styled hides terrain and retains native navigation artwork, with exploration clipping";
    std::istringstream table(artworkFixture()+
        "Example\texit\t0\t0\t0\tCave exit\t42\tStairs\t43\tShrine\t310\tEvent marker\t333\n");
    std::istringstream objects("Name\tAutoMap\nShrine\t310\nEvent\t333\nPortal\t301\n");
    require(hybridArtwork.load(table) && hybridArtwork.protectObjects(objects));
    for(DWORD id:{13u,42u,43u,301u,302u,310u,311u,317u,333u})require(hybridArtwork.styledDetail(id));
    for(DWORD id:{10u,11u,12u,16u,283u,289u})require(!hybridArtwork.styledDetail(id));
    std::vector<DWORD> frame(8),art(406);frame[1]=20;frame[2]=20;art[5]=400;
    for(unsigned i=0;i<400;++i)art[6+i]=reinterpret_cast<DWORD>(frame.data());
    context[13]=reinterpret_cast<DWORD>(art.data());
    StyledState drawing;drawing.floorCells=1;styledCurrent=&drawing;originalCell=captureStyleCell;
    entrancePaletteDraw=nullptr;nativePalette=nullptr; // Classification/clipping tested separately from the boost.
    for(int divisor:{10,20})for(LONG level:{2L,92L,203L}) {
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,0);put(memory,0x11c1fc,0);
        put(memory,0xdbc48,100);put(memory,0xdbc4c,100);styleTransform={double(divisor),0,0};
        Mask mask(.25);mask.revealAround(inverse({20,30},styleTransform),30);explored=&mask;++gameSerial;
        activeStyle=MapStyle::Styled;observedLevel=level;
        enabled=maskActive=styledActive=inPass=haveViewport=true;nativeTownActive=false;
        styleFrames.clear();
        for(DWORD id:{10u,11u,12u,16u,283u,289u,13u,42u,43u,301u,302u,310u,311u,317u,333u}) {
            context[0]=id;cellHook(context,10,40,&viewport,5);
        }
        require(styleFrames==std::vector<DWORD>({13,42,43,301,302,310,311,317,333}));
        Mask hidden(.25);explored=&hidden;++gameSerial;styleFrames.clear();
        for(DWORD id:{13u,42u,43u,301u,310u,333u}){context[0]=id;cellHook(context,10,40,&viewport,5);}
        require(styleFrames.empty());
        // A boundary-only result cannot suppress terrain before floors arrive.
        drawing.floorCells=0;explored=&mask;++gameSerial;context[0]=10;cellHook(context,10,40,&viewport,5);
        require(styleFrames==std::vector<DWORD>{10});styleFrames.clear();drawing.floorCells=1;
        // During worker warmup use the existing native fallback, never holes.
        styledActive=false;explored=&mask;++gameSerial;context[0]=10;cellHook(context,10,40,&viewport,5);
        require(styleFrames==std::vector<DWORD>{10});
    }
    require(DeleteFileA(file)!=0);settingsPath=oldPath;client=nullptr;styledCurrent=nullptr;explored=&emptyMask;
    campaignThickness=mapsThickness=1.0;campaignColors.water=mapsColors.water=exploration::BoundaryColor::LightBlue;activateColors(activeMaps);
    campaignStyle=mapsStyle=MapStyle::Hybrid;activeStyle=MapStyle::Styled;
    maskActive=styledActive=inPass=haveViewport=false;gameTablesPending=false;styledLevels.clear();
    std::cout<<"PASS: Original/Native/Hybrid/Styled menu cycle and persistence, navigation artwork, terrain suppression, mask/history/cache preservation and safe fallback\n";
}
static void testStationaryThicknessUpdates() {
    checkContext="thickness rebuilds without movement while retaining discovery and completed walls";
    const auto oldMaps=mapsThickness,oldCampaign=campaignThickness;
    styled_map::Worker worker;styledWorker=&worker;styledArray=captureArray;styledColor=captureColor;
    client=nullptr;nativeTownActive=false;maskActive=true;activeStyle=MapStyle::Hybrid;++gameSerial;
    Mask mask(.25);mask.revealAround({50,50},132);explored=&mask;const auto visible=mask.rows();
    PlayerState player{50,50,203,12345,54321};lastLevelKey=203;
    updateStyled(player);auto& state=styledLevels[lastLevelKey];
    std::vector<std::uint16_t> flags(100*100);state.floor.ingest(0,0,100,100,flags);
    styled_map::Level reference;reference.ingest(0,0,100,100,flags);require(reference.connect({50,50}));
    auto boundaryArea=[](const styled_map::Drawing& drawing) {
        double area=0;for(const auto& layer:drawing.layers)for(const auto& q:layer.redQuads)
            area+=std::abs((q.c.x-q.a.x)*(q.c.y-q.a.y));
        return area;
    };
    bool hadDrawing=false;std::vector<styled_map::Stroke> walls;
    for(int width:{24,6,18,12}) {
        mapsThickness=width/12.0;activateColors(true);
        const auto expected=reference.build(mask,nullptr,width,true);const auto expectedArea=boundaryArea(expected);
        require(expectedArea>0);bool received=false;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(std::chrono::steady_clock::now()<deadline) {
            updateStyled(player);
            require(mask.rows()==visible);
            if(hadDrawing) {
                require(state.floorCells>0 && state.drawing.walls.size()==walls.size());
                for(std::size_t i=0;i<walls.size();++i)require(state.drawing.walls[i].a.x==walls[i].a.x &&
                    state.drawing.walls[i].a.y==walls[i].a.y && state.drawing.walls[i].b.x==walls[i].b.x && state.drawing.walls[i].b.y==walls[i].b.y);
            }
            if(state.floorCells>0 && state.queuedWidth==width && std::abs(boundaryArea(state.drawing)-expectedArea)<1e-6) {
                walls=state.drawing.walls;hadDrawing=received=true;break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(received);
    }
    styledWorker=nullptr;styledCurrent=nullptr;styledLevels.clear();preparedFloors.reset();preparedOwner=nullptr;
    mapsThickness=oldMaps;campaignThickness=oldCampaign;activateColors(activeMaps);
    explored=&emptyMask;maskActive=styledActive=false;
    std::cout<<"PASS: stationary boundary thickness matches full-build geometry, retains exploration and walls, and rejects obsolete-width results\n";
}
static void testEmptyAreaPass() {
    checkContext="area-entry boundary before native tiles, both viewports and renderer warmup";
    std::vector<unsigned char> memory(0x135000);client=memory.data();
    put(memory,0xdbc48,100);put(memory,0xdbc4c,100);put(memory,0xf9e14,100);put(memory,0xf9e18,100);
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    preparedFloors.reset();preparedOwner=nullptr;overlayOpacity=100;nativeTownActive=false;
    observedLevel=2;entranceCount=entranceClipCount=0;
    for(int divisor:{10,20})for(int mini:{0,1}) {
        put(memory,0xf16b0,divisor);put(memory,0x11c1f8,0);put(memory,0x11c1fc,0);
        put(memory,0x11c1b0,mini);put(memory,0x11c23c,30);put(memory,0x11c238,20);
        Transform t{double(divisor),0,0};Mask mask(.25);mask.revealAround(inverse({50,40},t),40);
        explored=&mask;++gameSerial;Rect bounds{};require(emptyPassViewport(bounds));
        if(mini)require(bounds.left==22 && bounds.top==5 && bounds.right==71 && bounds.bottom==70);
        else require(bounds.left==0 && bounds.top==0 && bounds.right==100 && bounds.bottom==100);
        styled_map::Level unknown;StyledState drawing;
        drawing.drawing=unknown.build(mask,nullptr,12,true);styledCurrent=&drawing;
        for(auto style:{MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled})for(bool ready:{false,true}) {
            activeStyle=style;maskActive=inPass=true;haveViewport=false;styledActive=ready;
            appearancePositions.clear();appearanceColors.clear();endPass();
            require(haveViewport && !appearancePositions.empty() && !styledBatch && !inPass);
            for(std::size_t i=0;i<appearancePositions.size();i+=2) {
                require(appearancePositions[i]>=bounds.left && appearancePositions[i]<=bounds.right);
                require(appearancePositions[i+1]>=bounds.top && appearancePositions[i+1]<=bounds.bottom);
            }
        }
        // Original has no custom submissions even with cached geometry.
        activeStyle=MapStyle::Original;inPass=true;maskActive=false;haveViewport=false;
        appearancePositions.clear();endPass();require(appearancePositions.empty());
        // Native emits boundary color only; cached gray floor and wall data
        // remain available for a subsequent Hybrid or Styled switch.
        drawing.drawing.layers[0].quads.push_back({inverse({40,30},t),inverse({60,30},t),inverse({60,50},t),inverse({40,50},t)});
        drawing.drawing.walls.push_back({inverse({30,30},t),inverse({60,30},t)});
        activeStyle=MapStyle::Native;appearanceColors.clear();drawStyled(t,bounds);
        for(auto color:appearanceColors)require(std::any_of(drawing.drawing.layers.begin(),drawing.drawing.layers.end(),
            [&](const styled_map::Layer& layer){return color==exploration::boundaryRGBA(boundaryColor,layer.gray,layer.alpha);}));
    }
    client=nullptr;styledCurrent=nullptr;explored=&emptyMask;maskActive=styledActive=inPass=haveViewport=false;
    activeStyle=MapStyle::Styled;
    std::cout<<"PASS: entry boundary with no native tile callbacks, worker warmup, both zooms/viewports, no Original submissions or Native floor/contour overlay\n";
}
