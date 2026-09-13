// Native terrain, navigation and entrance classification from active game tables.
#pragma once
#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace exploration {
class HybridArtwork {
    std::array<std::uint8_t,65536> flags_{};
    std::array<std::uint8_t,65536> sewerShapes_{};
    std::array<std::uint8_t,65536> sewerWaterRoles_{};
    std::array<std::uint8_t,65536> poisonedWellFlags_{};
    std::array<std::uint8_t,65536> entrances_{};
    std::array<std::uint8_t,65536> waterTiles_{};
    bool loaded_=false;
    static std::vector<std::string> columns(const std::string& line) {
        std::vector<std::string> out;std::size_t start=0;
        for(;;){auto end=line.find('\t',start);out.push_back(line.substr(start,end-start));
            if(end==std::string::npos)break;start=end+1;}
        for(auto& value:out)if(!value.empty() && value.back()=='\r')value.pop_back();
        return out;
    }
    static int number(const std::string& value) {
        if(value.empty() || value.size()>5)return -1;
        int result=0;for(char c:value){if(c<'0'||c>'9')return -1;result=result*10+c-'0';}
        return result<65536?result:-1;
    }
    static bool oneOf(const std::string& word,std::initializer_list<const char*> values) {
        for(auto value:values)if(word==value)return true;return false;
    }
public:
    // Presentation only: this never changes the wall role or exploration mask.
    static bool entranceLabel(const std::string& label) {
        std::string text;for(unsigned char c:label)text+=std::isalnum(c)?char(std::tolower(c)):' ';
        std::istringstream words(text);std::string word;bool found=false;
        while(words>>word) {
            if(word=="fake" || word=="arcane")return false;
            if(oneOf(word,{"exit","entrance","ent","stairs","stairsr","stairsl","stair","trapdoor","trappdoor","edwn","cdwn","cext"}) ||
               word.compare(0,4,"cdwn")==0 || word.compare(0,4,"cext")==0)found=true;
        }
        return found || text=="den of evil" || text=="ftwr marker" ||
            text.compare(0,6,"crypt ")==0 || text=="temple str dwn" || text.compare(0,9,"mesa dwn ")==0;
    }
    static bool landmarkLabel(const std::string& label) {
        std::string text;for(unsigned char c:label)text+=std::isalnum(c)?char(std::tolower(c)):' ';
        std::istringstream words(text);std::string word;
        while(words>>word)if(oneOf(word,{"waypoint","shrine","marker","altar","forge","tome"}))return true;
        return false;
    }
    enum class Role { Detail, Wall, Water, SewerWall, SewerWater };
    enum class SewerShape { None, Down, Up, Peak, Cap };
    static SewerShape sewerShape(const std::string& label) {
        std::string text=label;for(auto& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));
        if(text=="3sewer wr" || text=="3sewer wbl")return SewerShape::Down;
        if(text=="3sewer wl" || text=="3sewer wtr")return SewerShape::Up;
        if(text=="3sewer wtll")return SewerShape::Peak;
        if(text=="3sewer wbr")return SewerShape::Cap;
        return SewerShape::None;
    }
    static Role describe(const std::string& label) {
        std::vector<std::string> words;std::string word;
        for(unsigned char c:label) {
            if(std::isalnum(c))word+=char(std::tolower(c));
            else if(!word.empty()){words.push_back(word);word.clear();}
        }
        if(!word.empty())words.push_back(word);
        if(words.empty())return Role::Detail;
        // Water remains native; named water tiles identify adjacent contours
        // for the independent shoreline color. No material is guessed from RGB.
        for(const auto& w:words)if(oneOf(w,{"river","water","pool","oasis"}))return Role::Water;
        // Act 3 sewer architecture has visible wall faces that the connected
        // walkable-floor contour does not reliably reproduce. Preserve its
        // artwork as the source for tracing, with native fallback. Keep bridges
        // and drains outside that tracing role.
        if(words[0]=="3sewer") {
            if(words.size()>1 && words[1]=="drain")return Role::SewerWater;
            return sewerShape(label)!=SewerShape::None?Role::SewerWall:Role::Detail;
        }
        for(const auto& w:words)if(oneOf(w,{"exit","ent","entrance","stair","stairs","str","down","dwn","door",
            "bridge","path","platform","walkway","tran","transition","pens","prison","shrine","waypoint",
            "marker","altar","alter","forge","tome","crypt","ftwr"}))return Role::Detail;
        std::size_t part=0;
        if(oneOf(words[0],{"c","f","g","p","rd","s","tmb","stn","m"}))part=1;
        else if(words[0]=="baal")part=words.size()>1 && words[1]=="low"?2:1;
        if(part<words.size() && oneOf(words[part],{"wl","wr","wbl","wbr","wtr","wtll","wtlr","rw","lw"}))return Role::Wall;
        if(words[0]=="cave" && words.size()>1 && oneOf(words[1],{"la","lb","lc","ra","rb","rc","bot","top","fill",
            "r2ll","ll","ltop","lr","lr2l","lbot","lbotx","ltoplx","ltoplngl","ltoplngr","bngr","blngxb","blngxa","blngl"}))return Role::Wall;
        return Role::Detail;
    }
    bool load(std::istream& stream) {
        flags_.fill(0);sewerShapes_.fill(0);sewerWaterRoles_.fill(0);poisonedWellFlags_.fill(0);entrances_.fill(0);waterTiles_.fill(0);loaded_=false;std::string line;
        if(!std::getline(stream,line) || line.size()>32768)return false;
        auto header=columns(line);std::vector<std::size_t> cells;
        auto levelColumn=std::find(header.begin(),header.end(),"LevelName");
        const auto levelIndex=std::size_t(levelColumn-header.begin());
        for(const char* name:{"Cel1","Cel2","Cel3","Cel4"}) {
            auto it=std::find(header.begin(),header.end(),name);
            if(it==header.end() || it==header.begin())return false;
            cells.push_back(std::size_t(it-header.begin()));
        }
        std::size_t bytes=line.size(),lines=0,definitions=0;
        while(std::getline(stream,line)) {
            bytes+=line.size();if(line.size()>32768 || bytes>16*1024*1024 || ++lines>100000){flags_.fill(0);return false;}
            auto row=columns(line);if(row.size()!=header.size())continue;
            for(auto col:cells) {
                int id=number(row[col]);if(id<0)continue;
                Role role=describe(row[col-1]);
                std::string label=row[col-1];for(auto& c:label)c=char(std::tolower(static_cast<unsigned char>(c)));
                if(role==Role::Water)waterTiles_[id]|=label.find("island")==std::string::npos?1:2;
                entrances_[id]|=entranceLabel(row[col-1])?1:2;
                if(landmarkLabel(row[col-1]))entrances_[id]|=4;
                flags_[id]|=role==Role::Wall?1:role==Role::Water?6:role==Role::SewerWall?16:role==Role::SewerWater?32:2;++definitions;
                // This profile's Poisoned Well (level 202) uses table group 46.
                // Its custom labels denote contour sprites, including a blank
                // filler frame, not water texture. Keep aliases in other areas
                // independent; an unknown alias in this group protects the ID.
                if(levelIndex<row.size() && row[levelIndex]=="46")
                    poisonedWellFlags_[id]|=(row[col-1]=="PW wall" || row[col-1]=="PW outline")?1:2;
                if(levelIndex<row.size() && row[levelIndex]=="3 Sewer")sewerWaterRoles_[id]|=role==Role::SewerWater?1:2;
                if(role==Role::SewerWall) {
                    auto shape=std::uint8_t(sewerShape(row[col-1]));
                    if(sewerShapes_[id] && sewerShapes_[id]!=shape)flags_[id]|=2;
                    sewerShapes_[id]=shape;
                }
            }
        }
        loaded_=!stream.bad() && definitions>0;return loaded_;
    }
    bool protectObjects(std::istream& stream) {
        std::string line;if(!std::getline(stream,line))return false;
        auto header=columns(line);auto it=std::find(header.begin(),header.end(),"AutoMap");
        if(it==header.end())return false;auto col=std::size_t(it-header.begin());
        auto nameIt=std::find(header.begin(),header.end(),"Name");auto nameCol=std::size_t(nameIt-header.begin());
        auto descIt=std::find(header.begin(),header.end(),"description - not loaded");auto descCol=std::size_t(descIt-header.begin());
        std::size_t bytes=0,lines=0;
        while(std::getline(stream,line)) {
            bytes+=line.size();if(line.size()>32768 || bytes>16*1024*1024 || ++lines>100000)return false;
            auto row=columns(line);if(col>=row.size())continue;
            int id=number(row[col]);if(id>0)flags_[id]|=10; // Keep object protection distinct from terrain aliases.
            if(id>0 && nameCol<row.size())entrances_[id]|=
                (entranceLabel(row[nameCol]) || (descCol<row.size() && entranceLabel(row[descCol])))?1:2;
        }
        return !stream.bad();
    }
    Role role(std::uint32_t id,std::uint32_t level=0) const {
        if(!loaded_ || id>=flags_.size())return Role::Detail;
        auto flags=flags_[id];
        if(level==202 && poisonedWellFlags_[id]==1 && !(flags&8))return Role::Wall;
        // Drain artwork is reused as decorative floors in unrelated endgame
        // areas. The runtime applies this role only in sewer levels 92/93.
        if((flags&32) && !(flags&8) && sewerWaterRoles_[id]==1)return Role::SewerWater;
        return (flags&4)?Role::Water:flags==1?Role::Wall:flags==16?Role::SewerWall:Role::Detail;
    }
    SewerShape sewerShape(std::uint32_t id) const {return role(id)==Role::SewerWall?SewerShape(sewerShapes_[id]):SewerShape::None;}
    bool loaded() const {return loaded_;}
    bool waterTile(std::uint32_t id) const {
        return loaded_ && id<flags_.size() && waterTiles_[id]==1 && !(flags_[id]&8) && !styledDetail(id);
    }
    bool entrance(std::uint32_t id) const {
        if(!loaded_ || id>=entrances_.size())return false;
        // Native generic stair/exit symbol is not listed in automap.txt.
        // Any active table alias overrides this supported-artwork fallback.
        return (entrances_[id]&3)==1 || (id==308 && entrances_[id]==0 && flags_[id]==0);
    }
    bool styledDetail(std::uint32_t id) const {
        if(!loaded_ || id>=flags_.size())return false;
        // Built-in portal/stair/shrine/waypoint variants absent from TXT.
        // An active table alias takes precedence over this artwork fallback.
        const bool builtIn=(id==301 || id==302 || id==311 || id==317) && flags_[id]==0 && entrances_[id]==0;
        // An ambiguous stair/terrain alias is kept for navigation, even when
        // it is too uncertain for the independent brightness enhancement.
        return builtIn || (flags_[id]&8) || (entrances_[id]&5) || entrance(id);
    }
    std::size_t walls() const {return std::count(flags_.begin(),flags_.end(),std::uint8_t(1));}
    std::size_t poisonedWellContours() const {
        if(!loaded_)return 0;
        std::size_t count=0;
        for(std::size_t id=0;id<flags_.size();++id)if(poisonedWellFlags_[id]==1 && !(flags_[id]&8))++count;
        return count;
    }
};
}
