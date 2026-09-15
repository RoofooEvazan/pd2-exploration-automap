// Included by the runtime harness to exercise menu ABI guards and callbacks.
static std::wstring menuLabel;
static DWORD menuFont=1;
static unsigned menuForwards=0;
struct MenuTextCall {std::wstring text;int x,y;DWORD font,color;};
static std::vector<MenuTextCall> menuTextCalls;
static void __fastcall captureMenuText(const wchar_t* text,int x,int y,DWORD color,DWORD){
    menuLabel=text;menuTextCalls.push_back({text,x,y,menuFont,color});
}
static DWORD __fastcall captureMenuFont(DWORD font){auto old=menuFont;menuFont=font;return old;}
static void __fastcall captureMenuWidth(const wchar_t* text,DWORD* width,DWORD* file){*width=DWORD(wcslen(text)*10);*file=0;}
static void __fastcall captureMenuCell(void*,int,int,int,int,int){++menuForwards;}
static void testBoundaryMenu() {
    checkContext="boundary presets, native menu guards, persistence and draw-time changes";
    using namespace exploration;
    namespace ui=exploration::boundary_menu;
    const DWORD rgb[]={0xff0000,0xffff00,0x00ff00,0x00ffff,0xff00ff,0xffffff};
    static_assert(std::size(rgb)==boundaryPresets.size(),"palette coverage");
    require(parseBoundaryColor("MAGENTA")==BoundaryColor::Magenta && parseBoundaryColor("invalid")==BoundaryColor::Cyan);
    require(parseBoundaryColor("",BoundaryColor::White)==BoundaryColor::White);
    for(unsigned i=0;i<std::size(rgb);++i) {
        const auto color=static_cast<BoundaryColor>(i);
        require(parseBoundaryColor(boundaryPreset(color).key)==color);
        require(boundaryRGBA(color,100,70)==((rgb[i]<<8)|70));
        require(wallRGBA(color,224)==((rgb[i]<<8)|224));
        for(unsigned shade:{14u,23u,38u,52u,65u,96u,116u})require((boundaryRGBA(color,shade,70)&255)==70);
    }
    require(wallRGBA(BoundaryColor::White,255)==0xffffffff);
    require(parseWaterColor("LIGHT-BLUE")==BoundaryColor::Cyan && waterRGBA(BoundaryColor::Cyan,224)==0x00ffffe0);
    require(parseWaterColor("invalid")==BoundaryColor::Cyan && parseWaterColor("",BoundaryColor::Magenta)==BoundaryColor::Magenta);
    checkContext="independent styling groups and nested color menus";
    const auto oldPath=settingsPath;const auto oldColor=boundaryColor;const auto oldWall=wallColor;
    const auto oldOpacity=overlayOpacity;const auto oldCampaign=campaignStyle,oldMaps=mapsStyle;
    const auto oldCampaignColors=campaignColors,oldMapsColors=mapsColors;const auto oldWater=waterColor;const auto oldWaterEdge=waterEdgeColor;
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"ebc",0,file)!=0);
    require(WritePrivateProfileStringA("Automap","OverlayOpacity","63",file)!=0);loadAppearanceSettings(file);
    for(auto colors:{mapsColors,campaignColors})require(colors.boundary==BoundaryColor::Cyan && colors.wall==BoundaryColor::White && colors.water==BoundaryColor::White);
    checkContext="legacy color names resolve without rewriting preferences";
    const std::pair<const char*,BoundaryColor> aliases[]={
        {"NEON-GREEN",BoundaryColor::Green},{"pale-green",BoundaryColor::Green},
        {"CyAn",BoundaryColor::Cyan},{"light-blue",BoundaryColor::Cyan},{"pale-blue",BoundaryColor::Cyan},
        {"pale-yellow",BoundaryColor::Yellow},{"pale-lemon",BoundaryColor::Yellow},
        {"gray",BoundaryColor::White},{"grey",BoundaryColor::White}
    };
    auto bytes=[&](){std::ifstream input(file,std::ios::binary);require(bool(input));
        return std::string((std::istreambuf_iterator<char>(input)),{});};
    for(auto alias:aliases) {
        require(parseBoundaryColor(alias.first)==alias.second);
        require(WritePrivateProfileStringA("Automap","BoundaryColor",alias.first,file)!=0);
        require(WritePrivateProfileStringA("Automap","WallColor",alias.first,file)!=0);
        require(WritePrivateProfileStringA("Maps","BoundaryColor","Magenta",file)!=0);
        require(WritePrivateProfileStringA("Campaign","WallColor",alias.first,file)!=0);
        const auto before=bytes();loadAppearanceSettings(file);
        require(mapsColors.boundary==BoundaryColor::Magenta && mapsColors.wall==alias.second);
        require(campaignColors.boundary==alias.second && campaignColors.wall==alias.second);
        require(bytes()==before && overlayOpacity==100);
    }
    require(WritePrivateProfileStringA("Automap","BoundaryColor",nullptr,file)!=0);
    require(WritePrivateProfileStringA("Automap","WallColor",nullptr,file)!=0);
    require(WritePrivateProfileStringA("Maps",nullptr,nullptr,file)!=0);
    require(WritePrivateProfileStringA("Campaign",nullptr,nullptr,file)!=0);loadAppearanceSettings(file);
    checkContext="independent styling groups and nested color menus";
    ui::selectColor=selectBoundaryColor;ui::currentColor=currentBoundaryColor;
    ui::selectWallColor=selectWallColor;ui::currentWallColor=currentWallColor;
    ui::selectWaterColor=selectWaterColor;ui::currentWaterColor=currentWaterColor;
    ui::selectStyle=selectMapStyle;ui::currentStyle=currentMapStyle;
    std::array<ui::Entry,9> source{};
    for(unsigned i=0;i<source.size();++i){source[i].type=i?1:0xffffffff;source[i].cell=reinterpret_cast<void*>(0x100+i*4);}
    source[8].type=0;strcpy_s(source[0].artwork,"AutoMapOptions");strcpy_s(source[8].artwork,"SPrevious");
    ui::Menu descriptor{9,45,34,49,36,0};ui::makeMenu(descriptor,source.data());
    require(ui::menu.count==11 && !memcmp(ui::entries.data(),source.data(),8*sizeof(ui::Entry)));
    require(!memcmp(&ui::entries[ui::backRow],&source[8],sizeof(ui::Entry)));
    for(const auto* rows:{ui::stylingEntries.data(),ui::pickerEntries.data()}) {
        const auto count=rows==ui::stylingEntries.data()?ui::stylingEntries.size():ui::pickerEntries.size();
        for(unsigned i=0;i<count;++i){require(!rows[i].cell && !rows[i].artwork[0]);for(auto cell:rows[i].switches)require(!cell);}
    }
    auto active=ui::entries.data();auto activeDescriptor=&ui::menu;DWORD selected=ui::mapsRow,last=ui::backRow;
    ui::activeEntries=&active;ui::activeMenu=&activeDescriptor;ui::selection=&selected;ui::escapeSelection=&last;
    ui::drawText=captureMenuText;ui::textSize=captureMenuFont;ui::textWidth=captureMenuWidth;ui::originalText=captureMenuCell;
    for(int height:{480,600,720})for(const auto& desc:{ui::menu,ui::stylingMenu,ui::pickerMenu}) {
        const int top=(height-80)/2-int(desc.count*desc.spacing)/2;
        require(top>=0 && top+int((desc.count-1)*desc.spacing+desc.textHeight)<=height-80 && desc.spacing>=desc.textHeight);
    }
    for(unsigned i=0;i<ui::entries.size();++i)ui::entries[i].y=40+i*ui::menu.spacing;
    for(unsigned i=0;i<ui::stylingEntries.size();++i)ui::stylingEntries[i].y=40+i*ui::stylingMenu.spacing;
    for(unsigned i=0;i<ui::pickerEntries.size();++i)ui::pickerEntries[i].y=40+i*ui::pickerMenu.spacing;
    for(bool maps:{true,false}) {
        auto& group=ui::entries[maps?ui::mapsRow:ui::campaignRow];require(group.press(&group,nullptr));
        require(ui::editingMaps==maps && active==ui::stylingEntries.data() && last==ui::stylingBackRow);
        for(auto target:{ui::ColorTarget::Boundary,ui::ColorTarget::Wall,ui::ColorTarget::Water})
        for(unsigned i=0;i<boundaryPresets.size();++i) {
            const auto other=maps?campaignColors:mapsColors;const auto untouched=editingColors();
            const bool walls=target==ui::ColorTarget::Wall,water=target==ui::ColorTarget::Water;
            const auto rowIndex=water?ui::waterRow:walls?ui::wallRow:ui::boundaryRow;
            auto& row=ui::stylingEntries[rowIndex];require(row.press(&row,nullptr));
            require(active==ui::pickerEntries.data() && last==ui::pickerMenu.count-1);
            const auto& desc=ui::pickerMenu;
            const int top=200-int(desc.count*desc.spacing)/2;
            require(top>=0 && top+int((desc.count-1)*desc.spacing+desc.textHeight)<=400);
            const auto current=ui::pickedCurrent()();require(selected==unsigned(current)+1);
            menuTextCalls.clear();ui::drawRow(nullptr,400,int(ui::pickerEntries[0].y+ui::pickerMenu.textHeight),1,5,-1);
            require(menuTextCalls.size()==1 && menuTextCalls[0].font==2 && menuFont==1);
            require(menuLabel==(water?L"Water Color":walls?L"Wall Color":L"Boundary Color"));
            ui::drawRow(nullptr,400,int(ui::pickerEntries[selected].y+ui::pickerMenu.textHeight),1,5,-1);
            require(menuLabel==std::wstring(ui::pickedPreset(current).label)+L" (Selected)" && menuTextCalls.back().font==0);
            auto& choice=ui::pickerEntries[i+1];require(choice.press(&choice,nullptr));
            require(ui::pickedCurrent()()==static_cast<BoundaryColor>(i));
            require((walls || currentWallColor()==untouched.wall) && (water || currentWaterColor()==untouched.water) &&
                (target==ui::ColorTarget::Boundary || currentBoundaryColor()==untouched.boundary));
            require((maps?campaignColors:mapsColors).boundary==other.boundary && (maps?campaignColors:mapsColors).wall==other.wall && (maps?campaignColors:mapsColors).water==other.water);
            require(active==ui::stylingEntries.data() && selected==rowIndex && last==ui::stylingBackRow);
            char saved[32]{};GetPrivateProfileStringA(maps?"Maps":"Campaign",water?"WaterColor":walls?"WallColor":"BoundaryColor","",saved,32,file);
            require(!strcmp(saved,(water?waterPreset(static_cast<BoundaryColor>(i)):boundaryPresets[i]).key) &&
                (water?parseWaterColor(saved):parseBoundaryColor(saved))==static_cast<BoundaryColor>(i));
            menuTextCalls.clear();const int y=int(row.y+ui::stylingMenu.textHeight);ui::drawRow(nullptr,400,y,1,5,-1);
            require(menuTextCalls.size()==2 && menuTextCalls[0].x==170 && menuTextCalls[1].x+int(menuTextCalls[1].text.size()*10)==630);
            require(menuTextCalls[0].font==2 && menuTextCalls[1].font==2 && menuFont==1);
            const auto before=editingColors();loadAppearanceSettings(file);
            require(editingColors().boundary==before.boundary && editingColors().wall==before.wall && editingColors().water==before.water && overlayOpacity==100);
        }
        require(ui::openPicker(ui::ColorTarget::Wall));require(ui::pickerEntries[last].press(active,nullptr));
        require(active==ui::stylingEntries.data() && selected==ui::wallRow);
        require(ui::stylingEntries[last].press(active,nullptr));
        require(active==ui::entries.data() && selected==(maps?ui::mapsRow:ui::campaignRow) && last==ui::backRow);
    }
    require(ui::openStyling(false));settingsPath.clear();
    for(auto target:{ui::ColorTarget::Boundary,ui::ColorTarget::Wall,ui::ColorTarget::Water}) {
        require(ui::openPicker(target));const auto colorBefore=ui::pickedCurrent()();
        require(ui::choose(&ui::pickerEntries[1],nullptr) && ui::saveFailed && ui::pickedCurrent()()==colorBefore);
        require(!ui::choose(source.data(),nullptr));
        ui::drawRow(nullptr,400,int(ui::pickerEntries[0].y+ui::pickerMenu.textHeight),1,5,-1);require(menuLabel==L"Could not save color");
        require(ui::back(active,nullptr) && selected==ui::pickedRow());
    }
    settingsPath=file;require(ui::back(active,nullptr));
    require(!selectBoundaryColor(static_cast<BoundaryColor>(99)) && !selectWallColor(static_cast<BoundaryColor>(99)) && !selectWaterColor(static_cast<BoundaryColor>(99)));
    ui::drawRow(source[0].cell,400,40,1,5,-1);ui::drawRow(nullptr,400,1,1,5,-1);active=source.data();ui::drawRow(nullptr,400,1,1,5,-1);
    require(menuForwards==3);
    settingsPath=oldPath;boundaryColor=oldColor;wallColor=oldWall;require(DeleteFileA(file)!=0);
    campaignStyle=oldCampaign;mapsStyle=oldMaps;overlayOpacity=oldOpacity;
    campaignColors=oldCampaignColors;mapsColors=oldMapsColors;waterColor=oldWater;waterEdgeColor=oldWaterEdge;
    ui::activeEntries=nullptr;ui::activeMenu=nullptr;ui::selection=ui::escapeSelection=nullptr;ui::saveFailed=false;
    checkContext="menu compatibility guards and active resource transfer";
    std::vector<unsigned char> pd(0x536000),game(0x135000);
    auto header=[](auto& bytes,DWORD stamp) {
        put(bytes,0,WORD(IMAGE_DOS_SIGNATURE));put(bytes,0x3c,DWORD(0x100));
        auto n=reinterpret_cast<IMAGE_NT_HEADERS32*>(bytes.data()+0x100);
        n->Signature=IMAGE_NT_SIGNATURE;n->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;n->FileHeader.TimeDateStamp=stamp;
        n->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;n->OptionalHeader.SizeOfImage=DWORD(bytes.size());
    };
    header(pd,0x6a049b81);header(game,0x4b95ca3e);
    memcpy(pd.data()+0x39da90,&descriptor,sizeof(descriptor));memcpy(pd.data()+0x3a3fb0,source.data(),sizeof(source));
    for(auto offset:{0x22fc79,0x22fe44,0x230410})put(pd,offset,pd.data()+0x39da90);
    for(auto offset:{0x22fc7e,0x22fe3f,0x230416})put(pd,offset,pd.data()+0x3a3fb0);
    game[0x653ae]=0xe8;put(game,0x653af,int(0xd372-0x653b3));
    const unsigned char before[]={0x6a,0x01,0x8b,0xd0,0x53,0xd1,0xfa};memcpy(game.data()+0x653a7,before,sizeof(before));
    game[0x65395]=game[0x65300]=0xa1;
    put(game,0x65396,game.data()+0xdbc48);put(game,0x65301,game.data()+0x11c060);
    put(pd,0x22e7e1,pd.data()+0x3d98e4);put(game,0x65182,game.data()+0x11c058);put(game,0x65232,game.data()+0x11c05c);
    const unsigned char escapeCall[]={0x8b,0x84,0x30,0x14,0x01,0,0,0xff,0xd0};memcpy(pd.data()+0x22e7ef,escapeCall,sizeof(escapeCall));
    require(ui::compatible(pd.data(),game.data()));
    put(game,0x11c060,pd.data()+0x3a3fb0);put(game,0x11c05c,pd.data()+0x39da90);put(game,0x11c058,DWORD(8));
    checkContext="active menu resource transfer";
    ui::bindActive(pd.data(),game.data());
    require(*ui::activeEntries==ui::entries.data() && *ui::activeMenu==&ui::menu && *ui::selection==ui::backRow && *ui::escapeSelection==ui::backRow);
    require(ui::entries[ui::backRow].cell==source[8].cell &&
        !memcmp(ui::entries[ui::backRow].switches,source[8].switches,sizeof(source[8].switches))); // resources unchanged after layout
    require(ui::openStyling(true) && ui::openPicker(ui::ColorTarget::Boundary) && ui::back(nullptr,nullptr) && ui::back(nullptr,nullptr));
    ui::activeEntries=nullptr;ui::activeMenu=nullptr;ui::selection=ui::escapeSelection=nullptr;
    checkContext="menu rejection of changed profile bytes";
    for(auto offset:{0x22fc79,0x22fe44,0x230410,0x22fc7e,0x22fe3f,0x230416,0x39da90,0x3a3fbc,0x22e7e1,0x22e7ef}) {
        pd[offset]^=1;require(!ui::compatible(pd.data(),game.data()));pd[offset]^=1;
    }
    for(auto offset:{0x653ae,0x653af,0x653a7,0x65396,0x65301,0x65182,0x65232}) {
        game[offset]^=1;require(!ui::compatible(pd.data(),game.data()));game[offset]^=1;
    }
    header(pd,0);require(!ui::compatible(pd.data(),game.data()));
    checkContext="boundary colors preserve floor batches and alpha";
    StyledState drawing;
    for(auto& layer:drawing.drawing.layers)layer.quads.push_back({{0,0},{10,0},{10,10},{0,10}});
    drawing.drawing.layers[0].redQuads.push_back({{10,0},{20,0},{20,10},{10,10}});
    styledCurrent=&drawing;styledArray=captureArray;styledColor=captureColor;originalLine=simulateStyledLine;
    preparedFloors.reset();preparedOwner=nullptr;overlayOpacity=100;
    for(int prepared:{0,1}) {
        if(prepared){preparedFloors=styled_map::PreparedFloors::build(drawing.drawing);preparedOwner=&drawing;preparedSerial=gameSerial;}
        for(unsigned i=0;i<boundaryPresets.size();++i) {
            boundaryColor=static_cast<BoundaryColor>(i);arrayCalls=colorCalls=0;
            drawStyled({10,-40,-20},{0,0,100,100});
            require(arrayCalls==8 && colorCalls==16 && !styledBatch);
        }
    }
    boundaryColor=oldColor;styledCurrent=nullptr;preparedFloors.reset();preparedOwner=nullptr;
    std::cout<<"PASS: exact RGB presets, independent saved boundary/wall colors, compact list selection, Back/Escape, resource ownership, save failures and profile rejection\n";
}
static std::vector<DWORD> appearanceColors;
static std::vector<float> appearancePositions;
static void __stdcall captureAppearanceArray(DWORD mode,DWORD count,const void* data) {
    require(mode==5 && count && data);
    appearanceColors.push_back(batchColor);
    auto vertices=static_cast<const GlideVertex* const*>(data);
    for(DWORD i=0;i<count;++i){appearancePositions.push_back(vertices[i]->x);appearancePositions.push_back(vertices[i]->y);}
}
static void __stdcall captureAppearanceLine(const void* a,const void* b) {
    appearanceColors.push_back(batchColor);
    for(auto v:{static_cast<const GlideVertex*>(a),static_cast<const GlideVertex*>(b)}) {
        appearancePositions.push_back(v->x);appearancePositions.push_back(v->y);
    }
}
static void __stdcall appearanceNativeLine(int x1,int y1,int x2,int y2,DWORD palette,DWORD alpha) {
    require(palette==frontierColor);
    captureColor(0x84848400u|alpha); // The profiled native line's palette/blend setup.
    GlideVertex a{float(x1),float(y1),0,1,0,0,0},b{float(x2),float(y2),0,1,0,0,0};
    if(x1==x2 && y1==y2)floatPointHook(&a);else floatLineHook(&a,&b);
}
static void testPersonalizedDrawing() {
    checkContext="draw-time wall colors preserve clipped geometry, shading, native state and alpha";
    using namespace exploration;
    const auto oldWall=wallColor,oldBoundary=boundaryColor;const auto oldOpacity=overlayOpacity;
    const auto oldStyle=activeStyle;
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    originalFloatLine=captureAppearanceLine;
    StyledState drawing;styledCurrent=&drawing;preparedFloors.reset();preparedOwner=nullptr;
    for(int divisor:{10,20}) {
        Transform t{double(divisor),36,106};Rect viewport{0,0,100,100};
        Mask mask(.25);mask.revealAround(inverse({22.5,30.5},t),24);explored=&mask;
        drawing.drawing=styled_map::Drawing{};
        drawing.drawing.walls.push_back({inverse({14,30},t),inverse({27,30},t)});
        wallColor=BoundaryColor::White;overlayOpacity=100;appearanceColors.clear();appearancePositions.clear();
        drawHybridWalls(t,viewport);
        require(appearanceColors==std::vector<DWORD>({0x181818c0,0xffffffe0}));
        const auto geometry=appearancePositions;
        for(unsigned i=0;i<boundaryPresets.size();++i)for(unsigned opacity:{80u,100u}) {
            wallColor=static_cast<BoundaryColor>(i);overlayOpacity=opacity;
            appearanceColors.clear();appearancePositions.clear();drawHybridWalls(t,viewport);
            require(appearanceColors==std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(wallRGBA(wallColor,224))}));
            require(appearancePositions==geometry && batchColor==0x848484e0 && !styledBatch);
            // Sewer walls and water use independent selected colors.
            // Neither setting changes the native fill or contrast casing.
            sewerCasing.assign(testQuad,testQuad+4);sewerCore=sewerCasing;sewerWaterFill=sewerCasing;sewerWater.clear();
            waterCore=sewerCasing;
            inPass=maskActive=styledActive=haveViewport=true;nativeTownActive=false;activeStyle=MapStyle::Hybrid;
            observedLevel=92;passViewport=viewport;markerArtworkFile=nullptr;
            appearanceColors.clear();appearancePositions.clear();endPass();
            require(appearanceColors==std::vector<DWORD>({overlayColor(0x56606438),overlayColor(0x181818c0),
                overlayColor(wallRGBA(wallColor,224)),overlayColor(waterEdgeColor)}));
            require(sewerCore.empty() && sewerCasing.empty() && sewerWaterFill.empty() && waterCore.empty());
            // Styled mode uses the same fractional contour casing and core.
            // Fallback line accents still preserve ordinary and collapsed strokes.
            activeStyle=MapStyle::Styled;appearanceColors.clear();appearancePositions.clear();
            drawStyled(t,viewport);
            require(appearanceColors==std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(wallRGBA(wallColor,224))}));
            require(appearancePositions==geometry && batchColor==0x848484e0 && !fractionalFrontier && !fractionalTint);
            appearanceColors.clear();appearancePositions.clear();
            drawFrontier({10.125,20.25},{10.375,20.375},wallRGBA(wallColor,255));
            require(appearancePositions==std::vector<float>({10.125f,20.25f,10.375f,20.375f}));
            require(appearanceColors==std::vector<DWORD>({overlayColor(wallRGBA(wallColor,255))}));
        }
    }
    wallColor=oldWall;boundaryColor=oldBoundary;overlayOpacity=oldOpacity;activeStyle=oldStyle;
    styledCurrent=nullptr;explored=&emptyMask;inPass=maskActive=styledActive=haveViewport=false;
    originalFloatLine=captureLine;
    std::cout<<"PASS: personalized hybrid/styled/sewer wall and shoreline colors, exact geometry, untouched water fill/casing, both zooms, alpha and line/point state restoration\n";
}
