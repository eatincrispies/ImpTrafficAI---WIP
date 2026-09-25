#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include "AIVehicleTraffic.h"
#include "AITraffic.h"
#include "TrafficCars.h"
#include "WRoadNetwork.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Hook.h"
#include "../Hooks/Log.h"
#include "../Hooks/Memory.h"

namespace AIVehicleTraffic {

    namespace {

        namespace Site = Addr::AIVehicleTraffic;
        using WRoadNetwork::Vector3;

        using ResetDriveToNavMethod = void(__fastcall*)(void*, void*, int);
        using SteeringGainMethod    = float(__fastcall*)(void*, void*, float);
        using OnGasBrakeMethod      = void(__fastcall*)(void*, void*, float);

        constexpr float kMwGain       = 1.0f;
        constexpr float kSnapVertical = 2.5f;
        constexpr float kSnapHeading  = 0.3f;
        constexpr float kBackupReach  = 60.0f;

        constexpr unsigned long kShockGraceMs = 250;

        constexpr float kStopRequest = 0.5f;
        constexpr float kFullGasError = 1.5f;
        constexpr float kGasBase      = 0.5f;
        constexpr float kGasGain      = 0.33f;
        constexpr float kCoastBand   = 0.7f;
        constexpr float kBrakeGain   = 0.35f;
        constexpr float kBrakeMin    = 0.1f;

        constexpr float kTiltCosine    = 0.966f;
        constexpr float kTiltRateSoft  = 1.0f;
        constexpr float kTiltRateMax   = 2.0f;
        constexpr float kTiltDamping   = 8.0f;
        constexpr float kRighting      = 12.0f;
        constexpr float kYawRateMax    = 3.0f;
        constexpr float kLaunchSpeed   = 3.0f;

        void* gResetDriveToNav = nullptr;
        void* gSteeringGain = nullptr;
        void* gOnGasBrake = nullptr;
        bool  gDiagnosed = false;

