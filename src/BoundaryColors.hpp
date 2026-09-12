// Draw-time exploration-frontier colors. Geometry and alpha stay unchanged.
#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace exploration {
enum class BoundaryColor : unsigned { Red, NeonGreen, Magenta, Cyan, LightBlue };
struct BoundaryPreset { const char* key;const wchar_t* label;unsigned r,g,b; };
inline constexpr std::array<BoundaryPreset,5> boundaryPresets{{
    {"red",L"Red",165,48,40},
    {"neon-green",L"Neon Green",30,220,15},
    {"magenta",L"Magenta",220,20,220},
    {"cyan",L"Cyan",0,210,220},
    {"light-blue",L"Light Blue",80,165,220}
}};
inline const BoundaryPreset& boundaryPreset(BoundaryColor color) {
    auto i=static_cast<unsigned>(color);return boundaryPresets[i<boundaryPresets.size()?i:0];
}
inline BoundaryColor parseBoundaryColor(std::string_view value) {
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
    return BoundaryColor::Red;
}
inline std::uint32_t boundaryRGBA(BoundaryColor color,unsigned shade,unsigned alpha) {
    const auto& p=boundaryPreset(color);
    auto channel=[&](unsigned value){auto scaled=shade*value/100;return scaled>255?255:scaled;};
    return (channel(p.r)<<24)|(channel(p.g)<<16)|(channel(p.b)<<8)|(alpha&255);
}
}
