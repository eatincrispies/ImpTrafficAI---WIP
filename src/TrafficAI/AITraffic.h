#pragma once
#include "TrafficCars.h"
#include "WRoadNetwork.h"

namespace AITraffic {

    using WRoadNetwork::Vector3;

    struct Profile {
        const char* name;
        float       cruise;
        float       corner;
        float       look;
        float       patience;
    };

    const Profile& Of(const TrafficCars::Car& car);
    void           Assign(TrafficCars::Car& car);

    void  Watch(TrafficCars::Car& car, const Vector3& forward, float speed, unsigned long now);
    bool  Shaken(const TrafficCars::Car& car, unsigned long now);
    void  Shake(TrafficCars::Car& car, unsigned long now);
    bool  PlayerClose(const Vector3& position, float radius);
    void  Note(int what);
    void  Census(char* out, unsigned size);

}
