#pragma once
#include <cstddef>
#include <cstdint>

namespace WRoadNetwork {

    struct Vector3 {
        float x;
        float y;
        float z;
    };

    bool    NavValid(const void* nav);
    bool    NavOccluded(const void* nav);
    Vector3 NavLeadVelocity(const void* nav);
    int     TrafficLanes(const void* nav);
    Vector3 NavPosition(const void* nav);
    Vector3 NavForward(const void* nav);
    bool    IsTrafficLane(const void* nav);
    int     NavSegment(const void* nav);
    int     NavNode(const void* nav);
    bool    SameRoad(int first, int second);
    bool    Connected(int first, int second);

    void ConstructNav(void* storage);
    void DestructNav(void* storage);
    void CopyNav(void* destination, const void* source);

}
