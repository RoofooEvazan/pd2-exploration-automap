// Preserve a circular frontier while approximating native discovery reach.
// Native terrain discovery follows the rendered view, not a circular radius.
#pragma once
#include <windows.h>
#include <algorithm>
#include <cmath>

namespace exploration {
inline bool readNativeView(const unsigned char* client,int* width,int* height) {
    __try {
        if(!client)return false;
        // D2Client 1.13c logical world view, NOT the scaled D2GL window size.
        *width=*reinterpret_cast<const int*>(client+0xdbc48);
        *height=*reinterpret_cast<const int*>(client+0xdbc4c);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
class NativeRevealDistance {
    int width_=0,height_=0,radius_=80;
public:
    int width() const{return width_;}
    int height() const{return height_;}
    bool ready() const{return width_!=0;}
    int update(int width,int height) {
        if(width==width_ && height==height_)return radius_;
        // Reject loading garbage; retain the previous calibrated size, or the
        // old 20-subtile circle until valid native dimensions are available.
        if(width<320 || height<200 || width>4096 || height>4096)return radius_;
        constexpr double pi=3.14159265358979323846;
        // Native projection sx=16*(x-y), sy=8*(x+y) maps the centered
        // view to a rotated world rectangle with half-extents a,b. Integrate
        // its radial distance over all angles, giving one stable circle size.
        const double a=width/(32*std::sqrt(2.0)),b=height/(16*std::sqrt(2.0));
        const double corner=std::hypot(a,b);
        const double mean=(2/pi)*(a*std::log((corner+b)/a)+b*std::log((corner+a)/b));
        const auto fineRadius=int(std::lround(mean*4));
        if(fineRadius<1 || fineRadius>1024)return radius_;
        width_=width;height_=height;radius_=fineRadius;return radius_;
    }
};
}
