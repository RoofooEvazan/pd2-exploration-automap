// Owned, bounded metadata for transparent padding in immutable DC6 frames.
// Native texture generation, colors and UV coordinates remain authoritative.
#pragma once
#include "ExplorationMask.hpp"
#include <array>

namespace exploration {
class ArtworkBounds {
public:
    static constexpr std::size_t slots=4096,maxBytes=512*1025;
    struct Key {
        std::uintptr_t file=0,frame=0;
        std::uint32_t index=0,length=0;
        int width=0,height=0;
        bool operator==(const Key& b) const {
            return file==b.file && frame==b.frame && index==b.index && length==b.length && width==b.width && height==b.height;
        }
    };
    // A one-texel margin preserves the support of native bilinear sampling.
    // Empty artwork is valid; malformed/unsupported data requests native fallback.
    static bool decode(const unsigned char* bytes,std::size_t length,int width,int height,Rect& bounds) {
        bounds={};
        if(!bytes || !length || length>maxBytes || width<1 || width>512 || height<1 || height>512)return false;
        Rect found{width,height,0,0};std::size_t pos=0;int x=0,y=height-1;
        while(pos<length && y>=0) {
            unsigned code=bytes[pos++];
            if(code==128){x=0;--y;continue;}
            const int count=code&127;
            if(!count || x+count>width)return false;
            if(code<128) {
                if(std::size_t(count)>length-pos)return false;
                for(int i=0;i<count;++i)if(bytes[pos+i]) {
                    found.left=std::min(found.left,x+i);found.right=std::max(found.right,x+i+1);
                    found.top=std::min(found.top,y);found.bottom=std::max(found.bottom,y+1);
                }
                pos+=count;
            }
            x+=count;
        }
        if(y!=-1 || pos!=length)return false;
        if(found.left<found.right && found.top<found.bottom)
            bounds={std::max(0,found.left-1),std::max(0,found.top-1),std::min(width,found.right+1),std::min(height,found.bottom+1)};
        return true;
    }
private:
    struct Entry {Key key{};Rect bounds{};std::uint32_t epoch=0;bool valid=false;};
    std::array<Entry,slots> entries_{};
    std::array<unsigned char,maxBytes> bytes_{};
    std::uint32_t epoch_=1;
    std::size_t hits_=0,decoded_=0,failures_=0;
public:
    void clear() {if(++epoch_==0){for(auto& entry:entries_)entry.epoch=0;epoch_=1;}}
    template<class Read> bool query(Key key,Read read,Rect& bounds) {
        if(!key.length || key.length>maxBytes || key.width<1 || key.width>512 || key.height<1 || key.height>512)return false;
        auto hash=(key.frame>>4)^key.file^(std::uintptr_t(key.index)*2654435761u);
        auto& entry=entries_[hash&(slots-1)];
        if(entry.epoch==epoch_ && entry.key==key){++hits_;bounds=entry.bounds;return entry.valid;}
        entry.key=key;entry.epoch=epoch_;entry.bounds={};entry.valid=false;
        if(read(bytes_.data()) && decode(bytes_.data(),key.length,key.width,key.height,entry.bounds)) {
            entry.valid=true;++decoded_;
        } else ++failures_;
        bounds=entry.bounds;return entry.valid;
    }
    std::size_t hits() const{return hits_;}
    std::size_t decoded() const{return decoded_;}
    std::size_t failures() const{return failures_;}
};
static_assert(sizeof(ArtworkBounds)<1024*1024,"Artwork metadata stays below one MiB, independent of map growth");
}
