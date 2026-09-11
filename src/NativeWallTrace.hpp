// Derives thin centerlines from native wall silhouettes in sprite pixels.
#pragma once
#include "FrontierContacts.hpp"
#include "HybridArtwork.hpp"

namespace exploration {
class NativeWallTrace {
    static double distanceSquared(Point p,Point a,Point b) {
        const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
        const double t=length?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy)/length,0.0,1.0):0;
        const double x=p.x-a.x-t*dx,y=p.y-a.y-t*dy;return x*x+y*y;
    }
    void addRun(std::vector<Point> points,bool horizontal) {
        if(points.empty())return;
        if(points.size()==1) {
            auto a=points[0],b=a;
            if(horizontal){a.x-=.5;b.x+=.5;}else{a.y-=.5;b.y+=.5;}
            lines.push_back({a,b});return;
        }
        auto first=points[0],second=points[1],last=points.back(),previous=points[points.size()-2];
        points.front()={first.x-(second.x-first.x)*.5,first.y-(second.y-first.y)*.5};
        points.back()={last.x+(last.x-previous.x)*.5,last.y+(last.y-previous.y)*.5};
        std::vector<bool> keep(points.size(),false);keep.front()=keep.back()=true;
        std::vector<std::pair<std::size_t,std::size_t>> pending{{0,points.size()-1}};
        while(!pending.empty()) {
            auto range=pending.back();pending.pop_back();double farthest=.65*.65;std::size_t split=range.first;
            for(auto i=range.first+1;i<range.second;++i) {
                double distance=distanceSquared(points[i],points[range.first],points[range.second]);
                if(distance>farthest){farthest=distance;split=i;}
            }
            if(split!=range.first){keep[split]=true;pending.push_back({range.first,split});pending.push_back({split,range.second});}
        }
        std::size_t lastKept=0;
        for(std::size_t i=1;i<points.size();++i)if(keep[i]){lines.push_back({points[lastKept],points[i]});lastKept=i;}
    }
public:
    std::vector<std::pair<Point,Point>> lines;
    bool buildSewer(const Silhouette& shape,int width,int height,HybridArtwork::SewerShape form) {
        lines.clear();
        if((width!=16 && width!=8) || height!=width*2 || shape.rows.size()!=std::size_t(height))return false;
        for(const auto& row:shape.rows)for(auto span:row)if(span.first<0 || span.first>=span.second || span.second>width)return false;
        Point left{0.,height-width*.25},peak{width*.5,height-width*.5},right{double(width),height-width*.25};
        using Form=HybridArtwork::SewerShape;
        if(form==Form::Down)lines.push_back({peak,right});
        else if(form==Form::Up)lines.push_back({left,peak});
        else if(form==Form::Peak){lines.push_back({left,peak});lines.push_back({peak,right});}
        else if(form==Form::Cap)lines.push_back({{peak.x-.5,peak.y},{peak.x+.5,peak.y}});
        else return false;
        // These shared anchors and exact 2:1 slopes remove per-sprite scallops.
        // Reject changed artwork whose pixels no longer support this topology.
        for(auto line:lines)for(int i=0;i<=16;++i) {
            Point p{line.first.x+(line.second.x-line.first.x)*i/16.,line.first.y+(line.second.y-line.first.y)*i/16.};
            double nearest=1e9;
            for(int y=0;y<height;++y)for(auto span:shape.rows[y])for(int x=span.first;x<span.second;++x)
                nearest=std::min(nearest,(p.x-x-.5)*(p.x-x-.5)+(p.y-y-.5)*(p.y-y-.5));
            if(nearest>4.){lines.clear();return false;}
        }
        return true;
    }
    bool build(const Silhouette& shape,int width,int height) {
        lines.clear();if(width<1 || width>512 || height<1 || height>512 || shape.rows.size()!=std::size_t(height))return false;
        std::vector<int> top(width,height),bottom(width,-1),left(height,width),right(height,-1);
        int minX=width,maxX=-1,minY=height,maxY=-1;
        for(int y=0;y<height;++y)for(auto span:shape.rows[y]) {
            if(span.first<0 || span.second>width || span.first>=span.second)return false;
            minX=std::min(minX,span.first);maxX=std::max(maxX,span.second-1);minY=std::min(minY,y);maxY=std::max(maxY,y);
            left[y]=std::min(left[y],span.first);right[y]=std::max(right[y],span.second-1);
            for(int x=span.first;x<span.second;++x){top[x]=std::min(top[x],y);bottom[x]=std::max(bottom[x],y);}
        }
        if(maxX<minX)return false;
        const bool horizontal=maxX-minX>=maxY-minY;
        const int start=horizontal?minX:minY,end=horizontal?maxX:maxY;
        std::vector<Point> run;
        for(int i=start;i<=end+1;++i) {
            const bool have=i<=end && (horizontal?bottom[i]>=0:right[i]>=0);
            if(have)run.push_back(horizontal?Point{i+.5,(top[i]+bottom[i]+1)*.5}:Point{(left[i]+right[i]+1)*.5,i+.5});
            else if(!run.empty()){addRun(std::move(run),horizontal);run.clear();}
        }
        return !lines.empty();
    }
};
}
