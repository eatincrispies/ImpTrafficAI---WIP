#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "AITrafficManager.h"
#include "TrafficCars.h"
#include "WRoadNetwork.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Hook.h"
#include "../Hooks/Memory.h"

namespace AITrafficManager {

    namespace {

        namespace Site = Addr::AITrafficManager;
        using WRoadNetwork::Vector3;

        using KeepMethod = bool(__fastcall*)(void*, void*, void*, float);

        constexpr float         kStuckRadius    = 1.5f;
        constexpr float         kStuckDrive     = 2.0f;
        constexpr unsigned long kStuckMs        = 5000;
        constexpr unsigned long kStallMs        = 30000;
        constexpr float         kStuckOffscreen = 2.0f;

        void* gKeep = nullptr;

        bool Stuck(void* vehicle) {
            void* const vehicleAI = Memory::Invoke<void*>(vehicle, Addr::IVehicle::GetAIVehiclePtr);
            TrafficCars::Car* car = TrafficCars::Find(vehicleAI);
            if (!car) return false;

            const unsigned long now = GetTickCount();
            if (!car->rigidBody || !TrafficCars::Driving(*car, now)) {
                car->anchored = false;
                return false;
            }

            const auto location = Memory::Invoke<const Vector3*>(car->rigidBody, Addr::IRigidBody::GetPosition);
            if (!location) return false;
            const Vector3 position = *location;

            const float dx = position.x - car->anchor.x;
            const float dy = position.y - car->anchor.y;
            const float dz = position.z - car->anchor.z;
            if (!car->anchored || std::sqrt(dx * dx + dy * dy + dz * dz) > kStuckRadius) {
                car->anchor = position;
                car->anchoredAt = now;
                car->anchored = true;
                return false;
            }

            const unsigned long still = now - car->anchoredAt;
            const float drive = Memory::Invoke<float>(vehicleAI, Addr::IVehicleAI::GetDriveSpeed);
            const bool trapped = drive > kStuckDrive ? still >= kStuckMs : still >= kStallMs;
            if (!trapped) return false;

            const float offscreen = Memory::Invoke<float>(vehicle, Addr::IVehicle::GetOffscreenTime);
            if (offscreen < kStuckOffscreen) return false;

            car->anchored = false;
            return true;
        }

        bool GuardedStuck(void* vehicle) {
            __try {
                return vehicle && Stuck(vehicle);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        void GuardedReport() {
            __try {
                TrafficCars::Report();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        bool __fastcall Keep(void* manager, void* edx, void* vehicle, float density) {
            const bool keep = reinterpret_cast<KeepMethod>(gKeep)(manager, edx, vehicle, density);
            GuardedReport();
            if (!keep) return false;
            if (!GuardedStuck(vehicle)) return true;

            TrafficCars::Record(TrafficCars::Event::Despawn);
            return false;
        }

    }

    void Install() {
        Hook::RedirectCall("AITrafficManager keep check", Site::KeepCall, reinterpret_cast<void*>(&Keep), &gKeep);
    }

}
