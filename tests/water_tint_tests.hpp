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
    auto artworkStorage=std::make_unique<HybridArtwork>();auto& artwork=*artworkStorage;
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
    for(unsigned id:{18u,19u})require(artwork.blueTerrainTile(id));
    for(unsigned id:{10u,20u,21u,22u,23u,24u,25u,26u,27u,28u,29u,30u,31u,32u,33u,65536u})require(!artwork.blueTerrainTile(id));
    std::istringstream protectedBank("Name\tAutoMap\nShrine\t18\n");
    require(artwork.protectObjects(protectedBank) && !artwork.blueTerrainTile(18));
    std::istringstream protectedLedge("Name\tAutoMap\nShrine\t24\n");
    require(artwork.protectObjects(protectedLedge) && !artwork.blueTerrainTile(24));

    checkContext="hybrid blue shorelines preserve walls, geometry, alpha and other styles";
    auto oldStyle=activeStyle;auto oldWall=wallColor;auto oldOpacity=overlayOpacity;
    const auto oldWater=waterColor;const auto oldWaterEdge=waterEdgeColor;
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    StyledState state;styledCurrent=&state;
    for(int divisor:{10,20}) {
        Transform t{double(divisor),-40,-20};const int w=divisor==10?16:8;
        Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        Mask mask(.25);mask.revealAround({0,0},132);explored=&mask;
        state.drawing.walls={{inverse({-10,w*1.75},stable),inverse({double(w+10),w*1.75},stable)}};
        ++gameSerial;waterTint.select(gameSerial,lastLevelKey,divisor);waterTint.add({0,0,w,w*2});
        for(auto style:{MapStyle::Hybrid,MapStyle::Styled})for(auto color:{BoundaryColor::White,BoundaryColor::Orange})for(unsigned opacity:{80u,100u}) {
            std::vector<float> positions;std::size_t colorBuilds=0;
            for(unsigned i=0;i<=boundaryPresets.size();++i) {
                activeStyle=style;wallColor=color;overlayOpacity=opacity;
                waterColor=static_cast<BoundaryColor>(i);waterEdgeColor=waterRGBA(waterColor,224);
                appearanceColors.clear();appearancePositions.clear();drawHybridWalls(t,{0,0,150,150});
                std::vector<DWORD> expected{overlayColor(0x181818c0),overlayColor(wallRGBA(color,224))};
                if(style==MapStyle::Hybrid)expected.push_back(overlayColor(waterEdgeColor));
                require(appearanceColors==expected && !styledBatch);
                if(i==0){positions=appearancePositions;colorBuilds=waterTint.builds();}
                else require(appearancePositions==positions && waterTint.builds()==colorBuilds);
            }
        }
    }
    waterColor=oldWater;waterEdgeColor=oldWaterEdge;
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
    checkContext="riverbanks register blue terrain while cliffs and stone walls retain wall color";
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
        const bool water=id==18 || id==19;
        require(waterTint.size()==std::size_t(water) && forwardedCells==before+(id<20?1:0));
        // The refresh fallback keeps the same material information and still
        // draws the native cliff only where completed terrain is unavailable.
        state.captureIncomplete=true;state.coverage.reset();
        cellHook(ctx.data(),-sx,w*2-sy,&nativeView,0);
        require(waterTint.size()==std::size_t(water) && forwardedCells==before+(id<20?2:1));
        // A native bank cell must reach the colored drawing, including when
        // its ordinary wall sprite was replaced before frame clipping.
        Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        state.drawing.walls={{inverse({-3,w*1.75},stable),inverse({double(w+3),w*1.75},stable)}};
        wallColor=BoundaryColor::White;overlayOpacity=80;
        appearanceColors.clear();appearancePositions.clear();drawHybridWalls({double(divisor),-40,-20},{0,0,150,150});
        auto expected=std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(wallRGBA(wallColor,224))});
        if(water)expected.push_back(overlayColor(0x50a5dce0));
        require(appearanceColors==expected);
        state.drawing.walls.clear();
    }
    waterTint.select(++gameSerial,lastLevelKey,10);client=nullptr;inPass=haveViewport=maskActive=styledActive=false;
    activeStyle=oldStyle;wallColor=oldWall;overlayOpacity=oldOpacity;styledCurrent=nullptr;explored=&emptyMask;
    std::cout<<"PASS: light-blue Hybrid water edges; disjoint contour coverage, both zooms, material/object protection, cache reuse, bounded storage, session/layer isolation and independent wall colors\n";
}
static void testPixelWaterTint() {
    using namespace exploration;
    checkContext="partial water coverage repairs shorelines without tinting dry objects or bridges";
    for(int divisor:{10,20}) {
        const int w=divisor==10?16:8;
        const Transform stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        WaterTint tint;tint.select(1,202,divisor);
        // Two water strips flank a dry bridge within one floor frame.
        const std::vector<Rect> water{{0,w,w/4,w*2},{w*3/4,w,w,w*2}};
        const Rect frame{-32,-32,-32+w,-32+w*2};
        require(tint.addPixels(frame,1590,water) && tint.size()==1);
        require(!tint.addPixels(frame,1591,{{0,0,w+1,1}}) && tint.size()==1);
        const double y=-32+w*1.75;
        std::vector<styled_map::Stroke> walls{{inverse({-36,y},stable),inverse({double(-28+w),y},stable)}};
        auto parts=tint.prepare(walls);require(parts.size()==5);
        // Independent texel-rectangle oracle also checks exact-once coverage.
        for(int i=0;i<1000;++i) {
            const double x=-36+(w+8)*(i+.5)/1000;unsigned hits=0;bool wet=false;
            for(auto r:water)wet|=x>=r.left-33 && x<r.right-31;
            for(auto part:parts) {
                auto a=project(part.stroke.a,stable),b=project(part.stroke.b,stable);
                if(x>=a.x && x<b.x){++hits;require(part.water==wet);}
            }
            require(hits==1);
        }
        const auto builds=tint.builds();
        for(int i=0;i<100;++i){require(tint.addPixels(frame,1590,water));tint.prepare(walls);}
        require(tint.size()==1 && tint.builds()==builds);
        // An object on the dry part above the water strips stays white.
        auto dry=tint.prepare({{inverse({-32.,-32.+w/2},stable),inverse({-32.+w,-32.+w/2},stable)}});
        require(dry.size()==1 && !dry[0].water);
        // Different floor layers at one placement contribute a union.
        require(tint.addPixels(frame,1591,{{w/4,w,w*3/4,w*2}}));
        parts=tint.prepare(walls);require(parts.size()==3 && parts[1].water);
        tint.select(2,202,divisor);require(!tint.size() && !tint.prepare(walls)[0].water);
    }
    WaterTint bounded;bounded.select(1,202,10);
    std::vector<Rect> limit(WaterTint::pixelRectangleLimit+1,Rect{0,0,1,1});
    require(!bounded.addPixels({0,0,16,32},1590,limit) && bounded.size()==0);
    std::cout<<"PASS: exact water-pixel color splits, dry/bridge exclusions, both zooms, negative bins, layer union, cache reuse and rectangle budget\n";
}

