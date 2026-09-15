static void testAppearanceDefaults() {
    using namespace exploration;
    checkContext="seven-color defaults and retired opacity settings";
    char temp[MAX_PATH]{},file[MAX_PATH]{};require(GetTempPathA(MAX_PATH,temp)>0 && GetTempFileNameA(temp,"ead",0,file)!=0);
    const auto oldPath=settingsPath;loadAppearanceSettings(file);
    require(mapsStyle==MapStyle::Styled && campaignStyle==MapStyle::Hybrid);
    for(const auto& colors:{mapsColors,campaignColors})
        require(colors.boundary==BoundaryColor::Cyan && colors.wall==BoundaryColor::White && colors.water==BoundaryColor::White);
    require(mapsThickness==1.0 && campaignThickness==1.0 && overlayAlpha(255)==255);
    for(const char* old:{"0","10","30","65","80","100","999","invalid"}) {
        require(WritePrivateProfileStringA("Automap","OverlayOpacity",old,file)!=0);
        for(const char* section:{"Maps","Campaign"})
            require(WritePrivateProfileStringA(section,"StylizationOpacity",old,file)!=0);
        loadAppearanceSettings(file);
        for(bool maps:{false,true}) {
            activateColors(maps);
            require(overlayAlpha(255)==255 && overlayColor(0xffffffe0)==0xffffffe0);
        }
    }
    for(const char* retired:{"vermilion","orange","amber","chartreuse","blue","violet","purple"}) {
        for(auto fallback:{BoundaryColor::Cyan,BoundaryColor::White})require(parseBoundaryColor(retired,fallback)==fallback);
        for(const auto& preset:boundaryPresets)require(!colorKeyMatches(retired,preset.key));
    }
    require(parseWaterColor("light-blue")==BoundaryColor::Cyan);
    require(DeleteFileA(file)!=0);settingsPath=oldPath;
    std::cout<<"PASS: seven-color defaults, independent styles and ignored legacy opacity settings\n";
}
