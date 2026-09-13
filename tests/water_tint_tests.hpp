static void testWaterTint() {
    using namespace exploration;
    checkContext="water tint partitions contours without adding or doubling lines";
    for(int divisor:{10,20}) {
        const int w=divisor==10?16:8;
        const Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        WaterTint tint;tint.select(1,2,divisor);
        require(tint.add({0,0,w,w*2}) && tint.add({0,0,w,w*2}) && tint.size()==1);
        require(!tint.add({0,0,17,34}) && !tint.add({0,0,w,w}));
        const double y=w*1.75;
        std::vector<styled_map::Stroke> walls{{inverse({-10,y},stable),inverse({double(w+10),y},stable)}};
        const auto parts=tint.prepare(walls);
        require(parts.size()==3 && !parts[0].water && parts[1].water && !parts[2].water);
        require(std::abs(project(parts[1].stroke.a,stable).x+1.5)<1e-8);
        require(std::abs(project(parts[1].stroke.b,stable).x-(w+1.5))<1e-8);
        // Sample against the diamond equation, independent of the splitter.
        for(int i=0;i<1000;++i) {
            Point p{-10+(w+20)*(i+.5)/1000,y};unsigned hits=0;
            const bool wet=std::abs(p.x-w*.5)+2*std::abs(p.y-w*1.75)<=w*.5+1.5;
            for(const auto& part:parts) {
                auto a=project(part.stroke.a,stable),b=project(part.stroke.b,stable);
                if(p.x>=a.x && p.x<b.x){++hits;require(part.water==wet);}
            }
            require(hits==1);
        }
        const auto builds=tint.builds();
        for(int i=0;i<100;++i){tint.select(1,2,divisor);require(tint.add({0,0,w,w*2}));tint.prepare(walls);}
        require(tint.builds()==builds);
        require(tint.add({w,0,w*2,w*2}));require(tint.prepare(walls).size()==std::size_t(w==16?2:3) && tint.builds()==builds+1);
        tint.select(1,3,divisor);require(tint.size()==0 && tint.prepare(walls).size()==1 && !tint.prepare(walls)[0].water);
        tint.add({-100,-100,-100+w,-100+w*2});tint.select(2,3,divisor);require(tint.size()==0);
    }
    WaterTint bounded;bounded.select(1,1,10);
    for(std::size_t i=0;i<WaterTint::tileLimit;++i){int x=int(i)*16;require(bounded.add({x,0,x+16,32}));}
    require(!bounded.add({-16,0,0,32}) && bounded.size()==WaterTint::tileLimit && bounded.add({0,0,16,32}));
    HybridArtwork artwork;
    std::istringstream table(artworkFixture()+
        "Test\tfl\t0\t0\t0\tpool island\t20\tRiver A\t21\tRiver stairs\t22\t\t-1\n");
    std::istringstream objects("Name\tAutoMap\nWater shrine\t21\n");
    require(artwork.load(table) && artwork.protectObjects(objects));
    require(artwork.waterTile(12) && !artwork.waterTile(10) && !artwork.waterTile(20) && !artwork.waterTile(21) && !artwork.waterTile(22) && !artwork.waterTile(65536));
    std::istringstream banks(artworkFixture()+
        "Test\tfl\t0\t0\t0\tRB_WL _T\t18\tPD2 RB_WR_B\t19\tC_WR a\t20\tC_WL d\t21\n"
        "Test\tfl\t0\t0\t0\tC_WTLL\t22\tbridgeLt\t23\tStn_WR a\t24\tF_WR\t25\n"
        "Test\tfl\t0\t0\t0\tStn_WL b\t26\tStn_X\t27\tStn_L R E\t28\tStn_U R E\t29\n"
        "Test\tfl\t0\t0\t0\tWR A\t30\tWL A\t31\tWTLL\t32\tStn_WR stairs\t33\n");
    require(artwork.load(banks));
    for(unsigned id:{18u,19u,20u,21u,22u,24u,26u,27u,28u,29u})require(artwork.blueTerrainTile(id));
    for(unsigned id:{10u,23u,25u,30u,31u,32u,33u,65536u})require(!artwork.blueTerrainTile(id));
    std::istringstream protectedBank("Name\tAutoMap\nShrine\t18\n");
    require(artwork.protectObjects(protectedBank) && !artwork.blueTerrainTile(18));
    std::istringstream protectedLedge("Name\tAutoMap\nShrine\t24\n");
    require(artwork.protectObjects(protectedLedge) && !artwork.blueTerrainTile(24));

    checkContext="hybrid blue shorelines preserve walls, geometry, alpha and other styles";
    auto oldStyle=activeStyle;auto oldWall=wallColor;auto oldOpacity=overlayOpacity;
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    StyledState state;styledCurrent=&state;
    for(int divisor:{10,20}) {
        Transform t{double(divisor),-40,-20};const int w=divisor==10?16:8;
        Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        Mask mask(.25);mask.revealAround({0,0},132);explored=&mask;
        state.drawing.walls={{inverse({-10,w*1.75},stable),inverse({double(w+10),w*1.75},stable)}};
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);waterTint.add({0,0,w,w*2});
        for(auto style:{MapStyle::Hybrid,MapStyle::Styled})for(auto color:{BoundaryColor::White,BoundaryColor::Orange})for(unsigned opacity:{80u,100u}) {
            activeStyle=style;wallColor=color;overlayOpacity=opacity;
            appearanceColors.clear();appearancePositions.clear();drawHybridWalls(t,{0,0,150,150});
            std::vector<DWORD> expected{overlayColor(0x181818c0),overlayColor(wallRGBA(color,148,224))};
            if(style==MapStyle::Hybrid)expected.push_back(overlayColor(0x50a5dce0));
            require(appearanceColors==expected && !styledBatch);
        }
    }
    ++gameSerial;waterTint.select(gameSerial,lastLevelKey,10);
    checkContext="native water cells register color coverage only in Hybrid";
    testHybridClassification();
    std::vector<unsigned char> memory(0x135000);client=memory.data();
    put(memory,0xdbc48,150);put(memory,0xdbc4c,150);put(memory,0x11c1f8,-40);put(memory,0x11c1fc,-20);
    std::vector<DWORD> frame(8),file(30),ctx(18);file[5]=24;
    for(int i=0;i<24;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    ctx[0]=12;ctx[13]=reinterpret_cast<DWORD>(file.data());
    state.floorCells=1;state.drawingRooms=0;state.drawing=styled_map::Drawing{};
    styledCurrent=&state;originalCell=captureCell;observedLevel=3;
    NativeRect nativeView{0,150,0,149};
    for(int divisor:{10,20})for(auto style:{MapStyle::Hybrid,MapStyle::Native,MapStyle::Styled,MapStyle::Original}) {
        Transform t{double(divisor),-40,-20};const int w=divisor==10?16:8;
        frame[1]=w;frame[2]=w*2;put(memory,0xf16b0,divisor);
        Mask mask(.25);mask.revealAround({0,0},132);explored=&mask;
        activeStyle=style;enabled=inPass=haveViewport=styledActive=true;
        maskActive=style!=MapStyle::Original;nativeTownActive=false;terrainClips=nullptr;
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);
        const auto before=forwardedCells;
        const int shiftX=int(t.ox)-(divisor==10?8:7),shiftY=int(t.oy)-(divisor==10?-8:-3);
        cellHook(ctx.data(),-shiftX,w*2-shiftY,&nativeView,0);
        require(waterTint.size()==std::size_t(style==MapStyle::Hybrid?1:0));
        require(forwardedCells==before+(style==MapStyle::Styled?0:1));
    }
    checkContext="riverbanks, cliffs and Act 1 grassy banks register blue terrain";
    std::istringstream runtimeBanks(artworkFixture()+
        "Test\tfl\t0\t0\t0\tRB_WL _T\t18\tPD2 RB_WR_B\t19\tC_WR a\t20\tC_WL d\t21\n"
        "Test\tfl\t0\t0\t0\tStn_WR a\t22\tStn_WL b\t23\tStn_X\t16\tStn_L R E\t17\n");
    require(hybridArtwork.load(runtimeBanks));
    for(int divisor:{10,20})for(DWORD id:{16u,17u,18u,19u,20u,21u,22u,23u}) {
        const int w=divisor==10?16:8;
        frame[1]=w;frame[2]=w*2;put(memory,0xf16b0,divisor);ctx[0]=id;
        Mask mask(.25);mask.revealAround({0,0},132);explored=&mask;
        activeStyle=MapStyle::Hybrid;enabled=inPass=haveViewport=styledActive=maskActive=true;
        state.captureIncomplete=false;nativeTownActive=false;terrainClips=nullptr;
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);
        const int sx=-40-(divisor==10?8:7),sy=-20-(divisor==10?-8:-3);
        auto before=forwardedCells;
        cellHook(ctx.data(),-sx,w*2-sy,&nativeView,0);
        require(waterTint.size()==1 && forwardedCells==before+(id<20?1:0));
        // The refresh fallback keeps the same material information and still
        // draws the native cliff only where completed terrain is unavailable.
        state.captureIncomplete=true;state.coverage.reset();
        cellHook(ctx.data(),-sx,w*2-sy,&nativeView,0);
        require(waterTint.size()==1 && forwardedCells==before+(id<20?2:1));
        // A native bank cell must reach the colored drawing, including when
        // its ordinary wall sprite was replaced before frame clipping.
        Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        state.drawing.walls={{inverse({-3,w*1.75},stable),inverse({double(w+3),w*1.75},stable)}};
        wallColor=BoundaryColor::White;overlayOpacity=80;
        appearanceColors.clear();appearancePositions.clear();drawHybridWalls({double(divisor),-40,-20},{0,0,150,150});
        require(appearanceColors==std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(wallRGBA(wallColor,148,224)),overlayColor(0x50a5dce0)}));
        state.drawing.walls.clear();
    }
    waterTint.select(++gameSerial,lastLevelKey,10);client=nullptr;inPass=haveViewport=maskActive=styledActive=false;
    activeStyle=oldStyle;wallColor=oldWall;overlayOpacity=oldOpacity;styledCurrent=nullptr;explored=&emptyMask;
    std::cout<<"PASS: light-blue Hybrid water edges; disjoint contour coverage, both zooms, material/object protection, cache reuse, bounded storage, session/layer isolation and independent wall colors\n";
}
