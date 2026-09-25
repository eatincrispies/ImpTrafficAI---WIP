#pragma once
#include "WRoadNetwork.h"

namespace TrafficCars {

    using WRoadNetwork::Vector3;

    inline constexpr int kTrailCapacity = 48;

    struct Car {
        void*         vehicleAI;
        void*         rigidBody;
        void*         action;
        void*         nav;
        unsigned long touched;
        unsigned long advanced;
        Vector3       trail[kTrailCapacity];
        int           trailCount;
        bool          anchored;
        Vector3       anchor;
        unsigned long anchoredAt;
        int           driver;
        bool          assigned;
        float         phase;
        Vector3       heading;
        float         speed;
        unsigned long hitAt;
        bool          primed;
        unsigned long stalledSince;
        unsigned long reportedAt;
        unsigned long shockAt;
    };

    struct Aim {
        Vector3 position;
        Vector3 forward;
        Vector3 target;
        float   distance;
        float   sine;
        float   cosine;
    };

    enum class Event {
        WallSpawn,
        GroundSpawn,
        LaneSpawn,
        Resnap,
        Creep,
        Revive,
        Despawn,
        Count
    };

    Car* Find(void* vehicleAI);
    Car* FindByNav(const void* nav);
    Car* Track(void* vehicleAI);
    bool Driving(const Car& car, unsigned long now);

    bool  ReadPose(void* vehicleAI, Vector3* position, Vector3* forward);
    float Speed(const Car& car);
    float LookAhead(float speed);
    bool  AimAhead(const Car& car, float distance, Aim* aim);

    void  ResetTrail(Car& car, const Vector3& first);
    void  AppendTrail(Car& car, const Vector3& point);
    void  PruneTrail(Car& car, const Vector3& position);
    bool  PointAhead(const Car& car, const Vector3& position, float distance, Vector3* point);

    void Record(Event event);
    void Report();

}
