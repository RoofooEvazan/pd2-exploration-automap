static void testNativeFade() {
    using exploration::NativeFade;
    checkContext="native Fade modes, center bands and unchanged geometry coverage";
    NativeFade fade{1,false,{400,290}};
    for(auto sample:std::vector<std::pair<Point,unsigned>>{{{400,290},64},{{449,290},64},{{450,290},128},
        {{499,290},128},{{500,290},192},{{541,290},255},{{500,390},255},{{450,340},128}})
        require(fade.alpha(sample.first)==sample.second);
    for(unsigned mode:{0u,2u,3u})for(bool idle:{false,true}) {
        fade.mode=mode;fade.idle=idle;unsigned count=0;
        fade.partition({{100,100},{700,100},{700,500},{100,500}},[&](auto q,unsigned alpha){
            require(q.a.x==100 && q.c.y==500 && alpha==(mode==2?128u:mode==3 && idle?192u:255u));++count;
        });require(count==1);
    }
    fade.mode=1;
    auto area=[](styled_map::Quad q){return std::abs(q.a.x*q.b.y-q.b.x*q.a.y+q.b.x*q.c.y-q.c.x*q.b.y+
        q.c.x*q.d.y-q.d.x*q.c.y+q.d.x*q.a.y-q.a.x*q.d.y)*.5;};
    for(auto quad:std::vector<styled_map::Quad>{{{100,100},{700,100},{700,500},{100,500}},
        {{90,289},{710,289},{710,291},{90,291}},{{399,100},{401,100},{401,500},{399,500}},
        {{150,280},{450,100},{650,300},{350,480}},{{380,280},{420,280},{420,300},{380,300}}}) {
        double sum=0;unsigned pieces=0;std::vector<std::pair<styled_map::Quad,unsigned>> parts;
        fade.partition(quad,[&](auto q,unsigned alpha){
            sum+=area(q);++pieces;parts.push_back({q,alpha});
            Point midpoint{(q.a.x+q.b.x+q.c.x+q.d.x)/4,(q.a.y+q.b.y+q.c.y+q.d.y)/4};
            require(fade.alpha(midpoint)==alpha);
        });
        require(std::abs(sum-area(quad))<1e-5 && pieces>0 && pieces<128);
        // Interior sample points agree with the original bands, without overlaps.
        auto inside=[](styled_map::Quad q,Point p){
            const Point points[]={q.a,q.b,q.c,q.d};bool positive=false,negative=false;
            for(unsigned i=0;i<4;++i){auto a=points[i],b=points[(i+1)%4];
                const auto cross=(b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
                positive|=cross>1e-7;negative|=cross<-1e-7;
            }return !(positive && negative);
        };
        for(double y=100.173;y<500;y+=7.37)for(double x=90.319;x<710;x+=8.13) {
            Point point{x,y};unsigned matches=0;
            for(const auto& part:parts)if(inside(part.first,point)){++matches;require(part.second==fade.alpha(point));}
            require(matches==unsigned(inside(quad,point)));
        }
    }
    double length=0;unsigned linePieces=0;
    fade.line({100,290},{700,290},[&](Point a,Point b,unsigned alpha){
        require(alpha==fade.alpha({(a.x+b.x)*.5,(a.y+b.y)*.5}));length+=hypot(b.x-a.x,b.y-a.y);++linePieces;
    });require(std::abs(length-600)<1e-7 && linePieces>4);

    checkContext="guarded native Fade reads, view bypass, panel position and Auto animation";
    std::vector<unsigned char> memory(0x11c900),unit(0x20);auto* oldClient=client;client=memory.data();
    put(memory,0x5f9c0,BYTE(0x83));put(memory,0x5f9c1,BYTE(0x3d));put(memory,0x5f9c2,client+0x11c1b0);
    put(memory,0x5f9c6,BYTE(1));put(memory,0x5f9c7,BYTE(0xa1));put(memory,0x5f9c8,client+0x11c208);
    put(memory,0x60346,BYTE(0xe8));put(memory,0x60347,int(0x5f9c0-0x6034b));
    put(memory,0x603b3,BYTE(0xa1));put(memory,0x603b4,client+0xf9e18);
    put(memory,0x603c2,BYTE(0xa1));put(memory,0x603c3,client+0xf9e14);
    require(bindNativeFade(client));put(memory,0x5f9c6,BYTE(2));require(!bindNativeFade(client) && !bindNativeFade(nullptr));
    put(memory,0x5f9c6,BYTE(1));nativeFadeBound=true;
    put(memory,0x11c8b8,DWORD(1));put(memory,0x11c208,DWORD(1));
    put(memory,0xf9e14,800);put(memory,0xf9e18,600);put(memory,0xdbc48,800);
    for(int side=0;side<3;++side){put(memory,0x11c414,side);sampleNativeFade();
        require(nativeFade.mode==1 && nativeFade.center.x==(side==1?200:side==2?600:400) && nativeFade.center.y==290);
    }
    put(memory,0x11c208,DWORD(3));put(memory,0x11bbfc,unit.data());
    for(unsigned animation=0;animation<20;++animation){unit[0x18]=BYTE(animation);sampleNativeFade();
        require(nativeFade.alpha({})==((animation==0 || animation==2)?192u:255u));
    }
    for(unsigned mode=0;mode<4;++mode)for(DWORD visible:{0u,1u}) {
        put(memory,0x11c208,mode);put(memory,0x11c8b8,visible);sampleNativeFade();require(nativeFade.mode==mode);
        put(memory,0x11c1b0,DWORD(1));sampleNativeFade();require(nativeFade.mode==0);put(memory,0x11c1b0,DWORD(0));
    }
    put(memory,0x11c208,DWORD(9));sampleNativeFade();require(nativeFade.mode==0);
    put(memory,0x11c208,DWORD(1));put(memory,0xf9e14,0);sampleNativeFade();require(nativeFade.mode==0);
    nativeFadeBound=false;client=oldClient;

    checkContext="Fade applies once to fullscreen wall/water batches and restores native color";
    styledArray=captureAppearanceArray;styledColor=captureColor;originalLine=appearanceNativeLine;
    std::vector<GlideVertex> vertices{{380,280,0xffffffff,1,0,0,0},{420,280,0xffffffff,1,0,0,0},
        {420,300,0xffffffff,1,0,0,0},{380,300,0xffffffff,1,0,0,0}};
    std::vector<const void*> pointers;for(auto& vertex:vertices)pointers.push_back(&vertex);
    overlayOpacity=100;styledBatch=&pointers;styledRestoreColor=0x848484e0;
    for(auto style:{MapStyle::Native,MapStyle::Hybrid,MapStyle::Styled})for(DWORD color:{0xffffffe0u,0x00ffffe0u,0x56606438u}) {
        activeStyle=style;styledBatchColor=color;
        for(unsigned mode=0;mode<4;++mode){nativeFade={mode,true,{400,290}};appearanceColors.clear();appearancePositions.clear();
            floatLineHook(nullptr,nullptr);
            const unsigned alpha=mode==1?64:mode==2?128:mode==3?192:255;
            require(appearanceColors==std::vector<DWORD>{(color&0xffffff00u)|NativeFade::scale(color&255,alpha)});
            require(batchColor==styledRestoreColor && appearancePositions.size()==8);
        }
    }
    checkContext="PD2 fullscreen with automap_on cleared fades terrain but never boundary batches";
    client=memory.data();nativeFadeBound=true;styledBatch=nullptr;
    put(memory,0x11c8b8,DWORD(0));put(memory,0x11c1b0,DWORD(0));
    put(memory,0xf9e14,800);put(memory,0xf9e18,600);put(memory,0x11c414,0);unit[0x18]=0;
    for(unsigned mode=0;mode<4;++mode)for(bool boundary:{false,true}) {
        put(memory,0x11c208,mode);sampleNativeFade();
        for(DWORD color:{0xffffffe0u,0x00ffffe0u,0x56606438u,0x00ffffafu}) {
            appearanceColors.clear();appearancePositions.clear();
            submitStyledVertices(pointers,color,{0,0,800,600},boundary);
            const unsigned alpha=boundary?255:mode==1?64:mode==2?128:mode==3?192:255;
            require(appearanceColors==std::vector<DWORD>{(color&0xffffff00u)|NativeFade::scale(color&255,alpha)});
            require(appearancePositions==std::vector<float>({380,280,420,280,420,300,380,300}));
            require(batchColor==(0x84848400u|(color&255)) && !styledBatch && !styledBatchBoundary);
        }
    }
    checkContext="fractional boundary accents ignore every Fade mode";
    originalFloatLine=captureAppearanceLine;
    for(unsigned mode=0;mode<4;++mode) {
        nativeFade={mode,true,{400,290}};appearanceColors.clear();appearancePositions.clear();
        drawFrontier({380,290},{420,290},0x00ffffff);
        require(appearanceColors==std::vector<DWORD>{0x00ffffff} && appearancePositions==std::vector<float>({380,290,420,290}));
        require(!fractionalFrontier && batchColor==0x848484ff);
    }
    nativeFade={};nativeFadeBound=false;styledBatch=nullptr;client=oldClient;
    std::cout<<"PASS: native Fade modes, center coverage, bounded batching, PD2 fullscreen with cleared visibility flag, native small-map bypass, terrain alpha restoration and unfaded boundaries\n";
}
