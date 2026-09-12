// Included by the runtime harness to exercise menu ABI guards and callbacks.
static std::wstring menuLabel;
static DWORD menuFont=1;
static unsigned menuForwards=0;
static void __fastcall captureMenuText(const wchar_t* text,int,int,DWORD,DWORD){menuLabel=text;}
static DWORD __fastcall captureMenuFont(DWORD font){auto old=menuFont;menuFont=font;return old;}
static void __fastcall captureMenuWidth(const wchar_t* text,DWORD* width,DWORD* file){*width=DWORD(wcslen(text)*10);*file=0;}
static void __fastcall captureMenuCell(void*,int,int,int,int,int){++menuForwards;}
static void testBoundaryMenu() {
    checkContext="boundary presets, native menu guards, persistence and draw-time changes";
    using namespace exploration;
    namespace ui=exploration::boundary_menu;
    require(parseBoundaryColor("MAGENTA")==BoundaryColor::Magenta && parseBoundaryColor("invalid")==BoundaryColor::Red);
    require(parseBoundaryColor("")==BoundaryColor::Red);
    for(unsigned shade:{14u,23u,38u,52u,65u,96u,116u}) {
        require(boundaryRGBA(BoundaryColor::Red,shade,70)==((shade*165/100)<<24 | (shade*48/100)<<16 | (shade*40/100)<<8 |70));
        for(unsigned i=0;i<5;++i)require((boundaryRGBA(static_cast<BoundaryColor>(i),shade,70)&255)==70);
    }
    const auto oldPath=settingsPath;const auto oldColor=boundaryColor;
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"ebc",0,file)!=0);
    settingsPath=file;boundaryColor=BoundaryColor::Red;
    require(WritePrivateProfileStringA("Automap","OverlayOpacity","63",file)!=0);
    ui::selectColor=selectBoundaryColor;ui::currentColor=[]{return boundaryColor;};
    std::array<ui::Entry,9> source{};
    for(unsigned i=0;i<source.size();++i){source[i].type=i?1:0xffffffff;source[i].cell=reinterpret_cast<void*>(0x100+i*4);}
    source[8].type=0;strcpy_s(source[0].artwork,"AutoMapOptions");strcpy_s(source[8].artwork,"SPrevious");
    ui::Menu descriptor{9,45,34,49,36,0};ui::makeMenu(descriptor,source.data());
    require(ui::menu.count==10 && !memcmp(ui::entries.data(),source.data(),8*sizeof(ui::Entry)));
    require(!memcmp(&ui::entries[9],&source[8],sizeof(ui::Entry)) && !ui::entries[8].cell && !ui::entries[8].artwork[0]);
    auto active=ui::entries.data();ui::activeEntries=&active;
    ui::drawText=captureMenuText;ui::textSize=captureMenuFont;ui::textWidth=captureMenuWidth;ui::originalText=captureMenuCell;
    ui::entries[8].y=400;
    for(unsigned i=0;i<5;++i) {
        require(ui::press(&ui::entries[8],nullptr));
        require(boundaryColor==static_cast<BoundaryColor>((i+1)%5));
        char saved[32]{};GetPrivateProfileStringA("Automap","BoundaryColor","",saved,32,file);
        require(parseBoundaryColor(saved)==boundaryColor && GetPrivateProfileIntA("Automap","OverlayOpacity",0,file)==63);
        ui::drawRow(nullptr,400,434,1,5,-1);
        require(menuLabel==std::wstring(L"Boundary Color: ")+boundaryPreset(boundaryColor).label && menuFont==1 && !menuForwards);
    }
    ui::drawRow(source[0].cell,400,434,1,5,-1);ui::drawRow(nullptr,400,433,1,5,-1);
    active=source.data();ui::drawRow(nullptr,400,434,1,5,-1);require(menuForwards==3);
    settingsPath.clear();require(ui::press(&ui::entries[8],nullptr) && ui::saveFailed && boundaryColor==BoundaryColor::Red);
    require(!selectBoundaryColor(static_cast<BoundaryColor>(99)));
    settingsPath=oldPath;boundaryColor=oldColor;require(DeleteFileA(file)!=0);
    ui::activeEntries=nullptr;ui::saveFailed=false;
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
    require(ui::compatible(pd.data(),game.data()));
    for(auto offset:{0x22fc79,0x22fe44,0x230410,0x22fc7e,0x22fe3f,0x230416,0x39da90,0x3a3fbc}) {
        pd[offset]^=1;require(!ui::compatible(pd.data(),game.data()));pd[offset]^=1;
    }
    for(auto offset:{0x653ae,0x653af,0x653a7,0x65396,0x65301}) {
        game[offset]^=1;require(!ui::compatible(pd.data(),game.data()));game[offset]^=1;
    }
    header(pd,0);require(!ui::compatible(pd.data(),game.data()));
    StyledState drawing;
    for(auto& layer:drawing.drawing.layers)layer.quads.push_back({{0,0},{10,0},{10,10},{0,10}});
    drawing.drawing.layers[0].redQuads.push_back({{10,0},{20,0},{20,10},{10,10}});
    styledCurrent=&drawing;styledArray=captureArray;styledColor=captureColor;originalLine=simulateStyledLine;
    preparedFloors.reset();preparedOwner=nullptr;overlayOpacity=100;
    for(int prepared:{0,1}) {
        if(prepared){preparedFloors=styled_map::PreparedFloors::build(drawing.drawing);preparedOwner=&drawing;preparedSerial=gameSerial;}
        for(unsigned i=0;i<5;++i) {
            boundaryColor=static_cast<BoundaryColor>(i);arrayCalls=colorCalls=0;
            drawStyled({10,-40,-20},{0,0,100,100});
            require(arrayCalls==8 && colorCalls==16 && !styledBatch);
        }
    }
    boundaryColor=oldColor;styledCurrent=nullptr;preparedFloors.reset();preparedOwner=nullptr;
    std::cout<<"PASS: five colors, unchanged default/alpha/gray geometry, saved settings, menu navigation callbacks and profile rejection\n";
}
