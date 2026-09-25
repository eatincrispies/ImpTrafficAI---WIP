#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include "AIActionTraffic.h"
#include "AITraffic.h"
#include "TrafficCars.h"
#include "WRoadNetwork.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Hook.h"
#include "../Hooks/Memory.h"

namespace AIActionTraffic {

    namespace {

        namespace Site = Addr::AIActionTraffic;
        using WRoadNetwork::Vector3;

        using AdvanceMethod      = void(__fastcall*)(void*, void*, float);
        using DriveSpeedMethod   = float(__fastcall*)(void*, void*, float);
        using StateMachineMethod = int(__fastcall*)(void*, void*, float);
        using CanRunMethod       = bool(__fastcall*)(void*, void*, int);

        constexpr float kTrailStep    = 2.5f;
        constexpr float kStepSlack    = 0.05f;
        constexpr float kMinProgress  = 0.01f;
        constexpr int   kMaxSteps     = 24;
        constexpr float kSeamDistance = 1.0f;
        constexpr float kReattach     = 6.0f;
        constexpr float kNearCar      = 3.0f;

        constexpr float kCarbonLook   = 35.0f;
        constexpr float kLookNear     = 10.0f;
        constexpr float kLookFar      = 30.0f;
        constexpr float kLookRamp     = 25.0f;

        constexpr float kGravity         = 9.8f;
        constexpr float kTrafficFriction = 0.6f;
        constexpr float kCopFriction     = 1.6f;
        constexpr int   kHighwayLanes    = 4;
        constexpr float kStopNear        = 3.0f;
        constexpr float kStopFar         = 50.0f;
        constexpr float kStopRange       = 80.0f;
        constexpr float kMassScale       = 0.0005f;
        constexpr float kOncoming        = -1.0f;
        constexpr float kEmergencyGap    = 4.0f;

        constexpr float kReviveLook  = 12.0f;
        constexpr float kStallSpeed  = 0.6f;
        constexpr float kStallGap    = 2.5f;
        constexpr float kPlayerStall = 15.0f;
        constexpr float kContact     = 4.0f;
        constexpr float kBreakSpeed  = 6.0f;

        constexpr unsigned long kReviveWindowMs = 2500;
        constexpr unsigned long kStallMs        = 400;
        constexpr unsigned long kKnockedStallMs = 150;

        void* gAdvance = nullptr;
        void* gDriveSpeed = nullptr;
        void* gStateMachine = nullptr;
        void* gCanRun = nullptr;

        struct Frame {
            void*   vehicleAI;
            void*   rigidBody;
            void*   nav;
            Vector3 position;
        };

