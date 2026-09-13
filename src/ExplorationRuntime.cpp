// Game integration: initialize the DLL, validate the supported hook chain, read
// player movement, bypass towns, and submit styled or clipped native automap
// geometry. Related components and the drawing flow are indexed in README.md.

// Experimental, build-specific, offline-test runtime. Original implementation.
// No game artwork or engine DLL on disk is modified.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <cstdio>
#include <climits>
#include <share.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "ExplorationMask.hpp"
#include "FrontierContacts.hpp"
#include "NativeFloorReader.hpp"
#include "StyledProjection.hpp"
#include "StyledWorker.hpp"
#include "SessionIdentity.hpp"
#include "HybridArtwork.hpp"
#include "CampaignLayers.hpp"
#include "GameTables.hpp"
#include "NativeWallTrace.hpp"
#include "SewerWater.hpp"
#include "RasterClipCache.hpp"
#include "ArtworkBounds.hpp"
#include "BoundaryColors.hpp"
#include "BoundaryMenu.hpp"
#include "MapMarkers.hpp"
#include <unordered_map>
#include <set>
using exploration::Point;
using exploration::Rect;
using exploration::Mask;
struct NativeRect { int left,right,top,bottom; };
using CellDraw=void(__stdcall*)(void*,int,int,NativeRect*,int);
using LineDraw=void(__stdcall*)(int,int,int,int,DWORD,DWORD);
// Inspected D2Glide/D2GL vertex ABI. Both exports receive pointers to these.
struct GlideVertex { float x,y;DWORD color;float scale,s,t;DWORD unused; };
static_assert(sizeof(GlideVertex)==28,"Glide vertex ABI");
using FloatLineDraw=void(__stdcall*)(const void*,const void*);
using FloatPointDraw=void(__stdcall*)(const void*);
using QuadDraw=void(__stdcall*)(DWORD,DWORD,const void*,DWORD);
static QuadDraw originalQuad=nullptr;
static FloatLineDraw originalFloatLine=nullptr;
static FloatPointDraw originalFloatPoint=nullptr;
static bool fractionalFrontier=false;
static Point frontierA{},frontierB{};
static const std::vector<std::pair<Point,Point>>* frontierBatch=nullptr;
static const std::vector<Rect>* terrainClips=nullptr;
using VertexArrayDraw=void(__stdcall*)(DWORD,DWORD,const void*);
using ConstantColor=void(__stdcall*)(DWORD);
static VertexArrayDraw styledArray=nullptr;
static ConstantColor styledColor=nullptr;
static const std::vector<const void*>* styledBatch=nullptr;
static DWORD styledBatchColor=0,styledRestoreColor=0;
static unsigned overlayOpacity=80;
static exploration::BoundaryColor boundaryColor=exploration::BoundaryColor::Red;
static exploration::BoundaryColor wallColor=exploration::BoundaryColor::Gray;
static DWORD fractionalTint=0;
static std::string settingsPath;
static DWORD overlayAlpha(DWORD alpha){return (alpha*overlayOpacity+50)/100;}
static DWORD overlayColor(DWORD color){return (color&0xffffff00u)|overlayAlpha(color&255);}
struct StyledState {
    styled_map::FloorCopies floor;
    styled_map::Drawing drawing;
    std::size_t queuedRooms=0,queuedMask=0,floorCells=0;
    DWORD submitted=0;
};
// Process-lifetime worker: its owned data stays valid during CRT/DLL teardown.
// It performs no logging, native callbacks, game reads or rendering calls.
static styled_map::Worker* styledWorker=nullptr;
static std::map<std::uint64_t,StyledState> styledLevels;
static StyledState* styledCurrent=nullptr;
// Keep one prepared floor result, not an extra mesh for every visited area.
static std::unique_ptr<styled_map::PreparedFloors> preparedFloors;
static const StyledState* preparedOwner=nullptr;
static std::uint64_t preparedSerial=0;
static bool styledActive=false;
static unsigned long styledPasses=0,styledQuadCount=0;
static double styledBuildMs=0;
static double styledLatencyMs=0;
static std::size_t styledRebuiltChunks=0,styledTotalChunks=0;
static unsigned char* client=nullptr;
static CellDraw originalCell=nullptr;
static LineDraw originalLine=nullptr;
static void* originalBegin=nullptr;
static void* originalEnd=nullptr;
static bool inPass=false,maskActive=false,enabled=true,installed=false;
enum class MapStyle { Original, Native, Styled, Hybrid };
static MapStyle campaignStyle=MapStyle::Hybrid,mapsStyle=MapStyle::Hybrid,activeStyle=MapStyle::Styled;
static exploration::HybridArtwork hybridArtwork;
static exploration::MapMarkerDefinitions mapMarkerDefinitions;
static std::vector<exploration::MapMarker> mapMarkers;
static std::set<std::tuple<unsigned,int,int>> nativeMarkers;
static void* markerArtworkFile=nullptr; // Borrowed only within this draw pass.
static int markerDrawMode=5;
static unsigned long markersDrawn=0,markersAlreadyNative=0;
static exploration::CampaignLayers campaignLayers;
static bool gameTablesPending=false;
static void ensureGameTables();
static void ensureBoundaryMenu();
static unsigned long layerWaits=0;
static unsigned long hybridWallsReplaced=0,hybridDetails=0,hybridWater=0;
static exploration::ArtworkBounds artworkBounds;
static unsigned long nativeBoundsTrimmed=0,blankSpritesSkipped=0;
static std::vector<GlideVertex> sewerCasing,sewerCore;
static unsigned long sewerTraced=0,sewerFallbacks=0;
struct CachedWallTrace {DWORD length=0;exploration::NativeWallTrace shape;};
static std::map<std::tuple<DWORD,int,int>,CachedWallTrace> sewerWallTraces;
static constexpr std::size_t sewerVertexLimit=65536;
static exploration::SewerWater sewerWater;
static std::vector<GlideVertex> sewerWaterFill;
static unsigned long sewerWaterCells=0;
static bool nativeArtwork() {return activeStyle==MapStyle::Native || activeStyle==MapStyle::Hybrid;}
static const char* styleName(MapStyle style) {
    switch(style){case MapStyle::Original:return "original";case MapStyle::Native:return "native";
        case MapStyle::Hybrid:return "hybrid";default:return "styled";}
}
// Verified local Levels.txt: the five-act campaign ends at Worldstone Chamber
// (132); special/endgame areas use the independent maps setting.
static MapStyle styleForLevel(DWORD level) {return level>=1 && level<=132?campaignStyle:mapsStyle;}
static constexpr DWORD targetLevel=0; // Zero applies to every area in the opt-in test.
// Neutral RGB 132/132/132; minimap capture forces high opacity, so keep the
// source gray muted instead of using a near-white pale palette entry.
static constexpr DWORD frontierColor=29;
static constexpr double maskCellSize=0.25; // Quarter-subtile: 20x finer per axis.
// Compiled policy: 33 world subtiles. No INI, menu or view-size override.
static constexpr int revealRadius=132; // Quarter-subtile mask cells.
static Rect passViewport{};
static bool haveViewport=false;
static exploration::FrontierContacts contactFrontier;
struct CachedSilhouette {
    DWORD length=0,flags=0;int width=0,height=0;
    exploration::Silhouette shape;
};
static std::unordered_map<const void*,CachedSilhouette> silhouetteCache;
static std::size_t silhouetteBytes=0;
static unsigned long shapesDecoded=0,shapeFailures=0;
static exploration::Session explorationSession(maskCellSize);
static Mask emptyMask(maskCellSize);
static Mask* explored=&emptyMask;
static std::uint64_t gameSerial=1,lastLevelKey=0;
static DWORD lastPlayerId=0,lastFrame=0,lastReport=0;
static exploration::SessionIdentity sessionIdentity;
static volatile LONG menuEpoch=0;
static void** firstMenuControl=nullptr;
static int lastX=-1,lastY=-1;
static bool sawTarget=false;
static unsigned long drawn=0,suppressed=0,partial=0;
static volatile LONG beginCount=0,endCount=0,cellCount=0,stateFailure=0,observedLevel=-1,frontierCount=0;
static volatile LONG fractionalCount=0,townPasses=0;
static unsigned long nativeCellCalls=0,clippedQuads=0;
static LARGE_INTEGER counterFrequency{},passStarted{};
static LONGLONG mapTicks=0;
static unsigned long mapSamples=0;
static void* expectedBeginTarget=nullptr;
static void* expectedEndTarget=nullptr;
static FILE* logfile=nullptr;
struct TownBoundary {
    std::uint64_t session=0;
    uintptr_t act=0;
    DWORD seed=0,level=0,layer=0,outside=0,sampled=0;
    Rect bounds{};
    exploration::ProjectedMask raster[2];
    Point connection{};
    bool ready=false,nearOutside=false;
    int revealX=INT_MIN,revealY=INT_MIN;
};
static TownBoundary townBoundary;
static bool nativeTownActive=false;
static bool townInViewport=true;
static unsigned long townClippedCells=0,townPreviewPasses=0;
static exploration::RasterClipCache rasterClips;
// Reuse exact clipping in stable automap coordinates. Exploration is monotonic
// within a session/layer; growth refreshes partial coverage, retaining full clips.
static void prepareRasterCache(int divisor) {
    auto context=std::make_tuple(gameSerial,lastLevelKey,explored,divisor,nativeTownActive,
        townBoundary.session,townBoundary.act,townBoundary.seed,townBoundary.level,townBoundary.layer,
        townBoundary.bounds.left,townBoundary.bounds.top,townBoundary.bounds.right,townBoundary.bounds.bottom);
    static decltype(context) previous{};
    static std::size_t previousSize=0;
    const auto size=explored->size();
    if(context!=previous || size<previousSize){rasterClips.invalidate();previous=context;}
    else if(size>previousSize)rasterClips.grow();
    previousSize=size;
}
template<class Emit> static void cachedRasterClips(Rect bounds,int divisor,int shiftX,int shiftY,int coverage,Emit emit) {
    // All drawing paths share both the cache and its invalidation context.
    // Keeping previous inside this template would give each emitter its own.
    prepareRasterCache(divisor);
    Rect stable{bounds.left+shiftX,bounds.top+shiftY,bounds.right+shiftX,bounds.bottom+shiftY};
    rasterClips.query({stable.left,stable.top,stable.right,stable.bottom,coverage},[&](auto append){
        auto town=[&](auto add){townBoundary.raster[divisor==20?1:0].clip(stable.left,stable.top,stable.right,stable.bottom,
            [&](int l,int t,int r,int b){add(Rect{l,t,r,b});});};
        if(coverage==0)explored->clipProjected(stable,divisor,0,0,append);
        else if(coverage==1)town(append);
        else {
            static std::vector<Rect> rows;rows.clear();
            auto collect=[&](Rect r){rows.push_back(r);};
            explored->clipProjected(stable,divisor,0,0,collect);town(collect);
            std::sort(rows.begin(),rows.end(),[](Rect a,Rect b){return std::tie(a.top,a.left,a.right)<std::tie(b.top,b.left,b.right);});
            Rect run{};bool have=false;
            for(auto row:rows){
                if(have && run.top==row.top && row.left<=run.right)run.right=std::max(run.right,row.right);
                else {if(have)append(run);run=row;have=true;}
            }
            if(have)append(run);
        }
    },[&](Rect r){emit(Rect{r.left-shiftX,r.top-shiftY,r.right-shiftX,r.bottom-shiftY});});
}
static void log(const char* msg) { if(logfile) {fprintf(logfile,"%s\n",msg);fflush(logfile);} }
template<class T> T read(const void* p,size_t offset=0) {
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p)+offset);
}
// SEH contains optional metadata reads; no exception crosses game callbacks.
struct PlayerState { double x,y;DWORD level,seed,id;uintptr_t act=0;DWORD actNumber=0; };
static bool playerState(PlayerState* s) {
    __try {
        void* unit=read<void*>(client,0x11bbfc);
        if(!unit || read<DWORD>(unit)!=0) return false;
        void* path=read<void*>(unit,0x2c);if(!path)return false;
        void* room=read<void*>(path,0x1c);if(!room)return false;
        void* room2=read<void*>(room,0x10);if(!room2)return false;
        void* level=read<void*>(room2,0x58);if(!level)return false;
        void* act=read<void*>(unit,0x1c);if(!act)return false;
        // Path offsets 0/4 contain unsigned 16.16 positions (fraction + tile).
        s->x=read<DWORD>(path,0)/65536.0;s->y=read<DWORD>(path,4)/65536.0;
        s->level=read<DWORD>(level,0x1d0);s->seed=read<DWORD>(act,0xc);
        s->id=read<DWORD>(unit,0xc);s->act=reinterpret_cast<uintptr_t>(act);s->actNumber=read<DWORD>(act,0x14);
        return s->level>0 && s->level<10000 && s->actNumber<5;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool isTown(DWORD level) {
    // Verified against local Levels.txt and BH's IsTown helper.
    return level==1 || level==40 || level==75 || level==103 || level==109;
}
static DWORD automapLayer() {
    __try {
        auto layer=read<void*>(client,0x11c1c4);
        return layer?read<DWORD>(layer):0xffffffffu;
    } __except(EXCEPTION_EXECUTE_HANDLER){return 0xffffffffu;}
}
static std::uint64_t mapKey(const PlayerState& p) {
    DWORD id=p.level;
    unsigned layer=0;
    // Native campaign automap layers already group adjoining areas in one
    // coordinate space. Share their mask AND geometry cache, so crossing a
    // zone label does not discard the visible part of the previous area.
    // Keep acts/seeds separate, and retain per-level keys for endgame maps,
    // towns, targeted diagnostics and unavailable native layer metadata.
    if(!isTown(p.level) && targetLevel==0 && campaignLayers.lookup(p.level,p.actNumber,layer) && automapLayer()==layer)
        id=0x80000000u|(p.actNumber<<24)|layer;
    return (std::uint64_t(p.seed)<<32)|id;
}
static bool inTownBounds(Point p) {
    const auto& r=townBoundary.bounds;
    return p.x>=r.left && p.x<r.right && p.y>=r.top && p.y<r.bottom;
}
static void setTownBounds(const PlayerState& p,DWORD layer,const floor_reader::AreaBounds& r) {
    townBoundary.level=p.level;townBoundary.layer=layer;
    townBoundary.bounds={r.x,r.y,r.x+r.width,r.y+r.height};
    for(int i=0;i<2;++i) {
        townBoundary.raster[i]=exploration::ProjectedMask{};
        townBoundary.raster[i].revealQuarterRect(r.x*4,r.y*4,(r.x+r.width)*4,(r.y+r.height)*4,i?20:10);
    }
    townBoundary.ready=true;
}
// Return the outdoor state to build while the player is still inside town.
// Only owned collision copies are inspected; no neighboring rooms are loaded.
static PlayerState updateTownBoundary(const PlayerState& p,DWORD now) {
    if(styleForLevel(p.level)==MapStyle::Original)return p;
    if(townBoundary.session!=gameSerial || townBoundary.act!=p.act || townBoundary.seed!=p.seed ||
       (isTown(p.level) && townBoundary.ready && townBoundary.level!=p.level)) {
        townBoundary=TownBoundary{};townBoundary.session=gameSerial;townBoundary.act=p.act;townBoundary.seed=p.seed;
    }
    DWORD layer=automapLayer();const bool town=isTown(p.level);
    if(town && !townBoundary.ready && layer<10000) {
        floor_reader::AreaBounds bounds;
        if(floor_reader::currentAreaBounds(client,&bounds) && p.x>=bounds.x && p.y>=bounds.y &&
           p.x<bounds.x+bounds.width && p.y<bounds.y+bounds.height) {
            setTownBounds(p,layer,bounds);
            if(logfile){fprintf(logfile,"TOWN footprint level=%lu layer=%lu x=%d y=%d w=%d h=%d\n",p.level,layer,bounds.x,bounds.y,bounds.width,bounds.height);fflush(logfile);}
        }
    }
    const bool sharedCampaign=p.level>=1 && p.level<=132 && !town && targetLevel==0;
    nativeTownActive=enabled && townBoundary.ready && townBoundary.layer==layer &&
        (p.level==townBoundary.level || p.level==townBoundary.outside || sharedCampaign);
    if(!nativeTownActive || !town || !styledWorker || !styledArray || !styledColor || targetLevel!=0 ||
       strstr(GetCommandLineA(),"-exploration-floor-probe"))return p;
    const double radius=revealRadius*maskCellSize;
    if(!townBoundary.sampled || now-townBoundary.sampled>=250) {
        townBoundary.sampled=now;townBoundary.nearOutside=false;
        double nearest=radius*radius;DWORD outside=0;Point connection{};
        floor_reader::capture(client,0,[&](const floor_reader::Room& r){
            return !isTown(r.level) && r.x<p.x+radius && r.x+r.width>p.x-radius &&
                r.y<p.y+radius && r.y+r.height>p.y-radius;
        },[&](const floor_reader::Room& r,const std::vector<std::uint16_t>& flags){
            int left=std::max(r.x,int(floor(p.x-radius))),right=std::min(r.x+r.width,int(ceil(p.x+radius)));
            int top=std::max(r.y,int(floor(p.y-radius))),bottom=std::min(r.y+r.height,int(ceil(p.y+radius)));
            for(int y=top;y<bottom;++y)for(int x=left;x<right;++x) {
                Point candidate{x+.5,y+.5};
                if((flags[std::size_t(y-r.y)*r.width+x-r.x]&0x0021) || inTownBounds(candidate))continue;
                double distance=(candidate.x-p.x)*(candidate.x-p.x)+(candidate.y-p.y)*(candidate.y-p.y);
                if(distance<nearest){nearest=distance;outside=r.level;connection=candidate;}
            }
        });
        if(outside) {
            if(townBoundary.outside!=outside){townBoundary.revealX=townBoundary.revealY=INT_MIN;}
            townBoundary.outside=outside;townBoundary.connection=connection;townBoundary.nearOutside=true;
        }
    }
    if(!townBoundary.outside)return p;
    PlayerState outdoor=p;outdoor.level=townBoundary.outside;
    auto key=mapKey(outdoor);
    explored=&explorationSession.select(gameSerial,key);maskActive=true;
    int x=int(floor(p.x/maskCellSize)),y=int(floor(p.y/maskCellSize));
    if(townBoundary.nearOutside && (x!=townBoundary.revealX || y!=townBoundary.revealY)) {
        explored->revealAround({p.x,p.y},revealRadius);townBoundary.revealX=x;townBoundary.revealY=y;
    }
    activeStyle=styleForLevel(outdoor.level);
    if(activeStyle==MapStyle::Original){maskActive=false;nativeTownActive=false;return outdoor;}
    outdoor.x=townBoundary.connection.x;outdoor.y=townBoundary.connection.y;
    ++townPreviewPasses;return outdoor;
}
static bool updateForPlayer(const PlayerState& p,DWORD now) {
    nativeTownActive=false;
    unsigned expectedLayer=0;
    if(campaignLayers.lookup(p.level,p.actNumber,expectedLayer) && automapLayer()!=expectedLayer) {
        // Loading can expose the new player area before the automap switches.
        // Do not reveal or ingest it into the previous layer's shared cache.
        maskActive=false;styledActive=false;++layerWaits;return false;
    }
    const bool changedArea=observedLevel!=static_cast<LONG>(p.level);
    InterlockedExchange(&observedLevel,static_cast<LONG>(p.level));
    auto change=sessionIdentity.observe(p.id,p.actNumber,p.seed,DWORD(InterlockedCompareExchange(&menuEpoch,0,0)));
    if(change!=exploration::SessionChange::None) {
        explorationSession.leave();++gameSerial;lastLevelKey=0;lastX=lastY=-1;sawTarget=false;
        styledLevels.clear();styledCurrent=nullptr;styledActive=false;townBoundary=TownBoundary{};
        preparedFloors.reset();preparedOwner=nullptr;
        silhouetteCache.clear();silhouetteBytes=0;
        artworkBounds.clear();
        sewerWallTraces.clear();
        if(logfile){fprintf(logfile,"SESSION reset serial=%llu reason=%s menuEpoch=%ld\n",
            static_cast<unsigned long long>(gameSerial),exploration::sessionReason(change),InterlockedCompareExchange(&menuEpoch,0,0));fflush(logfile);}
    }
    lastFrame=now;lastPlayerId=p.id;
    auto levelKey=mapKey(p);
    if(levelKey!=lastLevelKey || changedArea){
        lastLevelKey=levelKey;lastX=lastY=-1;sawTarget=false;
        // No borrowed frame memory survives an area/session change.
        silhouetteCache.clear();silhouetteBytes=0;
        artworkBounds.clear();
    }
    const bool town=isTown(p.level);
    activeStyle=styleForLevel(p.level);
    const bool selected=p.level>0 && !town && (targetLevel==0 || p.level==targetLevel);
    maskActive=enabled && selected && activeStyle!=MapStyle::Original;
    explored=selected?&explorationSession.select(gameSerial,levelKey):&emptyMask;
    if(maskActive) {
        int x=static_cast<int>(floor(p.x/maskCellSize)),y=static_cast<int>(floor(p.y/maskCellSize));
        if(x!=lastX || y!=lastY) {explored->revealAround({p.x,p.y},revealRadius);lastX=x;lastY=y;}
    }
    if(!sawTarget) {if(logfile){fprintf(logfile,"Area detected: level=%lu; town=%d; maskEnabled=%d; style=%s; mapKey=%llu; layer=%lu.\n",p.level,town,maskActive,
        styleName(activeStyle),static_cast<unsigned long long>(levelKey),automapLayer());fflush(logfile);}sawTarget=true;}
    return true;
}
static void probeFloors(DWORD level) {
    static DWORD sampled=0;static std::set<std::tuple<DWORD,int,int,int,int>> rooms;
    static FILE* file=nullptr;
    if(!strstr(GetCommandLineA(),"-exploration-floor-probe") || GetTickCount()-sampled<250 || rooms.size()>=512)return;
    sampled=GetTickCount();
    floor_reader::capture(client,level,[&](const floor_reader::Room& r){return !rooms.count({r.level,r.x,r.y,r.width,r.height});},
        [&](const floor_reader::Room& r,const std::vector<std::uint16_t>& grid){
            if(!file)file=_fsopen("ExplorationFloorProbe.bin","wb",_SH_DENYNO);
            if(!file)return;
            DWORD header[]={0x31524c46,r.level,DWORD(r.x),DWORD(r.y),DWORD(r.width),DWORD(r.height)};
            fwrite(header,sizeof(header),1,file);fwrite(grid.data(),sizeof(std::uint16_t),grid.size(),file);fflush(file);
            rooms.insert({r.level,r.x,r.y,r.width,r.height});
            if(logfile){fprintf(logfile,"FLOOR level=%lu x=%d y=%d w=%d h=%d samples=%zu\n",r.level,r.x,r.y,r.width,r.height,grid.size());fflush(logfile);}
        });
}
static void updateStyled(const PlayerState& p,std::uint64_t levelKey=0) {
    if(!levelKey)levelKey=lastLevelKey;
    styledActive=false;
    if(!maskActive || !styledWorker || !styledArray || !styledColor || strstr(GetCommandLineA(),"-exploration-floor-probe"))return;
    try {
        static std::uint64_t serial=0,selected=0;static DWORD sampled=0;
        if(serial!=gameSerial){styledLevels.clear();styledCurrent=nullptr;preparedFloors.reset();preparedOwner=nullptr;serial=gameSerial;selected=0;}
        const bool changedArea=selected!=levelKey;
        if(changedArea){sampled=0;selected=levelKey;}
        if(auto result=styledWorker->take()) {
            auto it=styledLevels.find(result->level);
            if(result->session==gameSerial && it!=styledLevels.end() && result->success) {
                it->second.drawing=std::move(result->drawing);it->second.floorCells=result->floorCells;
                preparedFloors=std::move(result->preparedFloors);preparedOwner=&it->second;preparedSerial=gameSerial;
                styledBuildMs=result->milliseconds;
                styledLatencyMs=result->latencyMilliseconds;styledRebuiltChunks=result->rebuiltChunks;styledTotalChunks=result->totalChunks;
            }
        }
        auto& state=styledLevels[levelKey];styledCurrent=&state;
        if(changedArea){state.queuedMask=0;state.submitted=0;}
        if(GetTickCount()-sampled>=250) {
            sampled=GetTickCount();
            floor_reader::capture(client,p.level,[&](const floor_reader::Room& r){return state.floor.wanted(r.x,r.y,r.width,r.height);},
                [&](const floor_reader::Room& r,const std::vector<std::uint16_t>& grid){state.floor.ingest(r.x,r.y,r.width,r.height,grid);});
        }
        if((state.queuedRooms!=state.floor.rooms.size() || state.queuedMask!=explored->size()) && GetTickCount()-state.submitted>=40) {
            auto request=std::make_unique<styled_map::BuildRequest>();
            request->session=gameSerial;request->level=levelKey;request->maskSize=explored->size();request->player={p.x,p.y};
            request->visible.spans=explored->rows();request->rooms=state.floor.rooms;
            styledWorker->submit(std::move(request));
            state.queuedRooms=state.floor.rooms.size();state.queuedMask=explored->size();state.submitted=GetTickCount();
        }
        styledActive=state.drawing.quads>0;
    } catch(...) {styledCurrent=nullptr;log("Styled map unavailable; retaining normal artwork clipping.");}
}
static void update() {
    PlayerState p{};
    if(!playerState(&p)) {InterlockedIncrement(&stateFailure);maskActive=false;nativeTownActive=false;sawTarget=false;return;}
    ensureGameTables();
    ensureBoundaryMenu();
    DWORD now=GetTickCount();if(!updateForPlayer(p,now))return;
    if(maskActive)probeFloors(p.level);
    auto drawingPlayer=updateTownBoundary(p,now);
    updateStyled(drawingPlayer,mapKey(drawingPlayer));
}
static bool mapMarkersActive() {
    return enabled && maskActive && observedLevel>132 && observedLevel<10000 && activeStyle!=MapStyle::Original;
}
static void sampleMapMarkers(DWORD now) {
    static std::uint64_t serial=0,key=0;
    static DWORD sampled=0;static bool valid=false;
    if(!mapMarkersActive()){mapMarkers.clear();valid=false;return;}
    if(!valid || serial!=gameSerial || key!=lastLevelKey || now-sampled>=200) {
        mapMarkers=exploration::marker_reader::capture(client,observedLevel,mapMarkerDefinitions);
        serial=gameSerial;key=lastLevelKey;sampled=now;valid=true;
    }
}
struct Transform { double divisor,ox,oy; };
static bool transform(Transform& t) {
    t.divisor=read<int>(client,0xf16b0);
    t.ox=read<int>(client,0x11c1f8);t.oy=read<int>(client,0x11c1fc);
    return (t.divisor==10 || t.divisor==20) && fabs(t.ox)<1000000 && fabs(t.oy)<1000000;
}
[[maybe_unused]] static Point inverse(Point p,const Transform& t) {
    // Reference world-to-automap registration, independent of wall layer.
    const double sx=t.divisor==20?7.0:8.0,sy=t.divisor==20?-3.0:-8.0;
    double a=(p.x+t.ox-sx)*t.divisor/16,b=(p.y+t.oy-sy)*t.divisor/8;
    return {(a+b)/2,(b-a)/2};
}
static Point project(Point p,const Transform& t) {
    return {16*(p.x-p.y)/t.divisor-t.ox+(t.divisor==20?7:8),
             8*(p.x+p.y)/t.divisor-t.oy+(t.divisor==20?-3:-8)};
}
static bool townIntersectsViewport(const Transform& t,Rect view) {
    const auto& r=townBoundary.bounds;
    Point left=project({double(r.left),double(r.bottom)},t),right=project({double(r.right),double(r.top)},t);
    Point top=project({double(r.left),double(r.top)},t),bottom=project({double(r.right),double(r.bottom)},t);
    return left.x<view.right && right.x>view.left && top.y<view.bottom && bottom.y>view.top;
}
static bool frameBounds(void* ctx,int x,int y,Rect* r) {
    __try {
        auto file=read<void*>(ctx,0x34);if(!file)return false;
        DWORD index=read<DWORD>(ctx),count=read<DWORD>(file,0x14);
        if(index>=count || count>65536)return false;
        auto frame=read<void*>(file,0x18+index*4);if(!frame)return false;
        int w=read<int>(frame,4),h=read<int>(frame,8);
        int dx=read<int>(frame,12),dy=read<int>(frame,16);
        if(w<1 || w>512 || h<1 || h>512 || abs(dx)>512 || abs(dy)>512)return false;
        *r={x+dx,y+dy-h,x+dx+w,y+dy};return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool frameIndex(void* ctx,DWORD* id) {
    __try {if(!ctx)return false;*id=read<DWORD>(ctx);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct FrameSource { const void* frame;DWORD length,flags;int width,height;const void* file;DWORD index; };
static bool frameSource(void* ctx,FrameSource* source) {
    __try {
        auto file=read<void*>(ctx,0x34);if(!file || read<DWORD>(file)!=6 || read<DWORD>(file,8)!=0)return false;
        DWORD index=read<DWORD>(ctx),count=read<DWORD>(file,0x14);
        if(index>=count || count>65536)return false;
        auto frame=read<void*>(file,0x18+index*4);if(!frame)return false;
        *source={frame,read<DWORD>(frame,0x1c),read<DWORD>(frame),read<int>(frame,4),read<int>(frame,8),file,index};
        return source->width>0 && source->width<=512 && source->height>0 && source->height<=512 &&
            source->length>0 && source->length<=512u*1025u && source->flags==0;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool copyFrameBytes(const FrameSource& source,unsigned char* bytes) {
    __try {memcpy(bytes,static_cast<const unsigned char*>(source.frame)+0x20,source.length);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool opaqueFrameBounds(void* ctx,Rect asset,Rect* bounds) {
    FrameSource source{};if(!frameSource(ctx,&source))return false;
    if(source.width!=asset.right-asset.left || source.height!=asset.bottom-asset.top)return false;
    Rect local{};
    exploration::ArtworkBounds::Key key{reinterpret_cast<std::uintptr_t>(source.file),reinterpret_cast<std::uintptr_t>(source.frame),
        source.index,source.length,source.width,source.height};
    if(!artworkBounds.query(key,[&](unsigned char* bytes){return copyFrameBytes(source,bytes);},local))return false;
    *bounds={asset.left+local.left,asset.top+local.top,asset.left+local.right,asset.top+local.bottom};return true;
}
static bool queueTraceLines(const std::vector<std::pair<Point,Point>>& lines,Point offset,const std::vector<Rect>& clips);
static bool queueSewerWall(void* ctx,DWORD id,Rect asset,const std::vector<Rect>& clips) {
    FrameSource source{};if(!frameSource(ctx,&source))return false;
    auto key=std::make_tuple(id,source.width,source.height);
    auto found=sewerWallTraces.find(key);
    if(found==sewerWallTraces.end() || found->second.length!=source.length) {
        std::vector<unsigned char> bytes(source.length);exploration::Silhouette silhouette;
        CachedWallTrace item;item.length=source.length;
        if(!copyFrameBytes(source,bytes.data()) || !silhouette.decode(bytes.data(),bytes.size(),source.width,source.height) ||
           !item.shape.buildSewer(silhouette,source.width,source.height,hybridArtwork.sewerShape(id)))return false;
        if(sewerWallTraces.size()>=24)sewerWallTraces.clear();
        found=sewerWallTraces.insert_or_assign(key,std::move(item)).first;
    }
    return queueTraceLines(found->second.shape.lines,{double(asset.left),double(asset.top)},clips);
}
static bool queueTraceLines(const std::vector<std::pair<Point,Point>>& lines,Point offset,const std::vector<Rect>& clips) {
    static std::vector<GlideVertex> outerVertices,innerVertices;
    outerVertices.clear();innerVertices.clear();bool overflow=false;
    auto append=[&](const styled_map::Quad& q,std::vector<GlideVertex>& into) {
        for(auto clip:clips)styled_map::clipQuad(q,clip,[&](const styled_map::Quad& part){
            if(into.size()+4>sewerVertexLimit){overflow=true;return;}
            for(auto p:{part.a,part.b,part.c,part.d})into.push_back({float(p.x),float(p.y),0xffffffff,1,0,0,0});
        });
    };
    for(auto line:lines) {
        Point a{line.first.x+offset.x,line.first.y+offset.y},b{line.second.x+offset.x,line.second.y+offset.y};
        styled_map::Quad outer{},inner{};
        if(styled_map::strokeQuad(a,b,2.5,outer) && styled_map::strokeQuad(a,b,1.0,inner)){
            append(outer,outerVertices);append(inner,innerVertices);
        }
    }
    if(overflow || sewerCasing.size()+outerVertices.size()>sewerVertexLimit || sewerCore.size()+innerVertices.size()>sewerVertexLimit)return false;
    sewerCasing.insert(sewerCasing.end(),outerVertices.begin(),outerVertices.end());
    sewerCore.insert(sewerCore.end(),innerVertices.begin(),innerVertices.end());
    return true;
}
static void recordAssetContact(void* ctx,Rect bounds) {
    if(!contactFrontier.crosses(bounds))return;
    FrameSource source{};
    if(!frameSource(ctx,&source)){
        if(++shapeFailures==1)log("Contact frontier: unsupported artwork metadata; omitting its accent.");
        return;
    }
    auto it=silhouetteCache.find(source.frame);
    if(it!=silhouetteCache.end() && (it->second.length!=source.length || it->second.flags!=source.flags ||
        it->second.width!=source.width || it->second.height!=source.height)) {
        silhouetteCache.clear();silhouetteBytes=0;it=silhouetteCache.end();
    }
    if(it==silhouetteCache.end()) {
        // Bounded, owned cache. Never retain a pointer into the game's texture
        // cache, and never read or modify its allocation/eviction metadata.
        if(silhouetteCache.size()>=4096 || silhouetteBytes>=4*1024*1024){silhouetteCache.clear();silhouetteBytes=0;}
        CachedSilhouette item;item.length=source.length;item.flags=source.flags;item.width=source.width;item.height=source.height;
        std::vector<unsigned char> bytes(source.length);
        if(copyFrameBytes(source,bytes.data()) && item.shape.decode(bytes.data(),bytes.size(),source.width,source.height)) {
            ++shapesDecoded;
            silhouetteBytes+=item.shape.rows.capacity()*sizeof(std::vector<exploration::Silhouette::Span>);
            for(const auto& row:item.shape.rows)silhouetteBytes+=row.capacity()*sizeof(exploration::Silhouette::Span);
        } else if(++shapeFailures==1)log("Contact frontier: unsupported artwork encoding; omitting its accent.");
        it=silhouetteCache.emplace(source.frame,std::move(item)).first;
    }
    contactFrontier.touch(it->second.shape,bounds);
}
// Clip the already-prepared textured quad. The native cache and crop helpers
// run once per cell, exactly as they do without the mask.
template<class Emit> static bool clipQuad(const GlideVertex* vertices,const std::vector<Rect>& clips,Emit emit) {
    float l=vertices[0].x,r=l,t=vertices[0].y,b=t;
    for(int i=0;i<4;++i) {
        const auto& v=vertices[i];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.s)||!std::isfinite(v.t))return false;
        l=std::min(l,v.x);r=std::max(r,v.x);t=std::min(t,v.y);b=std::max(b,v.y);
    }
    if(l>=r || t>=b)return true;
    int corners[4]={-1,-1,-1,-1};
    for(int i=0;i<4;++i) {
        const auto& v=vertices[i];
        if((v.x!=l && v.x!=r)||(v.y!=t && v.y!=b)||v.color!=vertices[0].color||v.scale!=vertices[0].scale)return false;
        int corner=(v.x==r?1:0)+(v.y==b?2:0);
        if(corners[corner]!=-1)return false;corners[corner]=i;
    }
    const auto& tl=vertices[corners[0]];const auto& tr=vertices[corners[1]];
    const auto& bl=vertices[corners[2]];const auto& br=vertices[corners[3]];
    if(fabs((tr.s+bl.s-tl.s)-br.s)>0.001f || fabs((tr.t+bl.t-tl.t)-br.t)>0.001f)return false;
    for(const auto& clip:clips) {
        float cl=std::max(l,float(clip.left)),cr=std::min(r,float(clip.right));
        float ct=std::max(t,float(clip.top)),cb=std::min(b,float(clip.bottom));
        if(cl>=cr || ct>=cb)continue;
        if(cl==l && cr==r && ct==t && cb==b){emit(vertices);continue;}
        GlideVertex out[4];
        for(int i=0;i<4;++i) {
            out[i]=vertices[i];out[i].x=vertices[i].x==l?cl:cr;out[i].y=vertices[i].y==t?ct:cb;
            float u=(out[i].x-l)/(r-l),v=(out[i].y-t)/(b-t);
            out[i].s=tl.s+u*(tr.s-tl.s)+v*(bl.s-tl.s);
            out[i].t=tl.t+u*(tr.t-tl.t)+v*(bl.t-tl.t);
        }
        emit(out);
    }
    return true;
}
static void __stdcall quadHook(DWORD mode,DWORD count,const void* data,DWORD stride) {
    if(!terrainClips){originalQuad(mode,count,data,stride);return;}
    if(mode!=5 || count!=4 || stride!=sizeof(GlideVertex) || !data ||
        !clipQuad(static_cast<const GlideVertex*>(data),*terrainClips,[&](const GlideVertex* out){
            originalQuad(mode,4,out,sizeof(GlideVertex));++clippedQuads;
        })) {
        enabled=false;maskActive=false;log("Mask disabled: unexpected final terrain quad.");
    }
}
static void drawPreparedCell(void* ctx,int x,int y,NativeRect* native,int mode,const std::vector<Rect>* clips=nullptr) {
    auto previous=terrainClips;terrainClips=clips;
    ++nativeCellCalls;
    originalCell(ctx,x,y,native,mode);
    terrainClips=previous;
}
static void drawStyled(const Transform& t,Rect viewport);
static void __stdcall cellHook(void* ctx,int x,int y,NativeRect* native,int mode) {
    InterlockedIncrement(&cellCount);
    if(!inPass || !enabled || (!maskActive && !nativeTownActive) || terrainClips || !native) {originalCell(ctx,x,y,native,mode);return;}
    try {
        DWORD markerId=0;
        const bool marker=mapMarkersActive() && frameIndex(ctx,&markerId) && mapMarkerDefinitions.artwork(markerId);
        if(mapMarkersActive() && !markerArtworkFile) {
            FrameSource source{};if(frameSource(ctx,&source)){markerArtworkFile=const_cast<void*>(source.file);markerDrawMode=mode;}
        }
        if(marker)nativeMarkers.emplace(markerId,x,y);
        if(styledActive && styledCurrent) {
            if(!haveViewport) {
                Transform t{};
                int w=read<int>(client,0xdbc48),h=read<int>(client,0xdbc4c);
                if(!transform(t) || w<1 || w>10000 || h<1 || h>10000){styledActive=false;}
                else {
                    passViewport=exploration::intersect({std::max(0,native->left),std::max(0,native->top+1),native->right,native->bottom+1},{0,0,w,h});
                    townInViewport=nativeTownActive && townIntersectsViewport(t,passViewport);
                    drawStyled(t,passViewport);haveViewport=true;++styledPasses;
                }
            }
            if(styledActive && !nativeArtwork() && !marker && (!nativeTownActive || !townInViewport)){++suppressed;return;}
        }
        const bool townOnly=nativeTownActive && !nativeArtwork() && (isTown(observedLevel) || styledActive);
        const bool hybrid=activeStyle==MapStyle::Hybrid && styledActive && hybridArtwork.loaded();
        bool replaceWall=false,traceSewer=false,traceWater=false;DWORD id=0;
        if(hybrid) {
            // frameBounds below validates the complete context before any draw.
            auto role=frameIndex(ctx,&id)?hybridArtwork.role(id,observedLevel):exploration::HybridArtwork::Role::Detail;
            traceSewer=role==exploration::HybridArtwork::Role::SewerWall && (observedLevel==92 || observedLevel==93);
            traceWater=role==exploration::HybridArtwork::Role::SewerWater && (observedLevel==92 || observedLevel==93);
            replaceWall=!marker && role==exploration::HybridArtwork::Role::Wall;
            if(replaceWall){++hybridWallsReplaced;if(!nativeTownActive || !townInViewport){++suppressed;return;}}
            else if(role==exploration::HybridArtwork::Role::Water || traceWater)++hybridWater;else ++hybridDetails;
        }
        Transform t{};Rect bounds{};
        if(!transform(t) || !frameBounds(ctx,x,y,&bounds)) {
            maskActive=false;enabled=false;log("Mask disabled: unrecognized frame/transform.");originalCell(ctx,x,y,native,mode);return;
        }
        // Native clipping's x limit is exclusive; its vertical geometry limits
        // are top+1 and bottom+1 (confirmed against the native crop helper).
        Rect viewport{std::max(0,native->left),std::max(0,native->top+1),native->right,native->bottom+1};
        if(!haveViewport){
            passViewport=viewport;haveViewport=true;
            int w=read<int>(client,0xdbc48),h=read<int>(client,0xdbc4c);
            Rect view=w>0 && w<10000 && h>0 && h<10000?exploration::intersect(viewport,{0,0,w,h}):Rect{};
            contactFrontier.begin(view);
            if(!townOnly)explored->frontier([&](Point a,Point b){contactFrontier.add(project(a,t),project(b,t));});
        }
        Rect assetBounds=bounds;
        if(traceSewer){bounds.left-=2;bounds.top-=2;bounds.right+=2;bounds.bottom+=2;}
        bounds=exploration::intersect(bounds,viewport);
        if(bounds.left>=bounds.right || bounds.top>=bounds.bottom){++suppressed;return;}
        // Retained native sprites often occupy only the bottom of their frame.
        // Ignore transparent padding when querying the mask; the original
        // textured quad/UVs still draw. Sewer traces need the complete canvas,
        // and native towns keep their existing route. Invalid metadata falls back.
        if(hybrid && !nativeTownActive && !traceSewer && !traceWater) {
            Rect opaque{};
            if(opaqueFrameBounds(ctx,assetBounds,&opaque)) {
                if(opaque.left>=opaque.right || opaque.top>=opaque.bottom){++blankSpritesSkipped;++suppressed;return;}
                if(opaque.left!=assetBounds.left || opaque.top!=assetBounds.top || opaque.right!=assetBounds.right || opaque.bottom!=assetBounds.bottom)++nativeBoundsTrimmed;
                bounds=exploration::intersect(bounds,opaque);
            }
        }
        if(bounds.left>=bounds.right || bounds.top>=bounds.bottom){++suppressed;return;}
        // Record all visible native water tiles before mask clipping so shared
        // internal edges cancel even when a neighboring tile is unexplored.
        int shiftX=int(t.ox)-(t.divisor==20?7:8),shiftY=int(t.oy)-(t.divisor==20?-3:-8);
        if(traceWater && sewerWater.add({assetBounds.left+shiftX,assetBounds.top+shiftY,assetBounds.right+shiftX,assetBounds.bottom+shiftY}))++sewerWaterCells;
        static std::vector<Rect> clips;clips.clear();
        auto appendClip=[&](Rect r){
            if(!clips.empty() && clips.back().left==r.left && clips.back().right==r.right && clips.back().bottom==r.top)
                clips.back().bottom=r.bottom;
            else clips.push_back(r);
        };
        const int coverage=nativeTownActive && !townOnly && !replaceWall?2:(townOnly || replaceWall?1:0);
        cachedRasterClips(bounds,int(t.divisor),shiftX,shiftY,coverage,appendClip);
        if(clips.empty()){++suppressed;return;}
        if(traceSewer) {
            if(queueSewerWall(ctx,id,assetBounds,clips)){++sewerTraced;++suppressed;return;}
            ++sewerFallbacks; // Unsupported artwork/budget: preserve the native wall.
        }
        if(nativeTownActive)++townClippedCells;else if(!styledActive)recordAssetContact(ctx,assetBounds);
        if(clips.size()==1 && clips[0].left==bounds.left && clips[0].right==bounds.right &&
           clips[0].top==bounds.top && clips[0].bottom==bounds.bottom) {
            drawPreparedCell(ctx,x,y,native,mode);++drawn;return;
        }
        ++partial;
        drawPreparedCell(ctx,x,y,native,mode,&clips);
    } catch(...) {styledBatch=nullptr;frontierBatch=nullptr;fractionalFrontier=false;terrainClips=nullptr;styledActive=false;maskActive=false;enabled=false;log("Mask disabled after C++ exception.");}
}
static void beginPass() {
    InterlockedIncrement(&beginCount);
    sewerCasing.clear();sewerCore.clear();sewerWater.clear();sewerWaterFill.clear();
    nativeMarkers.clear();markerArtworkFile=nullptr;
    QueryPerformanceCounter(&passStarted);
    try {update();sampleMapMarkers(GetTickCount());if(isTown(observedLevel))InterlockedIncrement(&townPasses);haveViewport=false;inPass=true;} catch(...) {maskActive=false;enabled=false;}
}
static void submitLine(Point start,Point end) {
    if(hypot(end.x-start.x,end.y-start.y)<1e-6)return;
    GlideVertex a{float(start.x),float(start.y),0xffffffff,1,0,0,0};
    GlideVertex b{float(end.x),float(end.y),0xffffffff,1,0,0,0};
    originalFloatLine(&a,&b);
    InterlockedIncrement(&fractionalCount);
}
static void submitFractionalLine() {
    if(fractionalTint && styledColor)styledColor(overlayColor(fractionalTint));
    if(frontierBatch)for(const auto& line:*frontierBatch)submitLine(line.first,line.second);
    else submitLine(frontierA,frontierB);
    if(fractionalTint && styledColor)styledColor(0x84848400u|overlayAlpha(255));
}
static void __stdcall floatLineHook(const void* a,const void* b) {
    if(styledBatch) {
        styledColor(overlayColor(styledBatchColor));styledArray(5,DWORD(styledBatch->size()),styledBatch->data());styledColor(styledRestoreColor);
    } else if(fractionalFrontier)submitFractionalLine();else originalFloatLine(a,b);
}
static void __stdcall floatPointHook(const void* a) {
    // A short segment may round to one pixel in D2gfx; preserve it as a line.
    if(styledBatch) {
        styledColor(overlayColor(styledBatchColor));styledArray(5,DWORD(styledBatch->size()),styledBatch->data());styledColor(styledRestoreColor);
    } else if(fractionalFrontier)submitFractionalLine();else originalFloatPoint(a);
}
static void drawFrontier(Point a,Point b,DWORD tint=0) {
    frontierA=a;frontierB=b;fractionalFrontier=true;fractionalTint=tint;
    struct ResetTint { ~ResetTint(){fractionalFrontier=false;fractionalTint=0;} } resetTint;
    // The native path sets/restores its own color and blend state. Replace only
    // its final vertex positions, after integer conversion, with fractional ones.
    originalLine(int(lround(a.x)),int(lround(a.y)),int(lround(b.x)),int(lround(b.y)),frontierColor,overlayAlpha(255));
}
static void drawHybridWalls(const Transform& t,Rect viewport) {
    static std::vector<GlideVertex> casing,core;
    static std::vector<const void*> pointers;
    static std::vector<Rect> clips;
    casing.clear();core.clear();
    const int shiftX=int(t.ox)-(t.divisor==20?7:8),shiftY=int(t.oy)-(t.divisor==20?-3:-8);
    for(const auto& wall:styledCurrent->drawing.walls) {
        const auto a=project(wall.a,t),b=project(wall.b,t);
        styled_map::Quad outer{},inner{};
        if(!styled_map::strokeQuad(a,b,2.5,outer) || !styled_map::strokeQuad(a,b,1.0,inner))continue;
        Rect bounds{int(floor(std::min({outer.a.x,outer.b.x,outer.c.x,outer.d.x}))),
            int(floor(std::min({outer.a.y,outer.b.y,outer.c.y,outer.d.y}))),
            int(ceil(std::max({outer.a.x,outer.b.x,outer.c.x,outer.d.x}))),
            int(ceil(std::max({outer.a.y,outer.b.y,outer.c.y,outer.d.y})))};
        bounds=exploration::intersect(bounds,viewport);
        if(bounds.left>=bounds.right || bounds.top>=bounds.bottom)continue;
        // Clip the full width, including the dark casing, to explored pixels.
        // Vertically merge runs before clipping polygons to keep submissions small.
        clips.clear();
        cachedRasterClips(bounds,int(t.divisor),shiftX,shiftY,0,[&](Rect r){
            if(!clips.empty() && clips.back().left==r.left && clips.back().right==r.right && clips.back().bottom==r.top)
                clips.back().bottom=r.bottom;else clips.push_back(r);
        });
        auto append=[&](const styled_map::Quad& q,std::vector<GlideVertex>& into){
            for(auto clip:clips)styled_map::clipQuad(q,clip,[&](const styled_map::Quad& part){
                for(auto p:{part.a,part.b,part.c,part.d})into.push_back({float(p.x),float(p.y),0xffffffff,1,0,0,0});
            });
        };
        append(outer,casing);append(inner,core);
    }
    auto submit=[&](const std::vector<GlideVertex>& vertices,DWORD color){
        if(vertices.empty())return;pointers.clear();
        for(const auto& vertex:vertices)pointers.push_back(&vertex);
        styledBatchColor=color;styledRestoreColor=0x84848400u|(color&255);styledBatch=&pointers;
        originalLine(viewport.left,viewport.top,viewport.left+1,viewport.top,frontierColor,color&255);
        styledBatch=nullptr;styledQuadCount+=static_cast<unsigned long>(vertices.size()/4);
    };
    submit(casing,0x181818c0);submit(core,exploration::wallRGBA(wallColor,148,224));
}
static void drawStyled(const Transform& t,Rect viewport) {
    if(viewport.left>=viewport.right || viewport.top>=viewport.bottom)return;
    struct ClearSubmission {
        ~ClearSubmission(){styledBatch=nullptr;frontierBatch=nullptr;fractionalFrontier=false;}
    } clearSubmission;
    static std::vector<GlideVertex> vertices;
    static std::vector<const void*> pointers;
    const bool prepared=preparedFloors && preparedOwner==styledCurrent && preparedSerial==gameSerial;
    for(std::size_t i=0;i<styledCurrent->drawing.layers.size();++i)for(int red:{0,1}) {
        const auto& layer=styledCurrent->drawing.layers[i];
        vertices.clear();pointers.clear();
        auto append=[&](const styled_map::Quad& clipped){
            for(auto p:{clipped.a,clipped.b,clipped.c,clipped.d})vertices.push_back({float(p.x),float(p.y),0xffffffff,1,0,0,0});
        };
        if(prepared)preparedFloors->clip(i,red!=0,int(t.divisor),t.ox,t.oy,viewport,append);
        else for(const auto& q:red?layer.redQuads:layer.quads) {
            styled_map::Quad screen{project(q.a,t),project(q.b,t),project(q.c,t),project(q.d,t)};
            styled_map::clipQuad(screen,viewport,append);
        }
        if(vertices.empty())continue;
        for(const auto& vertex:vertices)pointers.push_back(&vertex);
        styledBatchColor=red?exploration::boundaryRGBA(boundaryColor,layer.gray,layer.alpha):
            (layer.gray*0x01010100u)|layer.alpha;
        styledRestoreColor=0x84848400u|layer.alpha;
        styledBatch=&pointers;
        originalLine(viewport.left,viewport.top,viewport.left+1,viewport.top,frontierColor,layer.alpha);
        styledBatch=nullptr;styledQuadCount+=static_cast<unsigned long>(vertices.size()/4);
    }
    // Campaign native mode keeps the game's recognizable wall/stair shapes.
    // The cached floor shading and red reveal bands still draw underneath.
    if(activeStyle==MapStyle::Hybrid){
        // Sewer wall faces and floor-edge contours are offset from one another.
        // Their matching artwork traces are queued by cellHook and batched in
        // endPass; unsupported frames keep their native wall instead.
        if(observedLevel!=92 && observedLevel!=93)drawHybridWalls(t,viewport);
        return;
    }
    if(nativeArtwork())return;
    std::vector<std::pair<Point,Point>> walls;
    for(const auto& wall:styledCurrent->drawing.walls) {
        Point a=project(wall.a,t),b=project(wall.b,t);
        if(styled_map::clipStroke(a,b,viewport))walls.push_back({a,b});
    }
    if(!walls.empty()) {
        frontierBatch=&walls;
        drawFrontier(walls[0].first,walls[0].second,
            wallColor==exploration::BoundaryColor::Gray?0:exploration::wallRGBA(wallColor,132,255));
        frontierBatch=nullptr;
    }
}
static void queueWaterGeometry(const Transform& t,Rect viewport) {
    static std::vector<Rect> clips;
    const int shiftX=int(t.ox)-(t.divisor==20?7:8),shiftY=int(t.oy)-(t.divisor==20?-3:-8);
    auto clipBounds=[&](Rect bounds){
        clips.clear();bounds=exploration::intersect(bounds,viewport);
        if(bounds.left>=bounds.right || bounds.top>=bounds.bottom)return;
        explored->clipProjected(bounds,int(t.divisor),shiftX,shiftY,[&](Rect r){
            if(!clips.empty() && clips.back().left==r.left && clips.back().right==r.right && clips.back().bottom==r.top)clips.back().bottom=r.bottom;
            else clips.push_back(r);
        });
    };
    for(auto tile:sewerWater.tiles()) {
        const int x=tile[0]-shiftX,b=tile[1]-shiftY,w=tile[2];clipBounds({x,b-w/2,x+w,b});
        styled_map::Quad q{{double(x),b-w*.25},{x+w*.5,b-w*.5},{double(x+w),b-w*.25},{x+w*.5,double(b)}};
        for(auto clip:clips)styled_map::clipQuad(q,clip,[&](const styled_map::Quad& part){
            if(sewerWaterFill.size()+4>sewerVertexLimit)return;
            for(auto p:{part.a,part.b,part.c,part.d})sewerWaterFill.push_back({float(p.x),float(p.y),0xffffffff,1,0,0,0});
        });
    }
    static std::vector<std::pair<Point,Point>> line(1);
    for(auto edge:sewerWater.edges()) {
        edge[0]-=shiftX;edge[2]-=shiftX;edge[1]-=shiftY;edge[3]-=shiftY;
        clipBounds({std::min(edge[0],edge[2])-2,std::min(edge[1],edge[3])-2,std::max(edge[0],edge[2])+2,std::max(edge[1],edge[3])+2});
        line[0]={{double(edge[0]),double(edge[1])},{double(edge[2]),double(edge[3])}};
        if(!queueTraceLines(line,{0,0},clips))break;
    }
}
static void drawMapMarkers() {
    if(!inPass || !mapMarkersActive() || !haveViewport || !markerArtworkFile)return;
    Transform t{};if(!transform(t))return;
    NativeRect viewport{passViewport.left,passViewport.right,passViewport.top-1,passViewport.bottom-1};
    for(const auto& marker:mapMarkers) {
        if(!explored->contains({marker.x,marker.y}))continue;
        const int x=marker.mapX*10/int(t.divisor)-int(t.ox),y=marker.mapY*10/int(t.divisor)-int(t.oy);
        if(marker.nativeRegistered || nativeMarkers.count({marker.frame,x,y})){++markersAlreadyNative;continue;}
        // Same zero-initialized 0x48-byte context built by native +0x60250.
        // The original renderer owns all texture preparation and lifetimes.
        DWORD context[18]{};context[0]=marker.frame;
        memcpy(reinterpret_cast<unsigned char*>(context)+0x34,&markerArtworkFile,sizeof(markerArtworkFile));
        Rect bounds{};if(!frameBounds(context,x,y,&bounds))continue;
        bounds=exploration::intersect(bounds,passViewport);
        if(bounds.left>=bounds.right || bounds.top>=bounds.bottom)continue;
        cellHook(context,x,y,&viewport,markerDrawMode);++markersDrawn;
    }
}
static void endPass() {
    InterlockedIncrement(&endCount);
    try {
        if(inPass && maskActive && haveViewport && styledActive && activeStyle==MapStyle::Hybrid && styledArray && styledColor) {
            if(!sewerWater.tiles().empty()){Transform t{};if(transform(t))queueWaterGeometry(t,passViewport);}
            static std::vector<const void*> pointers;
            auto submit=[&](const std::vector<GlideVertex>& vertices,DWORD color){
                if(vertices.empty())return;pointers.clear();for(const auto& vertex:vertices)pointers.push_back(&vertex);
                styledBatch=&pointers;styledBatchColor=color;styledRestoreColor=0x84848400u|(color&255);
                originalLine(passViewport.left,passViewport.top,passViewport.left+1,passViewport.top,frontierColor,color&255);
                styledBatch=nullptr;styledQuadCount+=static_cast<unsigned long>(vertices.size()/4);
            };
            submit(sewerWaterFill,0x56606438);submit(sewerCasing,0x181818c0);
            submit(sewerCore,exploration::wallRGBA(wallColor,148,224));
        }
        sewerCasing.clear();sewerCore.clear();sewerWater.clear();sewerWaterFill.clear();
        if(inPass && maskActive && haveViewport && !styledActive && !nativeTownActive) {
            std::vector<std::pair<Point,Point>> lines;
            contactFrontier.emit([&](Point a,Point b){lines.push_back({a,b});InterlockedIncrement(&frontierCount);});
            if(!lines.empty()) {
                frontierBatch=&lines;
                drawFrontier(lines[0].first,lines[0].second,exploration::boundaryRGBA(boundaryColor,100,255));
                frontierBatch=nullptr;
            }
        }
        drawMapMarkers();
        markerArtworkFile=nullptr;
        if(inPass) {
            LARGE_INTEGER finished{};QueryPerformanceCounter(&finished);
            mapTicks+=finished.QuadPart-passStarted.QuadPart;++mapSamples;
        }
        inPass=false;
        if(drawn+suppressed+partial && GetTickCount()-lastReport>=1000 && logfile){
            lastReport=GetTickCount();
            double ms=counterFrequency.QuadPart && mapSamples?1000.0*mapTicks/counterFrequency.QuadPart/mapSamples:0;
            fprintf(logfile,"cells=%lu suppressed=%lu partial=%lu explored=%zu frontier=%ld fractional=%ld townPasses=%ld nativeCells=%lu clippedQuads=%lu mapMs=%.3f shapes=%lu shapeFailures=%lu styledPasses=%lu styledQuads=%lu floorCells=%zu workerBuildMs=%.3f revealLatencyMs=%.3f rebuiltChunks=%zu totalChunks=%zu townNativeCells=%lu townPreviewPasses=%lu previewLevel=%lu townClipActive=%d\n",drawn,suppressed,partial,explored->size(),frontierCount,fractionalCount,townPasses,nativeCellCalls,clippedQuads,ms,shapesDecoded,shapeFailures,styledPasses,styledQuadCount,styledCurrent?styledCurrent->floorCells:0,styledBuildMs,styledLatencyMs,styledRebuiltChunks,styledTotalChunks,townClippedCells,townPreviewPasses,townBoundary.outside,nativeTownActive);fflush(logfile);
            fprintf(logfile,"DRAW style=%s floorQuads=%zu wallRuns=%zu preparedFloors=%d\n",styleName(activeStyle),
                styledCurrent?styledCurrent->drawing.quads:0,styledCurrent?styledCurrent->drawing.walls.size():0,
                int(preparedFloors && preparedOwner==styledCurrent && preparedSerial==gameSerial));
            fprintf(logfile,"DISCOVERY mode=hardcoded radiusSubtiles=%.2f\n",revealRadius*maskCellSize);
            if(mapMarkersActive())fprintf(logfile,"MARKERS sampled=%zu added=%lu alreadyNative=%lu\n",mapMarkers.size(),markersDrawn,markersAlreadyNative);
            if(activeStyle==MapStyle::Hybrid){fprintf(logfile,"HYBRID wallsReplaced=%lu detailsRetained=%lu waterRetained=%lu sewerTraced=%lu sewerFallbacks=%lu sewerWaterCells=%lu layerWaits=%lu clipHits=%zu clipMisses=%zu\n",
                hybridWallsReplaced,hybridDetails,hybridWater,sewerTraced,sewerFallbacks,sewerWaterCells,layerWaits,rasterClips.hits(),rasterClips.misses());fflush(logfile);}
            if(activeStyle==MapStyle::Hybrid){fprintf(logfile,"ARTWORK trimmed=%lu blankSkipped=%lu boundsHits=%zu boundsDecoded=%zu boundsFailures=%zu\n",
                nativeBoundsTrimmed,blankSpritesSkipped,artworkBounds.hits(),artworkBounds.decoded(),artworkBounds.failures());fflush(logfile);}
            mapTicks=0;mapSamples=0;}
    } catch(...) {styledBatch=nullptr;frontierBatch=nullptr;fractionalFrontier=false;inPass=false;maskActive=false;enabled=false;}
}
__declspec(naked) static void beginStub() {
    __asm {
        call dword ptr [originalBegin]
        pushfd
        pushad
        call beginPass
        popad
        popfd
        ret
    }
}
__declspec(naked) static void endStub() {
    __asm {
        pushfd
        pushad
        call endPass
        popad
        popfd
        jmp dword ptr [originalEnd]
    }
}
struct CallPatch { unsigned char* site;void* replacement;unsigned char old[5]; };
static void* target(unsigned char* p){return p+5+read<int>(p,1);}
static bool inModule(void* address,HMODULE mod) {
    if(!mod)return false;
    auto base=reinterpret_cast<unsigned char*>(mod);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    return reinterpret_cast<uintptr_t>(address)>=reinterpret_cast<uintptr_t>(base) &&
           reinterpret_cast<uintptr_t>(address)<reinterpret_cast<uintptr_t>(base)+nt->OptionalHeader.SizeOfImage;
}
static bool bindMenuState(HMODULE module) {
    if(!module)return false;
    __try {
        auto win=reinterpret_cast<unsigned char*>(module);
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(win);
        if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096)return false;
        auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(win+dos->e_lfanew);
        if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage!=0xcf000 ||
           nt->FileHeader.TimeDateStamp!=0x4b95c21d)return false;
        const auto address=reinterpret_cast<uintptr_t>(win+0x214a0);
        // Inspected native control-list append paths, checked after relocation.
        if(win[0x1165d]!=0xa1 || read<uintptr_t>(win,0x1165e)!=address ||
           win[0x11667]!=0x89 || win[0x11668]!=0x35 || read<uintptr_t>(win,0x11669)!=address ||
           win[0x9dd2]!=0xa1 || read<uintptr_t>(win,0x9dd3)!=address)return false;
        firstMenuControl=reinterpret_cast<void**>(address);return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool menuState(bool* hasPlayer,bool* hasControls) {
    __try {
        if(!firstMenuControl || !client)return false;
        // A missing room/path during a portal load does not mean the player
        // unit is gone. Only the unit pointer itself participates in this check.
        *hasPlayer=read<void*>(client,0x11bbfc)!=nullptr;
        *hasControls=*firstMenuControl!=nullptr;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static MapStyle parseStyle(const char* value,MapStyle fallback) {
    if(_stricmp(value,"hybrid")==0)return MapStyle::Hybrid;
    if(_stricmp(value,"native")==0)return MapStyle::Native;
    if(_stricmp(value,"styled")==0)return MapStyle::Styled;
    if(_stricmp(value,"original")==0)return MapStyle::Original;
    return fallback;
}
static bool saveMapColor(const char* key,exploration::BoundaryColor color,exploration::BoundaryColor& current) {
    if(static_cast<unsigned>(color)>=exploration::boundaryPresets.size())return false;
    if(color==current)return true;
    const auto& preset=exploration::boundaryPreset(color);
    if(settingsPath.empty() || !WritePrivateProfileStringA("Automap",key,preset.key,settingsPath.c_str())) {
        log("Map color unchanged: unable to save ExplorationMask.ini.");return false;
    }
    current=color;
    if(logfile){fprintf(logfile,"APPEARANCE %s=%s; saved, geometry unchanged.\n",key,preset.key);fflush(logfile);}
    return true;
}
static bool selectBoundaryColor(exploration::BoundaryColor color) {return saveMapColor("BoundaryColor",color,boundaryColor);}
static bool selectWallColor(exploration::BoundaryColor color) {return saveMapColor("WallColor",color,wallColor);}
static void ensureBoundaryMenu() {
    static bool attempted=false;
    if(attempted)return;
    auto pd=reinterpret_cast<unsigned char*>(GetModuleHandleA("ProjectDiablo.dll"));
    if(!pd)return;
    attempted=true;
    const auto ok=exploration::boundary_menu::install(pd,client,
        reinterpret_cast<unsigned char*>(GetModuleHandleA("D2Win.dll")),selectBoundaryColor,[]{return boundaryColor;},
        selectWallColor,[]{return wallColor;});
    log(ok?"BOUNDARY menu installed in native Automap Options; boundary and wall color lists.":
        "BOUNDARY menu unavailable: supported menu signatures differ. INI color remains available.");
}
static void loadAppearanceSettings(const std::string& settings) {
    settingsPath=settings;
    char boundary[32]{};
    GetPrivateProfileStringA("Automap","BoundaryColor","red",boundary,32,settings.c_str());
    boundaryColor=exploration::parseBoundaryColor(boundary);
    char wall[32]{};
    GetPrivateProfileStringA("Automap","WallColor","gray",wall,32,settings.c_str());
    wallColor=exploration::parseBoundaryColor(wall,exploration::BoundaryColor::Gray);
    if(logfile){fprintf(logfile,"APPEARANCE BoundaryColor=%s WallColor=%s\n",
        exploration::boundaryPreset(boundaryColor).key,exploration::boundaryPreset(wallColor).key);fflush(logfile);}
    char campaign[32]{},maps[32]{};
    GetPrivateProfileStringA("Automap","CampaignStyle","hybrid",campaign,32,settings.c_str());
    GetPrivateProfileStringA("Automap","MapsStyle","hybrid",maps,32,settings.c_str());
    campaignStyle=parseStyle(campaign,MapStyle::Hybrid);mapsStyle=parseStyle(maps,MapStyle::Hybrid);
    auto opacity=GetPrivateProfileIntA("Automap","OverlayOpacity",80,settings.c_str());
    overlayOpacity=opacity>=10 && opacity<=100?opacity:80;
    if(logfile){fprintf(logfile,"OVERLAY opacity=%u%%; custom geometry only. Tested D2GL corner-map capture retains its fixed alpha.\n",overlayOpacity);fflush(logfile);}
}
static void loadStyles() {
    // D2GL initializes us before PD2 finishes mounting its archives. Read the
    // settings now, but resolve game tables only after a valid player exists.
    gameTablesPending=true;
    if(logfile){fprintf(logfile,"DISCOVERY mode=hardcoded; radius=%.2f subtiles; smooth circular reveal.\n",revealRadius*maskCellSize);fflush(logfile);}
    char path[MAX_PATH]{};
    DWORD length=GetModuleFileNameA(nullptr,path,MAX_PATH);
    if(!length || length>=MAX_PATH){log("Style settings unavailable; using defaults.");return;}
    auto slash=strrchr(path,'\\');if(!slash)return;
    std::string settings(path,slash+1);settings+="ExplorationMask.ini";
    loadAppearanceSettings(settings);
}
static void loadGameTables(const exploration::GameFiles& api) {
    campaignLayers=exploration::CampaignLayers{};
    mapMarkerDefinitions=exploration::MapMarkerDefinitions{};mapMarkers.clear();
    std::string bytes;
    auto readTable=[&](const char* name) {
        std::string path="data\\global\\excel\\";path+=name;
        auto result=exploration::readGameTable(api,path.c_str(),bytes);
        if(logfile){fprintf(logfile,"TABLE %s: %s; bytes=%zu; source=%s.\n",
            name,exploration::tableReadName(result),bytes.size(),
            api.archiveOnly?"mounted archives (table diagnostic)":"game file resolver");fflush(logfile);}
        return result==exploration::TableRead::Ready;
    };
    bool layersReady=false;
    if(readTable("Levels.txt")) {
        std::istringstream levels(std::move(bytes));layersReady=campaignLayers.load(levels);
    }
    if(layersReady) {
        if(logfile){fprintf(logfile,"CAMPAIGN layers=%zu; adjoining areas share state only on the expected native layer.\n",campaignLayers.size());fflush(logfile);}
    } else log("Campaign layer definitions unavailable; retaining separate per-area exploration caches.");
    std::string objectBytes;
    const bool objectsReady=readTable("Objects.txt");if(objectsReady)objectBytes=std::move(bytes);
    if(objectsReady){std::istringstream objects(objectBytes);mapMarkerDefinitions.loadObjects(objects);}
    if(mapsStyle!=MapStyle::Original) {
        std::string monsterBytes;
        if(readTable("MonStats.txt"))monsterBytes=std::move(bytes);
        if(!monsterBytes.empty() && readTable("MonStats2.txt")) {
            std::istringstream stats(std::move(monsterBytes)),extra(std::move(bytes));mapMarkerDefinitions.loadMonsters(stats,extra);
        }
    }
    if(logfile){fprintf(logfile,"MARKERS definitions objects=%zu eventActors=%zu; loaded endgame units only, explored locations only.\n",
        mapMarkerDefinitions.objects(),mapMarkerDefinitions.monsters());fflush(logfile);}
    if(campaignStyle==MapStyle::Hybrid || mapsStyle==MapStyle::Hybrid) {
        bool ready=false;
        if(readTable("automap.txt")) {
            std::istringstream definitions(std::move(bytes));ready=hybridArtwork.load(definitions);
        }
        if(ready) {
            ready=objectsReady;
            if(ready){std::istringstream objects(objectBytes);ready=hybridArtwork.protectObjects(objects);}
        }
        if(!ready) {
            if(campaignStyle==MapStyle::Hybrid)campaignStyle=MapStyle::Native;
            if(mapsStyle==MapStyle::Hybrid)mapsStyle=MapStyle::Native;
            log("Hybrid artwork definitions unavailable; retaining native exploration style.");
        } else if(logfile){fprintf(logfile,"HYBRID classified %zu ordinary wall artwork IDs; %zu Poisoned Well contour IDs; native details/water protected.\n",hybridArtwork.walls(),hybridArtwork.poisonedWellContours());fflush(logfile);}
    }
    if(logfile){fprintf(logfile,"STYLE campaign=%d maps=%d (0=original, 1=native exploration, 2=styled, 3=hybrid); native shrine/event icons; no quest or exit arrows.\n",
        int(campaignStyle),int(mapsStyle));fflush(logfile);}
}
static void ensureGameTables() {
    if(!gameTablesPending)return;
    gameTablesPending=false; // No table I/O, parsing or retries during exploration.
    try {
        auto files=exploration::GameFiles::bind(GetModuleHandleA("Storm.dll"));
        // A table-only diagnostic reproduces missing loose TXT files without
        // disabling -direct for the rest of a custom test installation.
        files.archiveOnly=strstr(GetCommandLineA(),"-exploration-archive-tables")!=nullptr;
        loadGameTables(files);
    }
    catch(...) {
        campaignLayers=exploration::CampaignLayers{};
        if(campaignStyle==MapStyle::Hybrid)campaignStyle=MapStyle::Native;
        if(mapsStyle==MapStyle::Hybrid)mapsStyle=MapStyle::Native;
        log("Game table loading failed; retaining native artwork and separate area histories.");
    }
}
static DWORD WINAPI diagnosticThread(void*) {
    // Reuse the existing watcher for process-lifetime menu detection. It only
    // publishes an atomic epoch; all mask/cache changes stay on the draw thread.
    exploration::MenuObserver menus;unsigned ticks=0,reports=0;
    for(;;) {
        Sleep(50);
        bool hasPlayer=false,hasControls=false;
        if(menuState(&hasPlayer,&hasControls) && menus.sample(hasPlayer,hasControls))InterlockedIncrement(&menuEpoch);
        if(++ticks%20 || reports>=120)continue;
        ++reports;
        PlayerState p{};bool ok=playerState(&p);
        auto b=client+0x6269e,e=client+0xc3aa1;
        if(logfile) {
            fprintf(logfile,"TRACE begin=%ld end=%ld cell=%ld failed=%ld level=%ld directLevel=%lu readOK=%d frontier=%ld beginChanged=%d endChanged=%d menuEpoch=%ld menuControls=%d playerUnit=%d\n",
                beginCount,endCount,cellCount,stateFailure,observedLevel,ok?p.level:0,ok,frontierCount,
                *b!=0xe8 || target(b)!=expectedBeginTarget,*e!=0xe8 || target(e)!=expectedEndTarget,
                InterlockedCompareExchange(&menuEpoch,0,0),hasControls,hasPlayer);
            fflush(logfile);
        }
    }
}
extern "C" __declspec(dllexport) void __cdecl InitExplorationMask() {
    if(installed)return;
    QueryPerformanceFrequency(&counterFrequency);
    logfile=_fsopen("ExplorationMask.log","w",_SH_DENYNO);
    log("Initializer reached.");
    // Command-line opt-in survives Windows elevation; environment alone may not.
    if(!strstr(GetCommandLineA(),"-exploration-test")) {log("Dormant: test-launch flag absent.");return;}
    client=reinterpret_cast<unsigned char*>(GetModuleHandleA("D2Client.dll"));
    auto glide=reinterpret_cast<unsigned char*>(GetModuleHandleA("D2Glide.dll"));
    auto gfx=GetModuleHandleA("D2gfx.dll"),wrapper=GetModuleHandleA("glide3x.dll");
    if(!client || !glide || !gfx || !wrapper){log("Not installed: expected Glide modules unavailable.");return;}
    if(!bindMenuState(GetModuleHandleA("D2Win.dll"))){log("Not installed: menu/session reader layout differs.");return;}
    try {loadStyles();}catch(...) {campaignStyle=MapStyle::Native;mapsStyle=MapStyle::Native;
        log("Hybrid/style settings unavailable; using native exploration styling.");}
    auto b=client+0x6269e,e=client+0xc3aa1,c=client+0x604ea,q=glide+0xa33f;
    auto l=glide+0x94ba,p=glide+0x944c;
    if(*b!=0xe8 || *e!=0xe8 || *c!=0xe8 || *q!=0xe8 || *l!=0xe8 || *p!=0xe8 ||
        target(l)!=glide+0x604a || target(p)!=glide+0x6044 || target(c)!=client+0xd162 || target(q)!=glide+0x6032 ||
        !(target(b)==client+0x5f9f0 || inModule(target(b),wrapper)) ||
        !(target(e)==client+0xbf0d0 || inModule(target(e),wrapper))) {
        log("Not installed: hook signature or renderer chain mismatch.");return;
    }
    originalBegin=target(b);originalEnd=target(e);originalCell=reinterpret_cast<CellDraw>(target(c));
    originalQuad=reinterpret_cast<QuadDraw>(target(q));
    originalLine=reinterpret_cast<LineDraw>(GetProcAddress(gfx,MAKEINTRESOURCEA(10010)));
    originalFloatLine=reinterpret_cast<FloatLineDraw>(target(l));
    originalFloatPoint=reinterpret_cast<FloatPointDraw>(target(p));
    if(!originalLine){log("Not installed: line renderer unavailable.");return;}
    auto array=GetProcAddress(wrapper,"_grDrawVertexArray@12"),color=GetProcAddress(wrapper,"_grConstantColorValue@4");
    if(reinterpret_cast<const unsigned char*>(array)==reinterpret_cast<const unsigned char*>(wrapper)+0xb4020 &&
       reinterpret_cast<const unsigned char*>(color)==reinterpret_cast<const unsigned char*>(wrapper)+0xb4210) {
        try {
            styledWorker=new styled_map::Worker;
            styledArray=reinterpret_cast<VertexArrayDraw>(array);styledColor=reinterpret_cast<ConstantColor>(color);
        } catch(...) {log("Styled worker unavailable; retaining normal artwork clipping.");}
    } else log("Styled map unavailable: renderer export layout differs.");
    CallPatch patches[]={{b,reinterpret_cast<void*>(beginStub),{}},{e,reinterpret_cast<void*>(endStub),{}},
        {c,reinterpret_cast<void*>(cellHook),{}},{q,reinterpret_cast<void*>(quadHook),{}},
        {l,reinterpret_cast<void*>(floatLineHook),{}},{p,reinterpret_cast<void*>(floatPointHook),{}}};
    // D2GL invokes this exported initializer during window creation, before
    // normal rendering. Hold all affected pages writable before any mutation.
    DWORD protections[6]{};int ready=0;
    for(auto& patch:patches) {
        memcpy(patch.old,patch.site,5);
        if(!VirtualProtect(patch.site,5,PAGE_EXECUTE_READWRITE,&protections[ready]))break;
        ++ready;
    }
    if(ready==6) {
        for(auto& patch:patches) {
            int delta=static_cast<int>(reinterpret_cast<uintptr_t>(patch.replacement)-reinterpret_cast<uintptr_t>(patch.site+5));
            memcpy(patch.site+1,&delta,4);FlushInstructionCache(GetCurrentProcess(),patch.site,5);
        }
        installed=true;expectedBeginTarget=reinterpret_cast<void*>(beginStub);expectedEndTarget=reinterpret_cast<void*>(endStub);
        log("Installed memory hooks with the selected INI styles. Hybrid: cased modern wall/shore contours; sewer highlights follow native artwork with fallback, while water/roads/icons remain native. Native town footprints and gate previews retained. Session resets use menu/player/seed identity, never automap timing. No added quest markers or exit arrows.");
    }
    for(int i=ready-1;i>=0;--i){DWORD ignored;VirtualProtect(patches[i].site,5,protections[i],&ignored);}
    if(!installed)log("Not installed: could not prepare code pages; no hooks changed.");
    else {HANDLE thread=CreateThread(nullptr,0,diagnosticThread,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);else {enabled=false;log("Mask disabled: session watcher could not start.");}}
}
