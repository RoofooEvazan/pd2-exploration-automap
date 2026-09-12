static void testFixedDistance() {
    static_assert(revealRadius==124 && maskCellSize==0.25,"31-subtile compiled reveal policy");
    checkContext="hardcoded circle ignores legacy INI radius/mode and logical view dimensions";
    const auto campaignBefore=campaignStyle,mapsBefore=mapsStyle;
    const auto colorBefore=boundaryColor;
    const auto opacityBefore=overlayOpacity;
    const auto settingsBefore=settingsPath;
    char temp[MAX_PATH]{},file[MAX_PATH]{};
    require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"erd",0,file)!=0);
    require(WritePrivateProfileStringA("Automap","CampaignStyle","native",file)!=0);
    require(WritePrivateProfileStringA("Automap","MapsStyle","styled",file)!=0);
    require(WritePrivateProfileStringA("Automap","BoundaryColor","cyan",file)!=0);
    require(WritePrivateProfileStringA("Automap","OverlayOpacity","70",file)!=0);
    std::vector<unsigned char> memory(0x11c210);client=memory.data();enabled=true;
    PlayerState p{100.125,100.125,202,876543,876543};
    DWORD now=500000;
    for(const char* value:{static_cast<const char*>(nullptr),"0","20","31","256","999999","-1","invalid"}) {
        require(WritePrivateProfileStringA("Automap","RevealRadiusSubtiles",value,file)!=0);
        for(const char* mode:{"circle","native-average","invalid"}) {
            require(WritePrivateProfileStringA("Automap","RevealMode",mode,file)!=0);
            loadAppearanceSettings(file);
            require(boundaryColor==exploration::BoundaryColor::Cyan && overlayOpacity==70);
            require(campaignStyle==MapStyle::Native && mapsStyle==MapStyle::Styled);
            ++p.id; // Fresh exploration exposes any oversized circle.
            for(auto dims:{std::pair<int,int>{1068,600},{4096,4096},{640,480},{0,INT_MAX}}) {
                put(memory,0xdbc48,dims.first);put(memory,0xdbc4c,dims.second);
                updateForPlayer(p,now+=10);require(maskActive && revealRadius==124);
                Mask reference(.25);reference.revealAround({p.x,p.y},124);
                require(explored->rows()==reference.rows());
                require(explored->contains({p.x+31,p.y}) && !explored->contains({p.x+31.25,p.y}));
                require(!explored->contains({p.x+31,p.y+31}));
            }
        }
    }
    checkContext="fixed circle keeps quarter-cell movement, teleport gaps and town history";
    Mask reference(.25);reference.revealAround({p.x,p.y},124);
    p.x+=.25;updateForPlayer(p,now+=10);reference.revealAround({p.x,p.y},124);
    require(explored->rows()==reference.rows());
    auto* stored=explored;
    for(DWORD town:{1u,40u,75u,103u,109u}) {
        p.level=town;updateForPlayer(p,now+=10);require(!maskActive && explored==&emptyMask);
    }
    p.level=202;updateForPlayer(p,now+=10);require(explored==stored && explored->rows()==reference.rows());
    p.x=1000;updateForPlayer(p,now+=10);reference.revealAround({p.x,p.y},124);
    require(explored->rows()==reference.rows() && !explored->contains({500,100}));
    require(DeleteFileA(file)!=0);
    client=nullptr;explored=&emptyMask;campaignStyle=campaignBefore;mapsStyle=mapsBefore;
    boundaryColor=colorBefore;overlayOpacity=opacityBefore;settingsPath=settingsBefore;
    std::cout<<"PASS: hardcoded 31-subtile circle, ignored legacy INI settings, independent of logical resolution, quarter-cell movement, teleport gaps, towns and preserved appearance settings\n";
}