        float Flat(const Vector3& a, const Vector3& b) {
            const float dx = b.x - a.x;
            const float dz = b.z - a.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        float Clamp(float value, float low, float high) {
            if (value < low) return low;
            if (value > high) return high;
            return value;
        }

        bool ReadFrame(void* action, Frame* frame) {
            frame->vehicleAI = *Memory::At<void*>(action, Site::VehicleAI);
            frame->rigidBody = *Memory::At<void*>(action, Site::RigidBody);
            if (!frame->vehicleAI || !frame->rigidBody) return false;

            frame->nav = Memory::Invoke<void*>(frame->vehicleAI, Addr::IVehicleAI::GetDriveToNav);
            if (!frame->nav || !WRoadNetwork::NavValid(frame->nav)) return false;

            const auto position = Memory::Invoke<const Vector3*>(frame->rigidBody, Addr::IRigidBody::GetPosition);
            if (!position) return false;
            frame->position = *position;
            return true;
        }

        float Lead(const Frame& frame) {
            const Vector3 nav = WRoadNetwork::NavPosition(frame.nav);
            const Vector3 forward = WRoadNetwork::NavForward(frame.nav);
            const float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
            if (!(length > 1e-6f)) return 0.0f;
            return ((nav.x - frame.position.x) * forward.x
                  + (nav.y - frame.position.y) * forward.y
                  + (nav.z - frame.position.z) * forward.z) / length;
        }

        float LookAhead(const Frame& frame, float carbonLook) {
            const float speed = Memory::Invoke<float>(frame.rigidBody, Addr::IRigidBody::GetSpeed);
            const float radius = Memory::Invoke<float>(frame.rigidBody, Addr::IRigidBody::GetRadius);
            const float travel = carbonLook > kCarbonLook ? carbonLook - kCarbonLook : 0.0f;

            float t = Clamp(speed / kLookRamp, 0.0f, 1.0f);
            if (WRoadNetwork::NavOccluded(frame.nav)) t = 1.0f;
            return kLookNear + (kLookFar - kLookNear) * t + travel + Clamp(radius, 0.0f, 6.0f);
        }

        bool Begin(void* action, float carbonLook, Frame* frame, TrafficCars::Car** car, float* lead, float* lookAhead) {
            if (!ReadFrame(action, frame)) return false;

            TrafficCars::Car* tracked = TrafficCars::Track(frame->vehicleAI);
            if (!tracked) return false;

            const Vector3 nav = WRoadNetwork::NavPosition(frame->nav);
            const float drift = tracked->trailCount == 0 ? FLT_MAX : Flat(tracked->trail[tracked->trailCount - 1], nav);
            if (drift > kSeamDistance && drift <= kReattach) {
                TrafficCars::AppendTrail(*tracked, nav);
            } else if (drift > kReattach) {
                if (Flat(nav, frame->position) <= kNearCar) {
                    TrafficCars::ResetTrail(*tracked, nav);
                } else {
                    TrafficCars::ResetTrail(*tracked, frame->position);
                    TrafficCars::AppendTrail(*tracked, nav);
                }
            }

            tracked->rigidBody = frame->rigidBody;
            tracked->action = action;
            tracked->nav = frame->nav;
            if (!tracked->assigned) {
                tracked->assigned = true;
                AITraffic::Assign(*tracked);
            }

            Vector3 pose = {};
            Vector3 forward = {};
            if (TrafficCars::ReadPose(frame->vehicleAI, &pose, &forward))
                AITraffic::Watch(*tracked, forward, TrafficCars::Speed(*tracked), GetTickCount());

            *car = tracked;
            *lead = Lead(*frame);
            *lookAhead = LookAhead(*frame, carbonLook);
            return true;
        }

        bool GuardedBegin(void* action, float carbonLook, Frame* frame, TrafficCars::Car** car, float* lead, float* lookAhead) {
            __try {
                return Begin(action, carbonLook, frame, car, lead, lookAhead);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool GuardedStep(const Frame* frame, TrafficCars::Car* car, float* lead) {
            __try {
                TrafficCars::AppendTrail(*car, WRoadNetwork::NavPosition(frame->nav));
                *lead = Lead(*frame);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        void GuardedFinish(const Frame* frame, TrafficCars::Car* car) {
            __try {
                TrafficCars::AppendTrail(*car, WRoadNetwork::NavPosition(frame->nav));
                TrafficCars::PruneTrail(*car, frame->position);
                car->advanced = GetTickCount();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                car->trailCount = 0;
            }
        }

        void __fastcall Advance(void* action, void* edx, float carbonLook) {
            const auto original = reinterpret_cast<AdvanceMethod>(gAdvance);

            Frame frame = {};
            TrafficCars::Car* car = nullptr;
            float lead = 0.0f;
            float lookAhead = carbonLook;
            if (!GuardedBegin(action, carbonLook, &frame, &car, &lead, &lookAhead)) {
                original(action, edx, carbonLook);
                return;
            }

            bool reached = false;
            for (int step = 0; step < kMaxSteps && lead < lookAhead - kStepSlack; ++step) {
                const bool last = !(lead + kTrailStep < lookAhead);
                original(action, edx, last ? lookAhead : lead + kTrailStep);
                reached = last;

                float next = lead;
                if (!GuardedStep(&frame, car, &next) || !(next > lead + kMinProgress)) break;
                lead = next;
            }

            if (!reached) original(action, edx, lookAhead);
            GuardedFinish(&frame, car);
        }

        bool Revive(void* action) {
            void* const vehicleAI = *Memory::At<void*>(action, Site::VehicleAI);
            TrafficCars::Car* car = TrafficCars::Find(vehicleAI);
            if (!car || car->trailCount < 2) return false;
            if (GetTickCount() - car->advanced > kReviveWindowMs) return false;

            void* const nav = Memory::Invoke<void*>(vehicleAI, Addr::IVehicleAI::GetDriveToNav);
            if (!nav) return false;

            Vector3 position = {};
            Vector3 forward = {};
            if (!TrafficCars::ReadPose(vehicleAI, &position, &forward)) return false;

            Vector3 ahead = {};
            if (!TrafficCars::PointAhead(*car, position, kReviveLook, &ahead)) return false;

            const float dx = ahead.x - position.x;
            const float dz = ahead.z - position.z;
            const float length = std::sqrt(dx * dx + dz * dz);
            if (!(length > 1.0f)) return false;

            const Vector3 heading = { dx / length, 0.0f, dz / length };
            Memory::Call<void>(Addr::WRoadNav::InitAtPoint, nav, &position, &heading, 0, 1.0f);
            return WRoadNetwork::NavValid(nav);
        }

        bool GuardedRevive(void* action) {
            __try {
                return Revive(action);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        bool __fastcall CanRun(void* action, void* edx, int mode) {
            if (reinterpret_cast<CanRunMethod>(gCanRun)(action, edx, mode)) return true;

            AITraffic::Note(2);
            if (!GuardedRevive(action)) return false;

            AITraffic::Note(3);
            TrafficCars::Record(TrafficCars::Event::Revive);
            return true;
        }

        bool GuardedNavValid(void* action, bool* valid) {
            __try {
                void* const vehicleAI = *Memory::At<void*>(action, Site::VehicleAI);
                void* const nav = vehicleAI ? Memory::Invoke<void*>(vehicleAI, Addr::IVehicleAI::GetDriveToNav) : nullptr;
                *valid = nav && WRoadNetwork::NavValid(nav);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        int __fastcall StateMachine(void* action, void* edx, float dT) {
            bool valid = false;
            if (!GuardedNavValid(action, &valid))
                return reinterpret_cast<StateMachineMethod>(gStateMachine)(action, edx, dT);
            return valid ? Site::StateCruise : Site::StateStopped;
        }

        float SpeedLimitForCurvature(float friction, float curvature, float topSpeed) {
            const float sideForce = friction * kGravity;
            const float floor = topSpeed > 0.1f ? sideForce / (topSpeed * topSpeed) : sideForce;
            const float bend = std::fabs(curvature) > floor ? std::fabs(curvature) : floor;
            return std::sqrt(sideForce / bend);
        }

        float StoppingDistance(float scaledSpeed) {
            const float t = Clamp(scaledSpeed / kStopRange, 0.0f, 1.0f);
            return kStopNear + (kStopFar - kStopNear) * t;
        }

        bool Stalled(TrafficCars::Car& car, void* vehicleAI, float current) {
            const unsigned long now = GetTickCount();
            if (current > kStallSpeed) {
                car.stalledSince = 0;
                return false;
            }
            if (car.stalledSince == 0) {
                car.stalledSince = now;
                return false;
            }

            const unsigned long patience = AITraffic::Shaken(car, now) ? kKnockedStallMs : kStallMs;
            if (now - car.stalledSince < patience) return false;

            Vector3 position = {};
            Vector3 forward = {};
            if (!TrafficCars::ReadPose(vehicleAI, &position, &forward)) return false;
            if (!AITraffic::PlayerClose(position, kPlayerStall)) return false;

            if (AITraffic::PlayerClose(position, kContact)) AITraffic::Shake(car, now);
            car.stalledSince = now;
            return true;
        }

        float ComputeSpeed(void* action, float dT) {
            void* const vehicleAI = *Memory::At<void*>(action, Site::VehicleAI);
            void* const rigidBody = *Memory::At<void*>(action, Site::RigidBody);
            if (!vehicleAI || !rigidBody) return 0.0f;

            void* const nav = Memory::Invoke<void*>(vehicleAI, Addr::IVehicleAI::GetDriveToNav);
            if (!nav || !WRoadNetwork::NavValid(nav)) return 0.0f;

            TrafficCars::Car* car = TrafficCars::Find(vehicleAI);
            const float cruise = car ? AITraffic::Of(*car).cruise : 1.0f;
            const float corner = car ? AITraffic::Of(*car).corner : 1.0f;
            const bool cop = *Memory::At<const std::uint8_t>(action, Site::HighGrip) != 0;
            const bool fixed = *Memory::At<const std::uint8_t>(action, Site::FixedSpeed) != 0;

            const float current = Memory::Invoke<float>(rigidBody, Addr::IRigidBody::GetSpeed);
            const unsigned postedAt = WRoadNetwork::TrafficLanes(nav) >= kHighwayLanes ? Site::CruiseHighway : Site::CruiseDefault;
            const float posted = *Memory::At<const float>(action, postedAt) * (fixed ? 1.0f : cruise);

            float desired = posted;
            if (!fixed) {
                const auto position = Memory::Invoke<const Vector3*>(rigidBody, Addr::IRigidBody::GetPosition);
                const auto velocity = Memory::Invoke<const Vector3*>(rigidBody, Addr::IRigidBody::GetLinearVelocity);
                if (position && velocity) {
                    const float curvature = Memory::Call<float>(Addr::WRoadNav::CookieTrailCurvature, nav, position, velocity);
                    const float friction = (cop ? kCopFriction : kTrafficFriction) * corner;
                    const float limit = SpeedLimitForCurvature(friction, curvature, posted);
                    if (limit < desired) desired = limit;
                }

                if (WRoadNetwork::NavOccluded(nav)) {
                    const float mass = Memory::Invoke<float>(rigidBody, Addr::IRigidBody::GetMass);
                    float gap = Memory::Call<float>(Site::Gap, action);
                    if (gap < 0.0f) gap = 0.0f;

                    const float stopping = StoppingDistance(current * (mass * kMassScale > 1.0f ? mass * kMassScale : 1.0f));
                    if (gap < stopping) {
                        Vector3 forward = {};
                        Memory::Invoke<void>(rigidBody, Addr::IRigidBody::GetForwardVector, &forward);
                        const Vector3 lead = WRoadNetwork::NavLeadVelocity(nav);
                        float leadSpeed = lead.x * forward.x + lead.y * forward.y + lead.z * forward.z;

                        if (leadSpeed < kOncoming && gap > kEmergencyGap) {
                            AITraffic::Note(5);
                        } else {
                            if (leadSpeed < 0.0f) leadSpeed = 0.0f;
                            desired = Clamp(leadSpeed * gap / stopping, 0.0f, desired);
                            AITraffic::Note(0);
                        }
                    }
                }
            }

            if (!cop && desired > current + (dT + dT)) desired = current + (dT + dT);

            if (car && Stalled(*car, vehicleAI, current)) {
                const float gap = Memory::Call<float>(Site::Gap, action);
                if (gap > kStallGap) {
                    const float roll = posted < kBreakSpeed ? posted : kBreakSpeed;
                    if (desired < roll) desired = roll;
                    AITraffic::Note(4);
                }
            }

            return desired;
        }

        bool GuardedComputeSpeed(void* action, float dT, float* speed) {
            __try {
                *speed = ComputeSpeed(action, dT);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        void GuardedStore(void* action, float value) {
            __try {
                *Memory::At<float>(action, Site::DriveSpeed) = value;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        }

        float __fastcall DriveSpeed(void* action, void* edx, float dT) {
            const float carbon = reinterpret_cast<DriveSpeedMethod>(gDriveSpeed)(action, edx, dT);

            float speed = carbon;
            if (!GuardedComputeSpeed(action, dT, &speed)) return carbon;

            GuardedStore(action, speed);
            return speed;
        }

    }

    void Install() {
        Hook::SwapSlot("AIActionTraffic keep driving", Site::CanRun, reinterpret_cast<void*>(&CanRun), &gCanRun);
        Hook::Detour("AIActionTraffic UpdateNavPos (MW look-ahead)", Site::Advance, reinterpret_cast<void*>(&Advance), &gAdvance);
        Hook::RedirectCall("AIActionTraffic state machine (MW has none)", Site::StateMachineCall, reinterpret_cast<void*>(&StateMachine), &gStateMachine);
        Hook::RedirectCall("AIActionTraffic ComputeSpeed (MW)", Site::DriveSpeedCall, reinterpret_cast<void*>(&DriveSpeed), &gDriveSpeed);
    }

}
