static void testNativeDistance() {
    checkContext="native view average, logical scaling and guarded dimensions";
    exploration::NativeRevealDistance distance;
    require(!distance.ready() && distance.update(0,0)==80);
    // Independent angular integration checks the closed-form mean.
    for(auto dims:{std::pair<int,int>{640,480},{800,600},{1068,600},{1280,720},{1920,1080},{3440,1440},{4096,4096}}) {
        double sum=0;
        for(int i=0;i<100000;++i) {
            double angle=(i+.5)*6.2831853071795864769/100000;
            double x=std::cos(angle),y=std::sin(angle);
            sum+=std::min(dims.first/(2*std::abs(16*(x-y))),dims.second/(2*std::abs(8*(x+y))));
        }
        const auto radius=distance.update(dims.first,dims.second);
        require(radius==int(std::lround(sum/100000*4)) && radius<=1024 && distance.ready());
        require(distance.update(dims.first,dims.second)==radius); // Cached result.
        require(distance.update(-1,INT_MAX)==radius && distance.width()==dims.first && distance.height()==dims.second);
    }
    int small=distance.update(640,480),large=distance.update(1280,960);
    require(std::abs(large-2*small)<=1);
    int w=0,h=0;require(!exploration::readNativeView(nullptr,&w,&h));
    std::vector<unsigned char> memory(0x11c210);client=memory.data();
    put(memory,0xdbc48,1068);put(memory,0xdbc4c,600);
    require(exploration::readNativeView(client,&w,&h) && w==1068 && h==600);

    checkContext="native average retains a circle, quarter-cell movement, resize and town persistence";
    nativeReveal=true;nativeDistance={};revealRadius=80;enabled=true;
    const auto campaignBefore=campaignStyle,mapsBefore=mapsStyle;
    campaignStyle=MapStyle::Native;mapsStyle=MapStyle::Styled;
    PlayerState p{100.125,100.125,202,876543,876543};
    updateForPlayer(p,500000);const auto radius=revealRadius;
    require(radius==distance.update(1068,600) && radius>80 && maskActive);
    Mask reference(.25);reference.revealAround({p.x,p.y},radius);
    require(explored->rows()==reference.rows()); // No tile footprints/jagged tile tracing.
    auto radiusWorld=radius*.25;
    require(explored->contains({p.x+radiusWorld,p.y}) && !explored->contains({p.x+radiusWorld+.25,p.y}));
    require(!explored->contains({p.x+radiusWorld,p.y+radiusWorld}));
    p.x+=.25;updateForPlayer(p,500010);reference.revealAround({p.x,p.y},radius);
    require(explored->rows()==reference.rows());
    auto previous=explored->size();put(memory,0xdbc48,1280);put(memory,0xdbc4c,720);
    updateForPlayer(p,500020);require(revealRadius>radius && explored->size()>previous);
    reference.revealAround({p.x,p.y},revealRadius);require(explored->rows()==reference.rows());
    previous=explored->size();put(memory,0xdbc48,640);put(memory,0xdbc4c,480);
    updateForPlayer(p,500030);require(explored->size()==previous); // Smaller view never hides history.
    put(memory,0xdbc48,0);updateForPlayer(p,500040);require(explored->size()==previous && revealRadius==small);
    auto* stored=explored;
    for(DWORD town:{1u,40u,75u,103u,109u}) {p.level=town;updateForPlayer(p,500050);require(!maskActive && explored==&emptyMask);}
    p.level=202;updateForPlayer(p,500060);require(explored==stored && explored->size()==previous);
    p.x=1000;updateForPlayer(p,500070);require(explored->contains({1000,100}) && !explored->contains({500,100}));
    nativeReveal=false;updateForPlayer(p,500080);require(revealRadius==80);
    checkContext="31-subtile setting overrides automatic radius and preserves circular coverage";
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"erd",0,file)!=0);
    require(loadRadiusSetting(file)==0);
    require(WritePrivateProfileStringA("Automap","RevealRadiusSubtiles","31",file)!=0);
    fixedRevealRadius=loadRadiusSetting(file);require(fixedRevealRadius==124);
    nativeReveal=true;p.level=203;p.x=100.125;put(memory,0xdbc48,1068);put(memory,0xdbc4c,600);
    updateForPlayer(p,500090);require(revealRadius==124 && maskActive);
    Mask fixedReference(.25);fixedReference.revealAround({p.x,p.y},124);
    require(explored->rows()==fixedReference.rows());
    require(explored->contains({p.x+31,p.y}) && !explored->contains({p.x+31.25,p.y}));
    put(memory,0xdbc48,1920);put(memory,0xdbc4c,1080);updateForPlayer(p,500100);
    require(revealRadius==124 && explored->rows()==fixedReference.rows());
    p.x+=.25;updateForPlayer(p,500110);fixedReference.revealAround({p.x,p.y},124);
    require(explored->rows()==fixedReference.rows());
    auto* fixedMask=explored;
    for(DWORD town:{1u,40u,75u,103u,109u}) {p.level=town;updateForPlayer(p,500120);require(!maskActive && revealRadius==124);}
    p.level=203;updateForPlayer(p,500130);require(explored==fixedMask && explored->rows()==fixedReference.rows());
    for(auto value:{"0","-1","257","invalid"}) {
        require(WritePrivateProfileStringA("Automap","RevealRadiusSubtiles",value,file)!=0);
        require(loadRadiusSetting(file)==0);
    }
    require(DeleteFileA(file)!=0);fixedRevealRadius=0;nativeReveal=false;revealRadius=80;
    nativeDistance={};client=nullptr;explored=&emptyMask;
    campaignStyle=campaignBefore;mapsStyle=mapsBefore;
    std::cout<<"PASS: averaged native-view circle, both logical dimensions, stable radius, quarter-cell movement, resizing and persistence\n";
}
