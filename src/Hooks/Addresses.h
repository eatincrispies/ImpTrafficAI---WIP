#pragma once
#include <cstddef>
#include <cstdint>

namespace Addr {

    struct CodeBytes {
        std::uintptr_t va;
        std::uint8_t   bytes[8];
        std::size_t    size;
    };

    struct Slot {
        std::uintptr_t va;
        std::uintptr_t expected;
    };

    inline constexpr std::uintptr_t ImageBase     = 0x00400000u;
    inline constexpr std::uint32_t  TimeDateStamp = 0x562B029Au;
    inline constexpr std::uint32_t  SizeOfImage   = 0x0082B34Eu;

    namespace AIActionTraffic {
        inline constexpr CodeBytes Advance          = { 0x00414160u, { 0x83, 0xEC, 0x64, 0x53, 0x55, 0x8B, 0xE9 }, 7 };
        inline constexpr CodeBytes DriveSpeedCall   = { 0x00435E03u, { 0xE8, 0x58, 0x86, 0xFF, 0xFF }, 5 };
        inline constexpr CodeBytes StateMachineCall = { 0x00435DE1u, { 0xE8, 0x7A, 0xEC, 0xFE, 0xFF }, 5 };

        inline constexpr Slot CanRun = { 0x009C2BB4u, 0x00407AB0u };

        inline constexpr std::uintptr_t Gap = 0x004144B0u;

        inline constexpr unsigned VehicleAI     = 0x38u;
        inline constexpr unsigned HighGrip      = 0x44u;
        inline constexpr unsigned FixedSpeed    = 0x47u;
        inline constexpr unsigned RigidBody     = 0x50u;
        inline constexpr unsigned DriveSpeed    = 0x6Cu;
        inline constexpr unsigned CruiseDefault = 0x7Cu;
        inline constexpr unsigned CruiseHighway = 0x80u;

        inline constexpr int StateStopped = 0;
        inline constexpr int StateCruise  = 1;
    }

    namespace AIVehicleTraffic {
        inline constexpr Slot ResetDriveToNav = { 0x009C408Cu, 0x00427AD0u };
        inline constexpr Slot OnGasBrake      = { 0x009C41B0u, 0x00409B10u };

        inline constexpr CodeBytes SteeringGain = { 0x00409A30u, { 0x83, 0xEC, 0x08, 0x57, 0x8B, 0xF9 }, 6 };

        inline constexpr unsigned VehicleAI      = 0x44u;
        inline constexpr unsigned Input          = 0x4Cu;
        inline constexpr unsigned DriveSpeed     = 0x94u;
        inline constexpr unsigned ReversingSpeed = 0xBCu;
        inline constexpr unsigned SteeringBehind = 0xBDu;
        inline constexpr unsigned DriveFlags     = 0xF4u;
        inline constexpr unsigned Pose           = 0xF8u;
        inline constexpr unsigned Transmission   = 0xFCu;
    }

    namespace IInput {
        inline constexpr unsigned SetControlGas   = 0x2Cu;
        inline constexpr unsigned SetControlBrake = 0x30u;
    }

    namespace ITransmission {
        inline constexpr unsigned GetGear     = 0x04u;
        inline constexpr unsigned Shift       = 0x0Cu;
        inline constexpr unsigned IsReversing = 0x14u;

        inline constexpr int GearNeutral = 1;
        inline constexpr int GearFirst   = 2;
    }

    namespace Pose {
        inline constexpr unsigned GetPosition = 0x04u;
        inline constexpr unsigned GetMatrix   = 0xA4u;
        inline constexpr unsigned ForwardX    = 0x20u;
        inline constexpr unsigned ForwardZ    = 0x28u;
    }

    namespace IPlayer {
        inline constexpr std::uintptr_t List  = 0x00A9FF5Cu;
        inline constexpr std::uintptr_t Count = 0x00A9FF64u;

        inline constexpr unsigned GetSimable = 0x04u;
    }

    namespace ISimable {
        inline constexpr unsigned GetRigidBody = 0x54u;
    }

    namespace IVehicleAI {
        inline constexpr unsigned GetVehicle    = 0x08u;
        inline constexpr unsigned GetDriveSpeed = 0x38u;
        inline constexpr unsigned GetDriveToNav = 0x4Cu;
    }

