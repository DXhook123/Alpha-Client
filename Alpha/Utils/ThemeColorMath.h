#pragma once
#include <algorithm>
#include <cmath>

namespace ThemeColorMath {
struct Rgb { int r=0,g=0,b=0; };
inline float phase(double ms,double period) {
    double value=std::fmod(ms,period)/period;
    return float(value<0 ? value+1:value);
}
inline Rgb hsv(float hue,float saturation) {
    float h=hue*6.f;
    int sector=int(h); float f=h-sector;
    float p=1.f-saturation,q=1.f-f*saturation,t=1.f-(1.f-f)*saturation;
    float r=1,g=1,b=1;
    switch (sector%6) {
    case 0:r=1;g=t;b=p;break; case 1:r=q;g=1;b=p;break;
    case 2:r=p;g=1;b=t;break; case 3:r=p;g=q;b=1;break;
    case 4:r=t;g=p;b=1;break; default:r=1;g=p;b=q;break;
    }
    return {int(r*255.f),int(g*255.f),int(b*255.f)};
}
inline Rgb sample(int mode,double now,double offset,float speed,float saturation,Rgb a,Rgb b) {
    if (!std::isfinite(now)) now=0;
    if (!std::isfinite(offset)) offset=0;
    speed=std::isfinite(speed) ? std::clamp(speed,1.f,10.f):4.f;
    saturation=std::isfinite(saturation) ? std::clamp(saturation,0.f,1.f):.5f;
    const double time=now+offset;
    if (mode==1) return hsv(phase(time,24000.0/speed),saturation);
    if (mode==2) {
        float h=phase(time,12000.0/speed);
        if (h>.5f) h=1.f-h;
        return hsv(h+.5f,saturation);
    }
    const float p=phase(time,8000.0/speed);
    const float blend=1.f-std::abs(p*2.f-1.f);
    Rgb result{int(a.r+(b.r-a.r)*blend),int(a.g+(b.g-a.g)*blend),int(a.b+(b.b-a.b)*blend)};
    if (mode==4) {
        Rgb rainbow=hsv(phase(time,40000.0/speed),saturation);
        result={result.r*rainbow.r/255,result.g*rainbow.g/255,result.b*rainbow.b/255};
    }
    return result;
}
}
