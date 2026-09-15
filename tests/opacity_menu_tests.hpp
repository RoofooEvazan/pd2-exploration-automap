static void testOpacityMenu() {
    using namespace exploration;
    namespace ui=exploration::boundary_menu;
    checkContext="requested defaults and retired shared opacity override";
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"eop",0,file)!=0);
    const auto oldPath=settingsPath;loadAppearanceSettings(file);
    require(mapsStyle==MapStyle::Styled && campaignStyle==MapStyle::Hybrid);
    for(const auto& colors:{mapsColors,campaignColors})
        require(colors.boundary==BoundaryColor::Cyan && colors.wall==BoundaryColor::White && colors.water==BoundaryColor::White);
    require(mapsThickness==1.0 && campaignThickness==1.0 && mapsOpacity==100 && campaignOpacity==100);
    for(const char* old:{"10","30","80","100"}) {
        require(WritePrivateProfileStringA("Automap","OverlayOpacity",old,file)!=0);loadAppearanceSettings(file);
        require(mapsOpacity==100 && campaignOpacity==100 && overlayAlpha(255)==255);
    }
    for(const char* retired:{"vermilion","orange","amber","chartreuse","blue","violet","purple"}) {
        for(auto fallback:{BoundaryColor::Cyan,BoundaryColor::White})require(parseBoundaryColor(retired,fallback)==fallback);
        for(const auto& preset:boundaryPresets)require(!colorKeyMatches(retired,preset.key));
    }
    require(parseWaterColor("light-blue")==BoundaryColor::Cyan);
    checkContext="opacity bounds, malformed settings and group independence";
    for(bool maps:{false,true}) {
        ui::editingMaps=maps;
        for(unsigned opacity=30;opacity<=100;++opacity) {
            const auto value=std::to_string(opacity);
            require(WritePrivateProfileStringA(editingSection(),"StylizationOpacity",value.c_str(),file)!=0);
            loadAppearanceSettings(file);activateColors(maps);
            require(currentStylizationOpacity()==opacity && overlayOpacity==opacity);
            require((maps?campaignOpacity:mapsOpacity)==100);
        }
        for(const auto& value:std::vector<std::pair<const char*,unsigned>>{{"-1",30},{"0",30},{"29",30},{"101",100},
            {"9999999999999999999999",100},{"NaN",100},{"35bad",100},{"35.5",100},{"",100}}) {
            require(WritePrivateProfileStringA(editingSection(),"StylizationOpacity",value.first,file)!=0);
            loadAppearanceSettings(file);require(currentStylizationOpacity()==value.second);
        }
    }
    std::array<ui::Entry,9> source{};source[0].type=0xffffffff;
    ui::Menu original{9,45,34,49,36,0};ui::makeMenu(original,source.data());
    auto active=ui::entries.data();auto menu=&ui::menu;DWORD selected=ui::mapsRow,last=ui::backRow;
    ui::activeEntries=&active;ui::activeMenu=&menu;ui::selection=&selected;ui::escapeSelection=&last;
    ui::selectOpacity=selectStylizationOpacity;ui::currentOpacity=currentStylizationOpacity;
    ui::drawText=captureMenuText;ui::textSize=captureMenuFont;ui::textWidth=captureMenuWidth;ui::originalText=captureMenuCell;
    for(int height:{480,600,720})for(auto descriptor:{ui::stylingMenu,ui::opacityMenu}) {
        const int top=(height-80)/2-int(descriptor.count*descriptor.spacing)/2;
        require(top>=0 && top+int((descriptor.count-1)*descriptor.spacing+descriptor.textHeight)<=height-80);
        require(descriptor.spacing>=descriptor.textHeight);
    }
    for(unsigned i=0;i<ui::stylingEntries.size();++i)ui::stylingEntries[i].y=40+i*ui::stylingMenu.spacing;
    for(unsigned i=0;i<ui::opacityEntries.size();++i) {
        auto& row=ui::opacityEntries[i];row.y=13+i*ui::opacityMenu.spacing;
        require(!row.cell && !row.artwork[0]);for(auto cell:row.switches)require(!cell);
    }
    checkContext="opacity percentage picker, persistence, Back and failure recovery";
    for(bool maps:{false,true}) {
        require(ui::openStyling(maps));activateColors(!maps);
        const auto other=overlayOpacity;
        for(unsigned i=0;i<ui::opacityChoices;++i) {
            auto& row=ui::stylingEntries[ui::opacityRow];require(row.press(&row,nullptr));
            require(active==ui::opacityEntries.data() && menu==&ui::opacityMenu && last==ui::opacityChoices+1);
            require(selected>=1 && selected<=ui::opacityChoices);
            menuTextCalls.clear();ui::drawRow(nullptr,400,int(ui::opacityEntries[0].y+ui::opacityMenu.textHeight),1,5,-1);
            require(menuTextCalls.size()==1 && menuTextCalls[0].font==2 && menuLabel==L"Stylization Opacity");
            auto& choice=ui::opacityEntries[i+1];require(choice.press(&choice,nullptr));
            const unsigned value=30+i*5;
            require(currentStylizationOpacity()==value && overlayOpacity==other && (maps?campaignOpacity:mapsOpacity)==other);
            require(active==ui::stylingEntries.data() && selected==ui::opacityRow && last==ui::stylingBackRow);
            menuTextCalls.clear();ui::drawRow(nullptr,400,int(row.y+ui::stylingMenu.textHeight),1,5,-1);
            require(menuTextCalls.size()==2 && menuTextCalls[0].text==L"Stylization Opacity" && menuTextCalls[1].text==std::to_wstring(value)+L"%");
            require(menuTextCalls[0].x==170 && menuTextCalls[1].x+int(menuTextCalls[1].text.size()*10)==630);
            require(GetPrivateProfileIntA(editingSection(),"StylizationOpacity",0,file)==value);
            loadAppearanceSettings(file);require(currentStylizationOpacity()==value && overlayOpacity==other);
            activateColors(maps);require(overlayAlpha(255)==(255*value+50)/100);activateColors(!maps);
        }
        auto& row=ui::stylingEntries[ui::opacityRow];require(row.press(&row,nullptr));
        const auto before=currentStylizationOpacity();require(ui::opacityEntries.back().press(nullptr,nullptr));
        require(currentStylizationOpacity()==before && active==ui::stylingEntries.data() && selected==ui::opacityRow);
        require(row.press(&row,nullptr));settingsPath.clear();
        require(ui::chooseOpacity(&ui::opacityEntries[1],nullptr) && ui::opacitySaveFailed && currentStylizationOpacity()==before);
        require(active==ui::opacityEntries.data());
        ui::drawRow(nullptr,400,int(ui::opacityEntries[0].y+ui::opacityMenu.textHeight),1,5,-1);
        require(menuLabel==L"Could not save opacity");
        require(!ui::chooseOpacity(&row,nullptr) && !selectStylizationOpacity(29) && !selectStylizationOpacity(101));
        settingsPath=file;require(ui::chooseOpacity(&ui::opacityEntries[ui::opacityChoices],nullptr) && !ui::opacitySaveFailed);
        require(ui::back(nullptr,nullptr) && active==ui::entries.data() && selected==(maps?ui::mapsRow:ui::campaignRow));
    }
    checkContext="full-range opacity changes alpha only, with no second fullscreen multiplier";
    StyledState state;styledCurrent=&state;nativeTownActive=false;
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    for(auto style:{MapStyle::Hybrid,MapStyle::Styled})for(int divisor:{10,20}) {
        activeStyle=style;const Transform transform{double(divisor),100,100};
        state.drawing.walls={{inverse({120,120},transform),inverse({160,120},transform)}};
        Mask mask(.25);mask.revealAround(inverse({140,120},transform),132);explored=&mask;
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);riverTint.select(gameSerial,lastLevelKey,divisor);
        std::vector<float> positions;
        for(unsigned value:{100u,30u,75u}) {
            ui::editingMaps=true;activateColors(true);require(selectStylizationOpacity(value));
            appearanceColors.clear();appearancePositions.clear();drawHybridWalls(transform,{0,0,300,300});
            const DWORD coreAlpha=(224*value+50)/100,casingAlpha=(192*value+50)/100;
            require(appearanceColors==std::vector<DWORD>({0x18181800u|casingAlpha,0xffffff00u|coreAlpha}));
            if(value==100)positions=appearancePositions;else require(appearancePositions==positions);
        }
    }
    require(DeleteFileA(file)!=0);settingsPath=oldPath;
    styledCurrent=nullptr;explored=&emptyMask;ui::activeEntries=nullptr;ui::activeMenu=nullptr;ui::selection=ui::escapeSelection=nullptr;
    std::cout<<"PASS: seven colors, requested defaults, independent 30-100 opacity, retired override ignored, picker layout/navigation/save failures and unchanged contour geometry\n";
}