    namespace IVehicle {
        inline constexpr unsigned GetDriverClass   = 0x6Cu;
        inline constexpr unsigned GetOffscreenTime = 0x74u;
        inline constexpr unsigned InShock          = 0x8Cu;
        inline constexpr unsigned GetSpeed         = 0xA4u;
        inline constexpr unsigned GetAIVehiclePtr  = 0xC0u;
    }

    namespace IRigidBody {
        inline constexpr unsigned GetRadius          = 0x14u;
        inline constexpr unsigned GetMass            = 0x18u;
        inline constexpr unsigned GetPosition        = 0x20u;
        inline constexpr unsigned GetLinearVelocity  = 0x24u;
        inline constexpr unsigned GetAngularVelocity = 0x28u;
        inline constexpr unsigned GetSpeed           = 0x2Cu;
        inline constexpr unsigned GetForwardVector   = 0x34u;
        inline constexpr unsigned GetUpVector        = 0x3Cu;
        inline constexpr unsigned SetLinearVelocity  = 0x60u;
        inline constexpr unsigned SetAngularVelocity = 0x64u;
    }

    namespace RandomSurveyor {
        inline constexpr CodeBytes ValidateCall = { 0x0044665Bu, { 0xE8, 0x00, 0x0F, 0xFC, 0xFF }, 5 };

        inline constexpr unsigned Nav = 0x48u;
    }

    namespace AITrafficManager {
        inline constexpr CodeBytes KeepCall = { 0x00445138u, { 0xE8, 0x33, 0xDC, 0xFC, 0xFF }, 5 };
    }

    namespace WRoadNav {
        inline constexpr std::uintptr_t InitAtPoint          = 0x0080F180u;
        inline constexpr std::uintptr_t CookieTrailCurvature = 0x007FB430u;
        inline constexpr std::uintptr_t Construct            = 0x00806820u;
        inline constexpr std::uintptr_t Destruct             = 0x007F7BF0u;
        inline constexpr std::uintptr_t CopyFrom             = 0x00801BE0u;
        inline constexpr std::size_t    Size                 = 0x340u;

        inline constexpr unsigned Valid        = 0x58u;
        inline constexpr unsigned Occluded     = 0x5Fu;
        inline constexpr unsigned NodeInd      = 0x88u;
        inline constexpr unsigned Segment      = 0x8Au;
        inline constexpr unsigned Position     = 0x94u;
        inline constexpr unsigned Forward      = 0xB8u;
        inline constexpr unsigned LeadVelocity = 0x178u;
        inline constexpr unsigned Lane         = 0x2E1u;
    }

    namespace WRoadNetwork {
        inline constexpr std::uintptr_t Profiles     = 0x00B77EC4u;
        inline constexpr std::uintptr_t Nodes        = 0x00B77EC8u;
        inline constexpr std::uintptr_t Segments     = 0x00B77ECCu;
        inline constexpr std::uintptr_t ProfileCount = 0x00B77E94u;
        inline constexpr std::uintptr_t NodeCount    = 0x00B77E8Cu;
        inline constexpr std::uintptr_t SegmentCount = 0x00B77E84u;

        inline constexpr unsigned SegmentSize  = 0x16u;
        inline constexpr unsigned SegmentFlags = 0x0Au;
        inline constexpr unsigned NodeSize     = 0x20u;
        inline constexpr unsigned NodeProfile  = 0x0Eu;
        inline constexpr unsigned NodeSegmentCount = 0x10u;
        inline constexpr unsigned NodeSegments     = 0x12u;
        inline constexpr unsigned MaxNodeSegments  = 7u;
        inline constexpr unsigned ProfileSize  = 0x40u;
        inline constexpr unsigned ProfileLanes = 0x04u;
        inline constexpr unsigned MaxLanes     = 15u;
        inline constexpr unsigned TrafficLane  = 1u;
    }

    namespace CollisionWorld {
        inline constexpr std::uintptr_t GroundHeight  = 0x00816DF0u;
        inline constexpr std::uintptr_t CheckHitWorld = 0x00814D70u;
        inline constexpr std::uintptr_t InitInfo      = 0x00404A20u;
        inline constexpr std::size_t    InfoSize      = 0xC0u;
        inline constexpr unsigned       HitType       = 0x51u;
        inline constexpr std::uint32_t  Primitives    = 3u;
        inline constexpr std::uint32_t  WorldOnly     = 1u;
    }

}
