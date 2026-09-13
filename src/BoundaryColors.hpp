// Draw-time automap colors. Geometry and alpha stay unchanged.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace exploration {
enum class BoundaryColor : unsigned {
    Red, NeonGreen, Magenta, Cyan, LightBlue,
    Orange, PaleBlue, PaleYellow, PaleGreen, PalePeach, PaleLemon, Gray, White
};
struct BoundaryPreset { const char* key;const wchar_t* label;unsigned r,g,b; };
inline constexpr std::array<BoundaryPreset,13> boundaryPresets{{
    {"red",L"Red",165,48,40},
    {"neon-green",L"Neon Green",30,220,15},
    {"magenta",L"Magenta",220,20,220},
    {"cyan",L"Cyan",0,210,220},
    {"light-blue",L"Light Blue",80,165,220},
    {"orange",L"Orange",0xf8,0x88,0x3c},
    {"pale-blue",L"Pale Blue",0xcc,0xf4,0xf4},
    {"pale-yellow",L"Pale Yellow",0xfc,0xe8,0x74},
    {"pale-green",L"Pale Green",0xc4,0xfc,0xb0},
    {"pale-peach",L"Pale Peach",0xfc,0xe4,0xa4},
    {"pale-lemon",L"Pale Lemon",0xfc,0xfc,0xc4},
    {"gray",L"Gray",0x94,0x94,0x94},
    {"white",L"White",0xff,0xff,0xff}
}};
inline const BoundaryPreset& boundaryPreset(BoundaryColor color) {
    auto i=static_cast<unsigned>(color);return boundaryPresets[i<boundaryPresets.size()?i:0];
}
inline BoundaryColor parseBoundaryColor(std::string_view value,BoundaryColor fallback=BoundaryColor::Red) {
    for(unsigned i=0;i<boundaryPresets.size();++i) {
        std::string_view key=boundaryPresets[i].key;
        if(value.size()!=key.size())continue;
        bool equal=true;
        for(std::size_t j=0;j<key.size();++j) {
            char c=value[j];if(c>='A' && c<='Z')c=char(c-'A'+'a');
            if(c!=key[j]){equal=false;break;}
        }
        if(equal)return static_cast<BoundaryColor>(i);
    }
    return fallback;
}
inline std::uint32_t boundaryRGBA(BoundaryColor color,unsigned shade,unsigned alpha) {
    const auto& p=boundaryPreset(color);
    auto channel=[&](unsigned value){auto scaled=shade*value/100;return scaled>255?255:scaled;};
    return (channel(p.r)<<24)|(channel(p.g)<<16)|(channel(p.b)<<8)|(alpha&255);
}
// Gray uses renderer-specific wall brightness. Other choices
// use their exact RGB values; opacity and the dark contrast casing stay separate.
inline std::uint32_t wallRGBA(BoundaryColor color,unsigned gray,unsigned alpha) {
    if(color==BoundaryColor::Gray)return (gray*0x01010100u)|(alpha&255);
    return boundaryRGBA(color,100,alpha);
}
}