static void testNativeRiverBanks() {
    using namespace exploration;
    using Form=HybridArtwork::RiverBank;
    checkContext="native river banks: water-only definitions and artwork topology";
    const auto fixture=artworkFixture()+
        "1 Wilderness\tfl\t2\t4\t7\tRiver T A\t4\tRiver B A\t5\tBridge T A\t6\tRB_WL _T\t7\n";
    std::istringstream table(fixture);require(hybridArtwork.load(table));
    require(hybridArtwork.riverBank(4)==Form::Top && hybridArtwork.riverBank(5)==Form::Bottom);
    require(hybridArtwork.riverBank(6)==Form::None && hybridArtwork.riverBank(7)==Form::None);
    auto protectedStorage=std::make_unique<HybridArtwork>();auto& protectedArt=*protectedStorage;
    std::istringstream alias(fixture+"Test\tfl\t0\t0\t0\tRiver M A\t4\tRiver T A\t5\t\t-1\t\t-1\n");
    require(protectedArt.load(alias) && protectedArt.riverBank(4)==Form::None && protectedArt.riverBank(5)==Form::None);
    std::istringstream plain(fixture),objects("Name\tAutoMap\nShrine\t4\n");
    require(protectedArt.load(plain) && protectedArt.protectObjects(objects) && protectedArt.riverBank(4)==Form::None);

    const auto oldStyle=activeStyle;const auto oldWall=wallColor;const auto oldOpacity=overlayOpacity;
    std::vector<unsigned char> memory(0x135000);client=memory.data();
    put(memory,0xdbc48,150);put(memory,0xdbc4c,150);put(memory,0x11c1f8,-40);put(memory,0x11c1fc,-20);
    std::vector<DWORD> frame(400),file(30),ctx(18);file[0]=6;file[5]=24;
    for(int i=0;i<24;++i)file[6+i]=reinterpret_cast<DWORD>(frame.data());
    ctx[13]=reinterpret_cast<DWORD>(file.data());
    StyledState state;state.floorCells=1;styledCurrent=&state;
    originalCell=captureCell;styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    NativeRect view{0,150,0,149};passViewport={0,0,150,150};
    observedLevel=2;wallColor=BoundaryColor::Orange;overlayOpacity=80;
    riverBankTraces.clear();
    for(int divisor:{10,20})for(bool town:{false,true})for(auto style:{MapStyle::Original,MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled}) {
        checkContext="native bank survives empty collision mesh and town-only visibility";
        const int w=divisor==10?16:8,h=w*2;
        Transform t{double(divisor),-40,-20},stable{double(divisor),divisor==10?8.0:7.0,divisor==10?-8.0:-3.0};
        const int sx=-40-(divisor==10?8:7),sy=-20-(divisor==10?-8:-3);
        put(memory,0xf16b0,divisor);frame[1]=w;frame[2]=h;
        std::vector<unsigned char> encoded;
        for(int y=0;y<h;++y){encoded.push_back(static_cast<unsigned char>(w));for(int x=0;x<w;++x)encoded.push_back(137);encoded.push_back(128);}
        frame[7]=DWORD(encoded.size());memcpy(frame.data()+8,encoded.data(),encoded.size());
        Silhouette shape;NativeWallTrace trace;require(shape.decode(encoded.data(),encoded.size(),w,h));
        for(auto form:{Form::Top,Form::Bottom}) {
            require(trace.buildRiverBank(shape,w,h,form) && trace.lines.size()==1);
            auto line=trace.lines[0];require(line.second.x-line.first.x==w*.5 && line.second.y-line.first.y==-w*.25);
        }
        Mask mask(.25);if(!town)mask.revealAround({0,0},256);explored=&mask;
        townBoundary=TownBoundary{};townBoundary.ready=town;townBoundary.session=++gameSerial;
        if(town)townBoundary.raster[divisor==20?1:0].revealQuarterRect(-240,-240,240,240,divisor);
        nativeTownActive=town;townInViewport=town;activeStyle=style;
        enabled=inPass=haveViewport=styledActive=true;maskActive=style!=MapStyle::Original;
        state.drawing=styled_map::Drawing{};state.captureIncomplete=false;
        riverTint.select(gameSerial,lastLevelKey,divisor);waterTint.select(gameSerial,lastLevelKey,divisor);
        riverBankCells.clear();waterCore.clear();sewerCasing.clear();sewerCore.clear();
        const auto before=forwardedCells;
        ctx[0]=4;cellHook(ctx.data(),-sx,h-sy,&view,0);
        const auto vertices=waterCore.size();
        cellHook(ctx.data(),-sx,h-sy,&view,0);require(waterCore.size()==vertices); // Duplicate callbacks cannot double the shore.
        ctx[0]=5;cellHook(ctx.data(),-sx+w*2,h-sy,&view,0);
        require(forwardedCells==before+((style==MapStyle::Styled && !town)?0:3));
        const bool traced=style==MapStyle::Hybrid;
        require(!waterCore.empty()==traced && riverTint.size()==(traced?2u:0u));
        if(traced) {
            require(waterCore.size()==8 && sewerCasing.size()==8); // Exactly two shores, no diamond closure or internal seams.
            for(const auto& vertex:waterCore)require(vertex.x>=0 && vertex.x<=150 && vertex.y>=0 && vertex.y<=150);
            appearanceColors.clear();appearancePositions.clear();endPass();
            require(appearanceColors==std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(waterEdgeColor)}));
            // Collision geometry at the river is replaced once. A separate
            // constructed wall retains its chosen color.
            state.drawing.walls={{inverse({0,w*1.75},stable),inverse({double(w),w*1.75},stable)},
                {inverse({-12,0},stable),inverse({-4,0},stable)}};
            if(!town) {
                appearanceColors.clear();appearancePositions.clear();drawHybridWalls(t,passViewport);
                require(appearanceColors==std::vector<DWORD>({overlayColor(0x181818c0),overlayColor(wallRGBA(wallColor,224))}));
            }
            require(waterCore.empty() && riverBankCells.empty());
            inPass=true;ctx[0]=6;const auto bridge=forwardedCells;
            cellHook(ctx.data(),-sx,h-sy,&view,0);
            require(forwardedCells==bridge+1 && waterCore.empty());
            // Unknown artwork retains the native water and cannot suppress a
            // collision edge or add a guessed blue bank.
            file[0]=0;inPass=true;riverTint.select(++gameSerial,lastLevelKey,divisor);
            ctx[0]=4;const auto fallback=forwardedCells;cellHook(ctx.data(),-sx,h-sy,&view,0);
            require(forwardedCells==fallback+1 && waterCore.empty() && riverTint.size()==0);file[0]=6;
            if(!town) {
                checkContext="river casing and core are clipped to explored pixels";
                Mask small(.25);const Point midpoint{w*.25-sx,h-w*.375-sy};
                small.revealAround(inverse(midpoint,t),6);explored=&small;
                riverTint.select(++gameSerial,lastLevelKey,divisor);riverBankCells.clear();
                cellHook(ctx.data(),-sx,h-sy,&view,0);
                require(!waterCore.empty());
                for(const auto* batch:{&waterCore,&sewerCasing})for(std::size_t q=0;q<batch->size();q+=4) {
                    Point center{};for(std::size_t j=0;j<4;++j){center.x+=(*batch)[q+j].x*.25;center.y+=(*batch)[q+j].y*.25;}
                    require(small.contains(inverse({floor(center.x)+.5,floor(center.y)+.5},t)));
                }
                waterCore.clear();sewerCasing.clear();riverBankCells.clear();
                explored=&mask;
                checkContext="river batch limit retains native water";
                sewerCasing.resize(sewerVertexLimit);riverTint.select(++gameSerial,lastLevelKey,divisor);
                const auto budget=forwardedCells;cellHook(ctx.data(),-sx,h-sy,&view,0);
                require(forwardedCells==budget+1 && waterCore.empty() && riverTint.size()==0);
                sewerCasing.clear();
            }
        }
    }
    require(riverBankTraces.size()==4); // Two sprites at two zooms, shared across repeated passes.
    riverBankTraces.clear();riverBankCells.clear();riverTint.select(++gameSerial,0,10);waterTint.select(gameSerial,0,10);
    waterCore.clear();sewerCasing.clear();sewerCore.clear();townBoundary=TownBoundary{};
    client=nullptr;styledCurrent=nullptr;explored=&emptyMask;inPass=haveViewport=maskActive=styledActive=nativeTownActive=false;
    activeStyle=oldStyle;wallColor=oldWall;overlayOpacity=oldOpacity;
    std::cout<<"PASS: native river shores in Hybrid at both zooms; town-only visibility with no collision contours, exact shore count, duplicate rejection, separate wall colors, single contour ownership, native water/bridge preservation and unsupported-artwork fallback\n";
}
