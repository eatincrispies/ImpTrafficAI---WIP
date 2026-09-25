#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "TrafficCars.h"
#include "AITraffic.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Log.h"
#include "../Hooks/Memory.h"

namespace TrafficCars {

    namespace {

        constexpr int           kTracked       = 48;
        constexpr unsigned long kFreshMs       = 750;
        constexpr unsigned long kReportMs      = 30000;
        constexpr float         kLookTime      = 0.8f;
        constexpr float         kLookBase      = 6.0f;
        constexpr float         kLookMin       = 10.0f;
        constexpr float         kLookMax       = 24.0f;
        constexpr float         kMergeDistance = 0.2f;
        constexpr float         kMinAim        = 1.0f;

        Car gCars[kTracked] = {};

        int           gCounts[static_cast<int>(Event::Count)] = {};
        unsigned long gReportedAt = 0;
        bool          gReportStarted = false;

        float Flat(const Vector3& a, const Vector3& b) {
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
            return Vector3{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
        }

        void Drop(Car& car, int count) {
            if (count <= 0) return;
            if (count >= car.trailCount) {
                car.trailCount = 0;
                return;
            }
            std::memmove(car.trail, car.trail + count, sizeof(Vector3) * static_cast<std::size_t>(car.trailCount - count));
            car.trailCount -= count;
        }

        bool Locate(const Car& car, const Vector3& position, int* segment, float* fraction) {
            if (car.trailCount < 2) return false;

            float best = FLT_MAX;
            for (int i = 0; i + 1 < car.trailCount; ++i) {
                const Vector3& a = car.trail[i];
                const Vector3& b = car.trail[i + 1];
                const float dx = b.x - a.x;
                const float dz = b.z - a.z;
                const float lengthSq = dx * dx + dz * dz;

                float u = 0.0f;
                if (lengthSq > 1e-6f) u = ((position.x - a.x) * dx + (position.z - a.z) * dz) / lengthSq;
                if (u < 0.0f) u = 0.0f;
                if (u > 1.0f) u = 1.0f;

                const float px = a.x + dx * u - position.x;
                const float pz = a.z + dz * u - position.z;
                const float distanceSq = px * px + pz * pz;
                if (distanceSq < best) {
                    best = distanceSq;
                    *segment = i;
                    *fraction = u;
                }
            }
            return true;
        }

    }

    Car* Find(void* vehicleAI) {
        if (!vehicleAI) return nullptr;
        for (Car& car : gCars)
            if (car.vehicleAI == vehicleAI) return &car;
        return nullptr;
    }

    Car* FindByNav(const void* nav) {
        if (!nav) return nullptr;
        for (Car& car : gCars)
            if (car.nav == nav) return &car;
        return nullptr;
    }

    Car* Track(void* vehicleAI) {
        if (!vehicleAI) return nullptr;

        const unsigned long now = GetTickCount();
        if (Car* found = Find(vehicleAI)) {
            found->touched = now;
            return found;
        }

        Car* slot = &gCars[0];
        for (Car& car : gCars) {
            if (!car.vehicleAI) {
                slot = &car;
                break;
            }
            if (now - car.touched > now - slot->touched) slot = &car;
        }

        *slot = Car{};
        slot->vehicleAI = vehicleAI;
        slot->touched = now;
        return slot;
    }

    bool Driving(const Car& car, unsigned long now) {
        return car.trailCount >= 2 && now - car.advanced <= kFreshMs;
    }

    bool ReadPose(void* vehicleAI, Vector3* position, Vector3* forward) {
        void* const aiVehicle = Memory::At<std::uint8_t>(vehicleAI, 0) - Addr::AIVehicleTraffic::VehicleAI;
        void* const pose = *Memory::At<void*>(aiVehicle, Addr::AIVehicleTraffic::Pose);
        if (!pose) return false;

        const auto location = Memory::Invoke<const Vector3*>(pose, Addr::Pose::GetPosition);
        const auto matrix = Memory::Invoke<const std::uint8_t*>(pose, Addr::Pose::GetMatrix);
        if (!location || !matrix) return false;

        const float fx = *Memory::At<const float>(matrix, Addr::Pose::ForwardX);
        const float fz = *Memory::At<const float>(matrix, Addr::Pose::ForwardZ);
        const float length = std::sqrt(fx * fx + fz * fz);
        if (!(length > 1e-3f)) return false;

        *position = *location;
        *forward = Vector3{ fx / length, 0.0f, fz / length };
        return true;
    }

