// Draw-time automap colors. Geometry and alpha stay unchanged.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace exploration {
enum class BoundaryColor : unsigned {
    Red, Yellow, Green, Cyan, Magenta, White
};
struct BoundaryPreset { const char* key;const wchar_t* label;unsigned r,g,b; };
inline constexpr std::array<BoundaryPreset,6> boundaryPresets{{
    {"red",L"Red",0xff,0x00,0x00},
    {"yellow",L"Yellow",0xff,0xff,0x00},
    {"green",L"Green",0x00,0xff,0x00},
    {"cyan",L"Cyan",0x00,0xff,0xff},
    {"magenta",L"Magenta",0xff,0x00,0xff},
    {"white",L"White",0xff,0xff,0xff}
}};
inline const BoundaryPreset& waterPreset(BoundaryColor color) {
    const auto i=static_cast<unsigned>(color);
    return boundaryPresets[i<boundaryPresets.size()?i:static_cast<unsigned>(BoundaryColor::Cyan)];
}
inline const BoundaryPreset& boundaryPreset(BoundaryColor color) {
    auto i=static_cast<unsigned>(color);return boundaryPresets[i<boundaryPresets.size()?i:static_cast<unsigned>(BoundaryColor::Cyan)];
}
inline bool colorKeyMatches(std::string_view value,std::string_view key) {
    if(value.size()!=key.size())return false;
    for(std::size_t j=0;j<key.size();++j) {
        char c=value[j];if(c>='A' && c<='Z')c=char(c-'A'+'a');
        if(c!=key[j])return false;
    }
    return true;
}
inline BoundaryColor parseBoundaryColor(std::string_view value,BoundaryColor fallback=BoundaryColor::Cyan) {
    auto matches=[&](std::string_view key){return colorKeyMatches(value,key);};
    for(unsigned i=0;i<boundaryPresets.size();++i)
        if(matches(boundaryPresets[i].key))return static_cast<BoundaryColor>(i);
    // Old INIs resolve to current choices; the picker only saves current keys.
    if(matches("neon-green") || matches("pale-green"))return BoundaryColor::Green;
    if(matches("light-blue") || matches("pale-blue"))return BoundaryColor::Cyan;
    if(matches("pale-yellow") || matches("pale-lemon"))return BoundaryColor::Yellow;
    if(matches("gray") || matches("grey"))return BoundaryColor::White;
    return fallback;
}
inline BoundaryColor parseWaterColor(std::string_view value,BoundaryColor fallback=BoundaryColor::Cyan) {
    return parseBoundaryColor(value,fallback);
}
inline std::uint32_t boundaryRGBA(BoundaryColor color,unsigned shade,unsigned alpha) {
    const auto& p=boundaryPreset(color);
    auto channel=[&](unsigned value){auto scaled=shade*value/100;return scaled>255?255:scaled;};
    return (channel(p.r)<<24)|(channel(p.g)<<16)|(channel(p.b)<<8)|(alpha&255);
}
inline std::uint32_t wallRGBA(BoundaryColor color,unsigned alpha) {
    return boundaryRGBA(color,100,alpha);
}
inline std::uint32_t waterRGBA(BoundaryColor color,unsigned alpha) {
    const auto& p=waterPreset(color);
    return (p.r<<24)|(p.g<<16)|(p.b<<8)|(alpha&255);
}
}
