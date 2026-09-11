// Reads expected campaign automap layers from the user's own Levels.txt.
// Native player area and automap layer can change on different callbacks.
#pragma once
#include <algorithm>
#include <array>
#include <istream>
#include <string>
#include <vector>

namespace exploration {
class CampaignLayers {
    std::array<unsigned,133> layers_{},acts_{};
    std::array<bool,133> known_{};
    std::size_t count_=0;
    static std::vector<std::string> columns(const std::string& line) {
        std::vector<std::string> out;std::size_t start=0;
        for(;;){auto end=line.find('\t',start);out.push_back(line.substr(start,end-start));
            if(!out.back().empty() && out.back().back()=='\r')out.back().pop_back();
            if(end==std::string::npos)break;start=end+1;}
        return out;
    }
    static bool number(const std::string& text,unsigned& value) {
        value=0;if(text.size()>4)return false;
        for(char c:text){if(c<'0' || c>'9')return false;value=value*10+unsigned(c-'0');}
        return true; // Empty numeric fields in these tables default to zero.
    }
public:
    bool load(std::istream& stream) {
        known_.fill(false);count_=0;std::string line;
        if(!std::getline(stream,line) || line.size()>32768)return false;
        auto header=columns(line);std::array<std::size_t,3> col{};
        std::size_t at=0;
        for(const char* name:{"Id","Act","Layer"}) {
            auto it=std::find(header.begin(),header.end(),name);if(it==header.end())return false;
            col[at++]=std::size_t(it-header.begin());
        }
        std::size_t bytes=line.size(),lines=0;
        while(std::getline(stream,line)) {
            bytes+=line.size();
            if(line.size()>32768 || bytes>16*1024*1024 || ++lines>100000){known_.fill(false);count_=0;return false;}
            auto row=columns(line);if(row.size()!=header.size())continue;
            unsigned id=0,act=0,layer=0;
            if(!number(row[col[0]],id) || id<1 || id>132)continue;
            if(!number(row[col[1]],act) || act>=5 || !number(row[col[2]],layer)){
                known_.fill(false);count_=0;return false;
            }
            if(known_[id] && (layers_[id]!=layer || acts_[id]!=act)){known_.fill(false);count_=0;return false;}
            if(!known_[id])++count_;
            known_[id]=true;layers_[id]=layer;acts_[id]=act;
        }
        if(stream.bad()){known_.fill(false);count_=0;return false;}
        return count_>0;
    }
    bool lookup(unsigned level,unsigned act,unsigned& layer) const {
        if(level>=known_.size() || !known_[level] || acts_[level]!=act)return false;
        layer=layers_[level];return true;
    }
    std::size_t size() const {return count_;}
};
}
