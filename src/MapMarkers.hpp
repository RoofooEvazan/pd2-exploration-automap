// Native shrine/event artwork, sourced from active tables and loaded units.
// No preset traversal, room revealing, game-state writes or additional icons.
#pragma once
#include "NativeFloorReader.hpp"
#include <array>
#include <map>
#include <string>
#include <istream>
#include <algorithm>

namespace exploration {
class MapMarkerDefinitions {
    std::array<std::uint16_t,65536> objects_{},monsters_{};
    std::array<bool,65536> artwork_{};
    static std::vector<std::string> split(const std::string& line) {
        std::vector<std::string> row;std::size_t start=0;
        for(;;){auto end=line.find('\t',start);row.push_back(line.substr(start,end-start));
            if(!row.back().empty() && row.back().back()=='\r')row.back().pop_back();
            if(end==std::string::npos)break;start=end+1;}
        return row;
    }
    static int number(const std::string& text) {
        if(text.empty() || text.size()>5)return -1;
        int n=0;for(char c:text){if(c<'0' || c>'9')return -1;n=n*10+c-'0';}return n<65536?n:-1;
    }
    template<class Visit> static bool table(std::istream& input,std::initializer_list<const char*> names,Visit visit) {
        std::string line;if(!std::getline(input,line) || line.size()>32768)return false;
        const auto header=split(line);std::vector<std::size_t> columns;
        for(auto name:names){auto it=std::find(header.begin(),header.end(),name);
            if(it==header.end())return false;columns.push_back(std::size_t(it-header.begin()));}
        std::size_t bytes=0,rows=0;
        while(std::getline(input,line)) {
            bytes+=line.size();if(line.size()>32768 || bytes>16*1024*1024 || ++rows>100000)return false;
            auto row=split(line);if(row.size()==header.size())visit(row,columns);
        }
        return !input.bad();
    }
    void updateArtwork() {
        artwork_.fill(false);
        for(auto id:objects_)if(id)artwork_[id]=true;
        for(auto id:monsters_)if(id)artwork_[id]=true;
    }
public:
    bool loadObjects(std::istream& input) {
        objects_.fill(0);
        const bool ok=table(input,{"Id","SubClass","AutoMap"},[&](const auto& row,const auto& c){
            const int id=number(row[c[0]]),subclass=number(row[c[1]]),frame=number(row[c[2]]);
            // 1499 is the supported PD2 event symbol. SubClass bit 1 denotes
            // shrines; require a native AutoMap entry (inactive props have none).
            if(id>=0 && frame>0 && frame<32768 && ((subclass>=0 && (subclass&1)) || frame==1499))objects_[id]=std::uint16_t(frame);
        });
        if(!ok)objects_.fill(0);updateArtwork();return ok;
    }
    bool loadMonsters(std::istream& stats,std::istream& extra) {
        monsters_.fill(0);std::map<std::string,int> events;
        bool ok=table(extra,{"Id","automapCel"},[&](const auto& row,const auto& c){
            if(number(row[c[1]])==1499)events[row[c[0]]]=1499;
        });
        if(ok)ok=table(stats,{"hcIdx","MonStatsEx"},[&](const auto& row,const auto& c){
            int id=number(row[c[0]]);auto it=events.find(row[c[1]]);
            if(id>=0 && it!=events.end())monsters_[id]=std::uint16_t(it->second);
        });
        if(!ok)monsters_.fill(0);updateArtwork();return ok;
    }
    unsigned frame(unsigned type,unsigned id) const {
        if(id>=65536)return 0;return type==2?objects_[id]:type==1?monsters_[id]:0;
    }
    bool artwork(unsigned id) const {return id<artwork_.size() && artwork_[id];}
    std::size_t objects() const {return std::count_if(objects_.begin(),objects_.end(),[](auto id){return id!=0;});}
    std::size_t monsters() const {return std::count_if(monsters_.begin(),monsters_.end(),[](auto id){return id!=0;});}
};

struct MapMarker {
    unsigned frame=0;
    double x=0,y=0;
    int mapX=0,mapY=0; // Native cell coordinates before view scaling/panning.
    bool nativeRegistered=false;
};
namespace marker_reader {
using floor_reader::field;
struct Room {void* next=nullptr;void* first=nullptr;unsigned level=0;};
inline bool room(void* address,Room* out) {
    __try {
        out->next=field<void*>(address,0x7c);
        auto room2=field<void*>(address,0x10);if(!room2)return false;
        auto level=field<void*>(room2,0x58);if(!level)return false;
        out->level=field<unsigned>(level,0x1d0);out->first=field<void*>(address,0x74);
        return out->level>0 && out->level<10000;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
inline bool unit(void* address,void* expectedRoom,const MapMarkerDefinitions& definitions,void** next,MapMarker* out) {
    __try {
        // Verified native object enumeration at D2Client + 0x61d00 uses +0xe8.
        *next=field<void*>(address,0xe8);
        auto type=field<unsigned>(address,0),id=field<unsigned>(address,4);
        out->frame=definitions.frame(type,id);if(!out->frame)return false;
        // The native reader sets this after adding the unit to its layer.
        // Respect it even if a moving event no longer matches that cell's
        // coordinates, or PD2 substitutes a different shrine symbol.
        out->nativeRegistered=(field<unsigned>(address,0xc4)&0x20000000u)!=0;
        auto mode=field<unsigned>(address,0x10);
        if((type==1 && (mode==0 || mode==12 || mode>15)) || (type==2 && mode>7))return false;
        auto path=field<void*>(address,0x2c);if(!path)return false;
        if(field<void*>(path,type==2?0:0x1c)!=expectedRoom)return false;
        out->x=type==2?double(field<unsigned>(path,0xc)):field<unsigned>(path,0)/65536.0;
        out->y=type==2?double(field<unsigned>(path,0x10)):field<unsigned>(path,4)/65536.0;
        if(out->x<0 || out->y<0 || out->x>=65536 || out->y>=65536)return false;
        // Match native +0x61780 exactly, including truncation before scaling.
        int sx=field<int>(path,type==2?4:8),sy=field<int>(path,type==2?8:12);
        if(sx<-1000000 || sx>1000000 || sy<-1000000 || sy>1000000)return false;
        out->mapX=sx/10+1;out->mapY=sy/10-3;
        return out->mapX>=-32768 && out->mapX<=32767 && out->mapY>=-32768 && out->mapY<=32767;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Only a bounded snapshot of already-loaded units. Old samples are replaced,
// so destroyed event actors cannot accumulate as permanent phantom markers.
inline std::vector<MapMarker> capture(const void* client,unsigned level,const MapMarkerDefinitions& definitions) {
    std::vector<MapMarker> found;found.reserve(32);unsigned rooms=0,units=0;
    for(auto address=floor_reader::firstRoom(client);address && rooms++<256;) {
        Room metadata{};auto current=address;const bool valid=room(address,&metadata);address=metadata.next;
        if(!valid || metadata.level!=level)continue;
        for(auto actor=metadata.first;actor && units++<4096;) {
            void* next=nullptr;MapMarker marker;
            if(unit(actor,current,definitions,&next,&marker))found.push_back(marker);
            actor=next;if(found.size()>=256)return found;
        }
    }
    return found;
}
}
}
