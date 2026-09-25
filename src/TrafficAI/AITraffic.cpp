#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "AITraffic.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Memory.h"

namespace AITraffic {

    namespace {

        constexpr int           kProfiles    = 5;
        constexpr int           kNotes       = 6;
        constexpr unsigned long kShakenMs    = 3000;
        constexpr float         kHeadingJump = 0.9945f;
        constexpr float         kSpeedDrop   = 1.5f;
        constexpr float         kTwoPi       = 6.2831853f;

        const Profile gProfiles[kProfiles] = {
            { "civil",  1.00f, 1.00f, 1.00f, 5.0f },
            { "slow",   0.82f, 0.90f, 1.05f, 7.0f },
            { "manic",  1.15f, 1.10f, 0.97f, 3.0f },
            { "racer",  1.25f, 1.20f, 0.95f, 2.5f },
            { "clumsy", 0.92f, 1.00f, 1.00f, 6.0f }
        };

        const int gShare[10] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 4 };

        int gCounts[kProfiles] = {};
        int gNotes[kNotes] = {};
        std::uint32_t gSeed = 0x9E3779B9u;

        std::uint32_t Mix(std::uint32_t value) {
            value ^= value >> 16;
            value *= 0x7FEB352Du;
            value ^= value >> 15;
            value *= 0x846CA68Bu;
            value ^= value >> 16;
            return value;
        }

        bool PlayerAt(Vector3* where) {
            if (*Memory::Global<const std::int32_t>(Addr::IPlayer::Count) <= 0) return false;

            void** const list = *Memory::Global<void**>(Addr::IPlayer::List);
            if (!list) return false;

            void* const player = list[0];
            if (!player) return false;

            void* const simable = Memory::Invoke<void*>(player, Addr::IPlayer::GetSimable);
            if (!simable) return false;

            void* const body = Memory::Invoke<void*>(simable, Addr::ISimable::GetRigidBody);
            if (!body) return false;

            const auto position = Memory::Invoke<const Vector3*>(body, Addr::IRigidBody::GetPosition);
            if (!position) return false;

            *where = *position;
            return true;
        }

    }

    const Profile& Of(const TrafficCars::Car& car) {
        const int index = car.driver >= 0 && car.driver < kProfiles ? car.driver : 0;
        return gProfiles[index];
    }

    void Assign(TrafficCars::Car& car) {
        gSeed = Mix(gSeed + 0x85EBCA6Bu);
        const std::uint32_t roll = Mix(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(car.vehicleAI)) ^ gSeed);
        car.driver = gShare[roll % 10u];
        car.phase = static_cast<float>(roll >> 8 & 0xFFFFu) / 65535.0f * kTwoPi;
        ++gCounts[car.driver];
    }

    void Watch(TrafficCars::Car& car, const Vector3& forward, float speed, unsigned long now) {
        if (car.primed) {
            const float aligned = car.heading.x * forward.x + car.heading.z * forward.z;
            if (aligned < kHeadingJump || car.speed - speed > kSpeedDrop) {
                if (!Shaken(car, now)) Note(1);
                car.hitAt = now;
            }
        }
        car.heading = forward;
        car.speed = speed;
        car.primed = true;
    }

    bool Shaken(const TrafficCars::Car& car, unsigned long now) {
        return car.hitAt != 0 && now - car.hitAt < kShakenMs;
    }

    void Shake(TrafficCars::Car& car, unsigned long now) {
        car.hitAt = now;
    }

    void Note(int what) {
        if (what >= 0 && what < kNotes) ++gNotes[what];
    }

    bool PlayerClose(const Vector3& position, float radius) {
        Vector3 player = {};
        if (!PlayerAt(&player)) return false;

        const float dx = player.x - position.x;
        const float dy = player.y - position.y;
        const float dz = player.z - position.z;
        return dx * dx + dy * dy + dz * dz < radius * radius;
    }

    void Census(char* out, unsigned size) {
        std::snprintf(out, size,
                      "%d civil, %d slow, %d manic, %d racer, %d clumsy; %d MW following tick(s), "
                      "%d oncoming car(s) ignored, %d knock(s), %d gate fail(s), %d revive(s), %d stall break(s)",
                      gCounts[0], gCounts[1], gCounts[2], gCounts[3], gCounts[4],
                      gNotes[0], gNotes[5], gNotes[1], gNotes[2], gNotes[3], gNotes[4]);
        for (int& note : gNotes) note = 0;
    }

}
