// Tracks a game independently from automap visibility and temporary room loads.
#pragma once
#include <array>
#include <cstdint>

namespace exploration {
enum class SessionChange { None, FirstPlayer, MenuReturn, PlayerChanged, SeedChanged };
class SessionIdentity {
    bool active_=false;
    std::uint32_t player_=0,menu_=0;
    std::array<std::uint32_t,5> seeds_{};
    std::array<bool,5> seen_{};
public:
    SessionChange observe(std::uint32_t player,std::uint32_t act,std::uint32_t seed,std::uint32_t menu) {
        // Invalid/transient native state supplies no evidence of a new game.
        if(act>=seen_.size())return SessionChange::None;
        SessionChange change=SessionChange::None;
        if(!active_)change=SessionChange::FirstPlayer;
        else if(menu!=menu_)change=SessionChange::MenuReturn;
        else if(player!=player_)change=SessionChange::PlayerChanged;
        else if(seen_[act] && seeds_[act]!=seed)change=SessionChange::SeedChanged;
        if(change!=SessionChange::None)seen_.fill(false);
        active_=true;player_=player;menu_=menu;seeds_[act]=seed;seen_[act]=true;
        return change;
    }
};
inline const char* sessionReason(SessionChange change) {
    switch(change) {
        case SessionChange::FirstPlayer:return "first player";
        case SessionChange::MenuReturn:return "returned through game menus";
        case SessionChange::PlayerChanged:return "player identity changed";
        case SessionChange::SeedChanged:return "known act seed changed";
        default:return "unchanged";
    }
}
// Only a confirmed menu (controls present AND no player unit) advances the
// epoch. Missing room/path data, map toggles and elapsed time do not qualify.
class MenuObserver {
    bool atMenu_=false;
public:
    bool sample(bool hasPlayer,bool hasControls) {
        const bool menu=!hasPlayer && hasControls;
        const bool entered=menu && !atMenu_;atMenu_=menu;return entered;
    }
};
}
