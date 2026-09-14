// Draw-time automap colors. Geometry and alpha stay unchanged.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace exploration {
enum class BoundaryColor : unsigned {
    Red, Vermilion, Orange, Amber, Yellow, Chartreuse, Green,
    Teal, Blue, Violet, Purple, Magenta, White
};
struct BoundaryPreset { const char* key;const wchar_t* label;unsigned r,g,b; };
inline constexpr std::array<BoundaryPreset,13> boundaryPresets{{
    {"red",L"Red",0xff,0x00,0x00},
    {"vermilion",L"Vermilion",0xe3,0x42,0x39},
    {"orange",L"Orange",0xff,0xa5,0x00},
    {"amber",L"Amber",0xff,0xbf,0x00},
    {"yellow",L"Yellow",0xff,0xff,0x00},
    {"chartreuse",L"Chartreuse",0x7f,0xff,0x00},
    {"green",L"Green",0x00,0xff,0x00},
    {"teal",L"Teal",0x00,0x80,0x80},
    {"blue",L"Blue",0x00,0x00,0xff},
    {"violet",L"Violet",0x7f,0x00,0xff},
    {"purple",L"Purple",0x80,0x00,0x80},
    {"magenta",L"Magenta",0xff,0x00,0xff},
    {"white",L"White",0xff,0xff,0xff}
}};
inline const BoundaryPreset& boundaryPreset(BoundaryColor color) {
    auto i=static_cast<unsigned>(color);return boundaryPresets[i<boundaryPresets.size()?i:0];
}
inline BoundaryColor parseBoundaryColor(std::string_view value,BoundaryColor fallback=BoundaryColor::Red) {
    auto matches=[&](std::string_view key) {
        if(value.size()!=key.size())return false;
        for(std::size_t j=0;j<key.size();++j) {
            char c=value[j];if(c>='A' && c<='Z')c=char(c-'A'+'a');
            if(c!=key[j])return false;
        }
        return true;
    };
    for(unsigned i=0;i<boundaryPresets.size();++i)
        if(matches(boundaryPresets[i].key))return static_cast<BoundaryColor>(i);
    // Old INIs resolve to current choices; the picker only saves current keys.
    if(matches("neon-green") || matches("pale-green"))return BoundaryColor::Green;
    if(matches("cyan"))return BoundaryColor::Teal;
    if(matches("light-blue") || matches("pale-blue"))return BoundaryColor::Blue;
    if(matches("pale-yellow") || matches("pale-lemon"))return BoundaryColor::Yellow;
    if(matches("pale-peach"))return BoundaryColor::Orange;
    if(matches("gray") || matches("grey"))return BoundaryColor::White;
    return fallback;
}
inline std::uint32_t boundaryRGBA(BoundaryColor color,unsigned shade,unsigned alpha) {
    const auto& p=boundaryPreset(color);
    auto channel=[&](unsigned value){auto scaled=shade*value/100;return scaled>255?255:scaled;};
    return (channel(p.r)<<24)|(channel(p.g)<<16)|(channel(p.b)<<8)|(alpha&255);
}
inline std::uint32_t wallRGBA(BoundaryColor color,unsigned alpha) {
    return boundaryRGBA(color,100,alpha);
}
}