    float Speed(const Car& car) {
        if (!car.rigidBody) return 0.0f;
        return Memory::Invoke<float>(car.rigidBody, Addr::IRigidBody::GetSpeed);
    }

    float LookAhead(float speed) {
        const float distance = kLookBase + speed * kLookTime;
        if (distance < kLookMin) return kLookMin;
        if (distance > kLookMax) return kLookMax;
        return distance;
    }

    bool AimAhead(const Car& car, float distance, Aim* aim) {
        if (!ReadPose(car.vehicleAI, &aim->position, &aim->forward)) return false;
        if (!PointAhead(car, aim->position, distance, &aim->target)) return false;

        const float dx = aim->target.x - aim->position.x;
        const float dz = aim->target.z - aim->position.z;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (!(length > kMinAim)) return false;

        const float ux = dx / length;
        const float uz = dz / length;
        aim->distance = length;
        aim->cosine = aim->forward.x * ux + aim->forward.z * uz;
        aim->sine = aim->forward.x * uz - aim->forward.z * ux;
        return true;
    }

    void ResetTrail(Car& car, const Vector3& first) {
        car.trail[0] = first;
        car.trailCount = 1;
    }

    void AppendTrail(Car& car, const Vector3& point) {
        if (car.trailCount > 0 && Flat(car.trail[car.trailCount - 1], point) < kMergeDistance) {
            car.trail[car.trailCount - 1] = point;
            return;
        }
        if (car.trailCount == kTrailCapacity) Drop(car, 1);
        car.trail[car.trailCount++] = point;
    }

    void PruneTrail(Car& car, const Vector3& position) {
        while (car.trailCount > 2) {
            const Vector3& a = car.trail[0];
            const Vector3& b = car.trail[1];
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            const float lengthSq = dx * dx + dz * dz;
            if (lengthSq > 1e-6f) {
                const float u = ((position.x - a.x) * dx + (position.z - a.z) * dz) / lengthSq;
                if (u < 1.0f) break;
            }
            Drop(car, 1);
        }
    }

    bool PointAhead(const Car& car, const Vector3& position, float distance, Vector3* point) {
        int segment = 0;
        float fraction = 0.0f;
        if (!Locate(car, position, &segment, &fraction)) return false;

        Vector3 current = Lerp(car.trail[segment], car.trail[segment + 1], fraction);
        float remaining = distance;
        for (int i = segment; i + 1 < car.trailCount; ++i) {
            const Vector3& end = car.trail[i + 1];
            const float length = Flat(current, end);
            if (length >= remaining && length > 1e-4f) {
                *point = Lerp(current, end, remaining / length);
                return true;
            }
            remaining -= length;
            current = end;
        }
        *point = car.trail[car.trailCount - 1];
        return true;
    }

    void Record(Event event) {
        ++gCounts[static_cast<int>(event)];
    }

    void Report() {
        const unsigned long now = GetTickCount();
        if (!gReportStarted) {
            gReportStarted = true;
            gReportedAt = now;
            return;
        }
        if (now - gReportedAt < kReportMs) return;

        const unsigned seconds = static_cast<unsigned>((now - gReportedAt) / 1000ul);
        gReportedAt = now;

        int total = 0;
        for (const int count : gCounts) total += count;
        if (total == 0) return;

        char census[256] = "";
        AITraffic::Census(census, sizeof(census));
        Log::Line("Driving styles seen so far: %s.", census);

        const int walls = gCounts[static_cast<int>(Event::WallSpawn)];
        const int ground = gCounts[static_cast<int>(Event::GroundSpawn)];
        const int lanes = gCounts[static_cast<int>(Event::LaneSpawn)];
        Log::Line("Last %u s: %d spawn point(s) rejected (%d near a wall, %d off the road surface, %d outside a traffic lane), "
                  "%d road re-snap(s) undone, %d stopped car(s) creeping up, %d knocked car(s) kept driving, "
                  "%d stuck car(s) removed.",
                  seconds, walls + ground + lanes, walls, ground, lanes,
                  gCounts[static_cast<int>(Event::Resnap)],
                  gCounts[static_cast<int>(Event::Creep)],
                  gCounts[static_cast<int>(Event::Revive)],
                  gCounts[static_cast<int>(Event::Despawn)]);

        for (int& count : gCounts) count = 0;
    }

}