        float Flat(const Vector3& a, const Vector3& b) {
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        void Diagnose(const TrafficCars::Car& car, float carbonGain) {
            void* const vehicle = Memory::Invoke<void*>(car.vehicleAI, Addr::IVehicleAI::GetVehicle);
            const int driverClass = vehicle ? Memory::Invoke<int>(vehicle, Addr::IVehicle::GetDriverClass) : -1;
            const float mass = car.rigidBody ? Memory::Invoke<float>(car.rigidBody, Addr::IRigidBody::GetMass) : 0.0f;
            Log::Line("First traffic car: driver class %d, mass %.0f, speed %.1f m/s, driving style %s; "
                      "Carbon wanted a steering gain of %.2f, MW steers at %.2f.",
                      driverClass, mass, TrafficCars::Speed(car), AITraffic::Of(car).name, carbonGain, kMwGain);
        }

        bool GuardedTraffic(void* aiVehicle, float carbonGain) {
            __try {
                void* const vehicleAI = Memory::At<std::uint8_t>(aiVehicle, 0) + Site::VehicleAI;
                const TrafficCars::Car* car = TrafficCars::Find(vehicleAI);
                if (!car) return false;
                if (!gDiagnosed) {
                    gDiagnosed = true;
                    Diagnose(*car, carbonGain);
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        float __fastcall SteeringGain(void* aiVehicle, void* edx, float steer) {
            const float carbonGain = reinterpret_cast<SteeringGainMethod>(gSteeringGain)(aiVehicle, edx, steer);
            return GuardedTraffic(aiVehicle, carbonGain) ? kMwGain : carbonGain;
        }

        TrafficCars::Car* CarOf(void* aiVehicle) {
            void* const vehicleAI = Memory::At<std::uint8_t>(aiVehicle, 0) + Site::VehicleAI;
            return TrafficCars::Find(vehicleAI);
        }

        void Stabilize(void* aiVehicle, float dT) {
            const TrafficCars::Car* car = CarOf(aiVehicle);
            if (!car || !car->rigidBody || !(dT > 0.0f)) return;
            void* const body = car->rigidBody;

            const auto spin = Memory::Invoke<const Vector3*>(body, Addr::IRigidBody::GetAngularVelocity);
            if (!spin) return;
            Vector3 omega = *spin;

            Vector3 up = {};
            Memory::Invoke<void>(body, Addr::IRigidBody::GetUpVector, &up);

            bool changed = false;
            const float tiltRate = std::sqrt(omega.x * omega.x + omega.z * omega.z);
            if (up.y < kTiltCosine || tiltRate > kTiltRateSoft) {
                const float damp = std::exp(-kTiltDamping * dT);
                omega.x *= damp;
                omega.z *= damp;
                if (up.y < kTiltCosine) {
                    omega.x += -up.z * kRighting * dT;
                    omega.z += up.x * kRighting * dT;
                }
                changed = true;
            }

            const float tilt = std::sqrt(omega.x * omega.x + omega.z * omega.z);
            if (tilt > kTiltRateMax) {
                omega.x *= kTiltRateMax / tilt;
                omega.z *= kTiltRateMax / tilt;
                changed = true;
            }
            if (omega.y > kYawRateMax || omega.y < -kYawRateMax) {
                omega.y = omega.y > 0.0f ? kYawRateMax : -kYawRateMax;
                changed = true;
            }
            if (changed) Memory::Invoke<void>(body, Addr::IRigidBody::SetAngularVelocity, &omega);

            if (!AITraffic::Shaken(*car, GetTickCount())) return;

            const auto moving = Memory::Invoke<const Vector3*>(body, Addr::IRigidBody::GetLinearVelocity);
            if (moving && moving->y > kLaunchSpeed) {
                Vector3 velocity = *moving;
                velocity.y = kLaunchSpeed;
                Memory::Invoke<void>(body, Addr::IRigidBody::SetLinearVelocity, &velocity);
            }
        }

        void GuardedStabilize(void* aiVehicle, float dT) {
            __try {
                Stabilize(aiVehicle, dT);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        void Pedals(void* aiVehicle) {
            TrafficCars::Car* car = CarOf(aiVehicle);
            if (!car) return;
            if ((*Memory::At<const std::uint8_t>(aiVehicle, Site::DriveFlags) & 2u) == 0) return;

            void* const vehicleAI = Memory::At<std::uint8_t>(aiVehicle, 0) + Site::VehicleAI;
            void* const vehicle = Memory::Invoke<void*>(vehicleAI, Addr::IVehicleAI::GetVehicle);
            void* const transmission = *Memory::At<void*>(aiVehicle, Site::Transmission);
            void* const input = Memory::At<std::uint8_t>(aiVehicle, 0) + Site::Input;
            if (!vehicle || !transmission) return;

            if (Memory::Invoke<bool>(transmission, Addr::ITransmission::IsReversing)) return;
            const bool reversingSpeed = *Memory::At<const std::uint8_t>(aiVehicle, Site::ReversingSpeed) != 0;
            const bool steeringBehind = *Memory::At<const std::uint8_t>(aiVehicle, Site::SteeringBehind) != 0;
            if (!reversingSpeed && steeringBehind) return;

            const unsigned long now = GetTickCount();
            if (Memory::Invoke<bool>(vehicle, Addr::IVehicle::InShock)) {
                if (car->shockAt == 0) car->shockAt = now;
                if (now - car->shockAt < kShockGraceMs) return;
                if (Memory::Invoke<int>(transmission, Addr::ITransmission::GetGear) == Addr::ITransmission::GearNeutral)
                    Memory::Invoke<void>(transmission, Addr::ITransmission::Shift, Addr::ITransmission::GearFirst);
            } else {
                car->shockAt = 0;
            }

            const float desired = *Memory::At<const float>(aiVehicle, Site::DriveSpeed);
            const float current = Memory::Invoke<float>(vehicle, Addr::IVehicle::GetSpeed);

            float gas = 0.0f;
            float brake = 0.0f;
            if (desired < kStopRequest) {
                brake = 1.0f;
            } else if (current < -1.0f) {
                brake = 1.0f;
            } else {
                const float error = desired - current;
                if (error >= kFullGasError) {
                    gas = 1.0f;
                } else if (error >= 0.0f) {
                    gas = kGasBase + error * kGasGain;
                } else if (-error > kCoastBand) {
                    brake = (-error - kCoastBand) * kBrakeGain;
                    if (brake < kBrakeMin) brake = kBrakeMin;
                    if (brake > 1.0f) brake = 1.0f;
                }
            }

            Memory::Invoke<void>(input, Addr::IInput::SetControlGas, gas);
            Memory::Invoke<void>(input, Addr::IInput::SetControlBrake, brake);
        }

        void GuardedPedals(void* aiVehicle) {
            __try {
                Pedals(aiVehicle);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        void __fastcall OnGasBrake(void* aiVehicle, void* edx, float dT) {
            GuardedStabilize(aiVehicle, dT);
            reinterpret_cast<OnGasBrakeMethod>(gOnGasBrake)(aiVehicle, edx, dT);
            GuardedPedals(aiVehicle);
        }

        bool GuardedBackup(void* vehicleAI, void** nav, void* storage, bool* constructed) {
            __try {
                *nav = Memory::Invoke<void*>(vehicleAI, Addr::IVehicleAI::GetDriveToNav);
                if (!*nav || !WRoadNetwork::NavValid(*nav)) return false;
                WRoadNetwork::ConstructNav(storage);
                *constructed = true;
                WRoadNetwork::CopyNav(storage, *nav);
                return WRoadNetwork::NavValid(storage);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool Wrong(void* vehicleAI, const void* nav, const void* backup) {
            Vector3 position = {};
            Vector3 forward = {};
            if (!TrafficCars::ReadPose(vehicleAI, &position, &forward)) return false;
            if (Flat(WRoadNetwork::NavPosition(backup), position) > kBackupReach) return false;

            if (!WRoadNetwork::NavValid(nav)) return true;

            if (!WRoadNetwork::Connected(WRoadNetwork::NavSegment(backup), WRoadNetwork::NavSegment(nav))) return true;

            const Vector3 fresh = WRoadNetwork::NavPosition(nav);
            if (std::fabs(fresh.y - position.y) > kSnapVertical) return true;

            const Vector3 heading = WRoadNetwork::NavForward(nav);
            const float length = std::sqrt(heading.x * heading.x + heading.z * heading.z);
            if (length > 1e-3f && (heading.x * forward.x + heading.z * forward.z) / length < kSnapHeading) return true;
            return false;
        }

        bool GuardedRevert(void* vehicleAI, void* nav, const void* backup) {
            __try {
                if (!Wrong(vehicleAI, nav, backup)) return false;
                WRoadNetwork::CopyNav(nav, backup);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        void GuardedRelease(void* storage) {
            __try {
                WRoadNetwork::DestructNav(storage);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        void __fastcall ResetDriveToNav(void* vehicleAI, void* edx, int mode) {
            const auto original = reinterpret_cast<ResetDriveToNavMethod>(gResetDriveToNav);

            alignas(16) std::uint8_t backup[Addr::WRoadNav::Size];
            void* nav = nullptr;
            bool constructed = false;
            const bool saved = GuardedBackup(vehicleAI, &nav, backup, &constructed);

            original(vehicleAI, edx, mode);

            if (saved && GuardedRevert(vehicleAI, nav, backup)) TrafficCars::Record(TrafficCars::Event::Resnap);
            if (constructed) GuardedRelease(backup);
        }

    }

    void Install() {
        Hook::Detour("AIVehicle OnSteering gain (MW has none)", Site::SteeringGain,
                     reinterpret_cast<void*>(&SteeringGain), &gSteeringGain);
        Hook::SwapSlot("AIVehicleTraffic ResetDriveToNav", Site::ResetDriveToNav,
                       reinterpret_cast<void*>(&ResetDriveToNav), &gResetDriveToNav);
        Hook::SwapSlot("AIVehicleTraffic OnGasBrake (smooth pedals, shock recovery, anti-flip)", Site::OnGasBrake,
                       reinterpret_cast<void*>(&OnGasBrake), &gOnGasBrake);
    }

}
