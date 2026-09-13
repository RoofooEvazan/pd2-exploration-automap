// Loaded-room collision reads. Only owned grid copies reach the worker.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

// Read-only 1.13c room/collision access. All game pointers are short-lived;
// callers keep owned grid copies only. No room load/reveal API is invoked.
namespace floor_reader {
struct Room {
    void* address=nullptr;void* next=nullptr;void* grid=nullptr;
    std::uint32_t level=0;
    int x=0,y=0,width=0,height=0;
};
struct AreaBounds {int x=0,y=0,width=0,height=0;};
template<class T> T field(const void* p,std::size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p)+offset);
}
inline void* firstRoom(const void* client) {
    __try {
        auto unit=field<void*>(client,0x11bbfc);if(!unit)return nullptr;
        auto act=field<void*>(unit,0x1c);return act?field<void*>(act,0x10):nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
}
inline bool currentAreaBounds(const void* client,AreaBounds* bounds) {
    __try {
        auto unit=field<void*>(client,0x11bbfc);if(!unit)return false;
        auto path=field<void*>(unit,0x2c);if(!path)return false;
        auto room=field<void*>(path,0x1c);if(!room)return false;
        auto room2=field<void*>(room,0x10);if(!room2)return false;
        auto level=field<void*>(room2,0x58);if(!level)return false;
        int x=field<int>(level,0x1c),y=field<int>(level,0x20);
        int w=field<int>(level,0x24),h=field<int>(level,0x28);
        if(x<0 || y<0 || w<1 || h<1 || w>512 || h>512 || x+w>13107 || y+h>13107)return false;
        // DRLG level rectangles use tiles; collision/player positions use subtiles.
        *bounds={x*5,y*5,w*5,h*5};
        auto grid=field<void*>(room,0x20);if(!grid)return false;
        int gx=field<int>(grid,0),gy=field<int>(grid,4),gw=field<int>(grid,8),gh=field<int>(grid,12);
        return gw>0 && gh>0 && gw<=512 && gh<=512 && gx>=bounds->x && gy>=bounds->y &&
            gx+gw<=bounds->x+bounds->width && gy+gh<=bounds->y+bounds->height;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool metadata(void* address,Room* room) {
    __try {
        room->address=address;room->next=field<void*>(address,0x7c);
        auto room2=field<void*>(address,0x10);if(!room2)return false;
        auto level=field<void*>(room2,0x58);if(!level)return false;
        room->level=field<std::uint32_t>(level,0x1d0);
        auto grid=field<void*>(address,0x20);if(!grid)return false;
        room->x=field<int>(grid,0);room->y=field<int>(grid,4);
        room->width=field<int>(grid,8);room->height=field<int>(grid,12);
        room->grid=field<void*>(grid,0x20);
        return room->grid && room->level>0 && room->level<10000 &&
            room->x>=0 && room->y>=0 && room->x<65536 && room->y<65536 &&
            room->width>0 && room->width<=512 && room->height>0 && room->height<=512 &&
            room->x+room->width<=65536 && room->y+room->height<=65536;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool copyGrid(const Room& room,std::uint16_t* destination) {
    __try {
        memcpy(destination,room.grid,std::size_t(room.width)*room.height*sizeof(std::uint16_t));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class Wanted,class Visit> bool capture(const void* client,std::uint32_t level,Wanted wanted,Visit visit) {
    void* seen[256]{};int count=0;bool any=false;
    for(auto address=firstRoom(client);address && count<256;) {
        for(int i=0;i<count;++i)if(seen[i]==address)return any;
        seen[count++]=address;Room room;
        bool valid=metadata(address,&room);address=room.next;
        if(!valid || (level && room.level!=level) || !wanted(room))continue;
        std::vector<std::uint16_t> grid(std::size_t(room.width)*room.height);
        if(copyGrid(room,grid.data())){visit(room,grid);any=true;}
    }
    return any;
}
}
