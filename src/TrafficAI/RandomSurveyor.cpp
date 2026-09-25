#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include "RandomSurveyor.h"
#include "CollisionWorld.h"
#include "TrafficCars.h"
#include "WRoadNetwork.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Hook.h"
#include "../Hooks/Memory.h"

namespace RandomSurveyor {

    namespace {

        namespace Site = Addr::RandomSurveyor;
        using WRoadNetwork::Vector3;

        using ValidateMethod = bool(__fastcall*)(void*, void*);

        constexpr float kGroundTolerance = 2.0f;
        constexpr float kSideClearance   = 1.3f;
        constexpr float kLengthClearance = 3.0f;
        constexpr float kLowRay          = 0.5f;
        constexpr float kHighRay         = 1.0f;

        enum class Verdict {
            Accept,
            Lane,
            Ground,
            Wall
        };

        void* gValidate = nullptr;

        bool Blocked(const Vector3& center, float dx, float dz) {
            const Vector3 end = { center.x + dx, center.y, center.z + dz };
            return CollisionWorld::HitsWorld(center, end);
        }

        Verdict Inspect(void* surveyor) {
            const void* nav = Memory::At<std::uint8_t>(surveyor, Site::Nav);
            if (!WRoadNetwork::IsTrafficLane(nav)) return Verdict::Lane;

            const Vector3 position = WRoadNetwork::NavPosition(nav);
            float ground = position.y;
            float height = 0.0f;
            if (CollisionWorld::GroundHeight(position, &height)) {
                if (std::fabs(height - position.y) > kGroundTolerance) return Verdict::Ground;
                ground = height;
            }

            const Vector3 heading = WRoadNetwork::NavForward(nav);
            const float length = std::sqrt(heading.x * heading.x + heading.z * heading.z);
            if (!(length > 1e-3f)) return Verdict::Accept;
            const float fx = heading.x / length;
            const float fz = heading.z / length;

            const float lifts[2] = { kLowRay, kHighRay };
            for (const float lift : lifts) {
                const Vector3 center = { position.x, ground + lift, position.z };
                if (Blocked(center, fz * kSideClearance, -fx * kSideClearance)
                 || Blocked(center, -fz * kSideClearance, fx * kSideClearance)
                 || Blocked(center, fx * kLengthClearance, fz * kLengthClearance)
                 || Blocked(center, -fx * kLengthClearance, -fz * kLengthClearance))
                    return Verdict::Wall;
            }
            return Verdict::Accept;
        }

        bool GuardedInspect(void* surveyor, Verdict* verdict) {
            __try {
                *verdict = Inspect(surveyor);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool __fastcall Validate(void* surveyor, void* edx) {
            if (!reinterpret_cast<ValidateMethod>(gValidate)(surveyor, edx)) return false;

            Verdict verdict = Verdict::Accept;
            if (!GuardedInspect(surveyor, &verdict)) return true;

            switch (verdict) {
            case Verdict::Lane:
                TrafficCars::Record(TrafficCars::Event::LaneSpawn);
                return false;
            case Verdict::Ground:
                TrafficCars::Record(TrafficCars::Event::GroundSpawn);
                return false;
            case Verdict::Wall:
                TrafficCars::Record(TrafficCars::Event::WallSpawn);
                return false;
            default:
                return true;
            }
        }

    }

    void Install() {
        Hook::RedirectCall("RandomSurveyor spawn check", Site::ValidateCall, reinterpret_cast<void*>(&Validate), &gValidate);
    }

}
