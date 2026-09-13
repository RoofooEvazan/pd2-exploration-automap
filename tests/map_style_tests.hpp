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
    checkContext="global map style menu, persistence, save failure and color preservation";
    namespace ui=exploration::boundary_menu;
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"ems",0,file)!=0);
    const auto oldPath=settingsPath;settingsPath=file;
    require(WritePrivateProfileStringA("Automap","BoundaryColor","cyan",file)!=0);
    require(WritePrivateProfileStringA("Automap","WallColor","white",file)!=0);
    require(WritePrivateProfileStringA("Automap","OverlayOpacity","63",file)!=0);
    // Legacy keys are still readable; the new single setting overrides both.
    require(WritePrivateProfileStringA("Automap","CampaignStyle","native",file)!=0);
    require(WritePrivateProfileStringA("Automap","MapsStyle","styled",file)!=0);
    loadAppearanceSettings(file);require(campaignStyle==MapStyle::Native && mapsStyle==MapStyle::Styled);
    require(selectMapStyle(1));observedLevel=2;
    ui::selectStyle=selectMapStyle;ui::currentStyle=currentMapStyle;
    ui::selectColor=selectBoundaryColor;ui::currentColor=[]{return boundaryColor;};
    ui::selectWallColor=selectWallColor;ui::currentWallColor=[]{return wallColor;};
    ui::Entry* active=ui::entries.data();ui::Menu* descriptor=&ui::menu;DWORD selected=ui::styleRow,last=ui::backRow;
    ui::activeEntries=&active;ui::activeMenu=&descriptor;ui::selection=&selected;ui::escapeSelection=&last;
    ui::drawText=captureMenuText;ui::textSize=captureMenuFont;ui::textWidth=captureMenuWidth;ui::originalText=captureMenuCell;
    auto& row=ui::entries[ui::styleRow];
    for(unsigned expected:{2u,0u,1u,2u,0u,1u}) {
        require(row.press(&row,nullptr) && !ui::styleSaveFailed && currentMapStyle()==expected);
        for(DWORD level:{2u,92u,109u,132u,203u})require(styleForLevel(level)==menuStyles[expected]);
        campaignStyle=mapsStyle=MapStyle::Native;loadAppearanceSettings(file);
        require(campaignStyle==menuStyles[expected] && mapsStyle==menuStyles[expected]);
        require(boundaryColor==exploration::BoundaryColor::Cyan && wallColor==exploration::BoundaryColor::White && overlayOpacity==63);
        const int baseline=int(row.y+ui::menu.textHeight);menuTextCalls.clear();
        ui::drawRow(nullptr,400,baseline,1,5,-1);
        require(menuTextCalls.size()==2 && menuTextCalls[0].text==L"Map Style" && menuTextCalls[0].x==170);
        require(menuTextCalls[1].text==ui::styleLabels[expected] && menuTextCalls[1].x+int(menuTextCalls[1].text.size()*10)==630);
        require(menuTextCalls[0].font==2 && menuTextCalls[1].font==2 && menuFont==1 && last==ui::backRow);
    }
    settingsPath.clear();const auto before=campaignStyle;
    require(row.press(&row,nullptr) && ui::styleSaveFailed && campaignStyle==before);
    require(!selectMapStyle(3) && !ui::cycleStyle(&ui::entries[ui::backRow],nullptr));
    require(ui::openPicker(false));require(!ui::cycleStyle(&row,nullptr));
    require(ui::back(nullptr,nullptr) && selected==ui::boundaryRow && last==ui::backRow);
    settingsPath=file;require(selectMapStyle(1));ui::styleSaveFailed=false;
    ui::activeEntries=nullptr;ui::activeMenu=nullptr;ui::selection=ui::escapeSelection=nullptr;

    checkContext="style changes preserve discovery and completed geometry; Native bypasses rendering";
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
    require(selectMapStyle(2));updateForPlayer(player,900020);
    require(maskActive && explored==history && gameSerial==serial && cached.floorCells==77 && history->contains({100,100}));
    require(selectMapStyle(1));updateForPlayer(player,900030);require(explored==history && history->contains({148,100}));

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
    StyledState drawing;styledCurrent=&drawing;originalCell=captureStyleCell;
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
        // During worker warmup use the existing native fallback, never holes.
        styledActive=false;explored=&mask;++gameSerial;context[0]=10;cellHook(context,10,40,&viewport,5);
        require(styleFrames==std::vector<DWORD>{10});
    }
    require(DeleteFileA(file)!=0);settingsPath=oldPath;client=nullptr;styledCurrent=nullptr;explored=&emptyMask;
    campaignStyle=mapsStyle=MapStyle::Hybrid;activeStyle=MapStyle::Styled;
    maskActive=styledActive=inPass=haveViewport=false;gameTablesPending=false;styledLevels.clear();
    std::cout<<"PASS: Native/Hybrid/Styled menu cycle and persistence, navigation artwork, terrain suppression, mask/history/cache preservation and safe fallback\n";
}
