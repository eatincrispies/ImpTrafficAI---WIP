#pragma once
#include "WRoadNetwork.h"

namespace CollisionWorld {

    bool GroundHeight(const WRoadNetwork::Vector3& position, float* height);
    bool HitsWorld(const WRoadNetwork::Vector3& from, const WRoadNetwork::Vector3& to);

}
