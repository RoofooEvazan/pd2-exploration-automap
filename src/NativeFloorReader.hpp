// Reads collision grids from already-loaded rooms in the supported game build.
// Captured flags become owned snapshots for floor shading and wall outlines;
// the background geometry worker never receives these native game pointers.

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
template<class T> T field(const void* p,std::size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p)+offset);
}
inline void* firstRoom(const void* client) {
    __try {
        auto unit=field<void*>(client,0x11bbfc);if(!unit)return nullptr;
        auto act=field<void*>(unit,0x1c);return act?field<void*>(act,0x10):nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER){return nullptr;}
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
        if(!valid || room.level!=level || !wanted(room))continue;
        std::vector<std::uint16_t> grid(std::size_t(room.width)*room.height);
        if(copyGrid(room,grid.data())){visit(room,grid);any=true;}
    }
    return any;
}
}
