#include <cstdint>
#include "CollisionWorld.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Memory.h"

namespace CollisionWorld {

    namespace {

        namespace Site = Addr::CollisionWorld;

        struct Manager {
            std::uint32_t surfaceExclusions;
            std::uint32_t primitives;
        };

    }

    bool GroundHeight(const WRoadNetwork::Vector3& position, float* height) {
        Manager manager = { 0u, Site::Primitives };
        return Memory::Call<bool>(Site::GroundHeight, &manager, &position, height,
                                  static_cast<WRoadNetwork::Vector3*>(nullptr));
    }

    bool HitsWorld(const WRoadNetwork::Vector3& from, const WRoadNetwork::Vector3& to) {
        Manager manager = { 0u, Site::Primitives };
        alignas(16) float segment[8] = { from.x, from.y, from.z, 1.0f, to.x, to.y, to.z, 1.0f };
        alignas(16) std::uint8_t info[Site::InfoSize] = {};

        Memory::Call<void*>(Site::InitInfo, info);
        Memory::Call<bool>(Site::CheckHitWorld, &manager, static_cast<const float*>(segment),
                           static_cast<void*>(info), Site::WorldOnly);
        return info[Site::HitType] != 0;
    }

}
