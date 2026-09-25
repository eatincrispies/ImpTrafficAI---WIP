#include "WRoadNetwork.h"
#include "../Hooks/Addresses.h"
#include "../Hooks/Memory.h"

namespace WRoadNetwork {

    namespace {

        namespace Nav = Addr::WRoadNav;
        namespace Net = Addr::WRoadNetwork;

        const std::uint8_t* Record(std::uintptr_t table, std::uintptr_t countVa, int index, unsigned size) {
            const auto base = *Memory::Global<const std::uint8_t*>(table);
            const auto count = *Memory::Global<const std::int32_t>(countVa);
            if (!base || index < 0 || index >= count) return nullptr;
            return base + static_cast<std::size_t>(index) * size;
        }

    }

    bool NavValid(const void* nav) {
        return *Memory::At<const std::uint8_t>(nav, Nav::Valid) != 0;
    }

    bool NavOccluded(const void* nav) {
        return *Memory::At<const std::uint8_t>(nav, Nav::Occluded) != 0;
    }

    Vector3 NavLeadVelocity(const void* nav) {
        return *Memory::At<const Vector3>(nav, Nav::LeadVelocity);
    }

    int TrafficLanes(const void* nav) {
        const int node = *Memory::At<const std::int8_t>(nav, Nav::NodeInd);
        const int segmentIndex = *Memory::At<const std::int16_t>(nav, Nav::Segment);
        if (node != 0 && node != 1) return 0;

        const std::uint8_t* segment = Record(Net::Segments, Net::SegmentCount, segmentIndex, Net::SegmentSize);
        if (!segment) return 0;

        const int nodeIndex = *Memory::At<const std::uint16_t>(segment, static_cast<unsigned>(node) * 2u);
        const std::uint8_t* nodeRecord = Record(Net::Nodes, Net::NodeCount, nodeIndex, Net::NodeSize);
        if (!nodeRecord) return 0;

        const int profileIndex = *Memory::At<const std::int16_t>(nodeRecord, Net::NodeProfile);
        const std::uint8_t* profile = Record(Net::Profiles, Net::ProfileCount, profileIndex, Net::ProfileSize);
        if (!profile) return 0;

        const int lanes = profile[0] > static_cast<int>(Net::MaxLanes) ? static_cast<int>(Net::MaxLanes) : profile[0];
        int traffic = 0;
        for (int lane = 0; lane < lanes; ++lane) {
            const auto laneData = *Memory::At<const std::uint32_t>(profile, Net::ProfileLanes + static_cast<unsigned>(lane) * 4u);
            if ((laneData & 0xFu) == Net::TrafficLane) ++traffic;
        }
        return traffic;
    }

    Vector3 NavPosition(const void* nav) {
        return *Memory::At<const Vector3>(nav, Nav::Position);
    }

    Vector3 NavForward(const void* nav) {
        return *Memory::At<const Vector3>(nav, Nav::Forward);
    }

    bool IsTrafficLane(const void* nav) {
        if (!NavValid(nav)) return false;

        const int node = *Memory::At<const std::int8_t>(nav, Nav::NodeInd);
        const int segmentIndex = *Memory::At<const std::int16_t>(nav, Nav::Segment);
        const int lane = *Memory::At<const std::int8_t>(nav, Nav::Lane);
        if (node != 0 && node != 1) return false;

        const std::uint8_t* segment = Record(Net::Segments, Net::SegmentCount, segmentIndex, Net::SegmentSize);
        if (!segment) return false;

        const int nodeIndex = *Memory::At<const std::uint16_t>(segment, static_cast<unsigned>(node) * 2u);
        const std::uint8_t* nodeRecord = Record(Net::Nodes, Net::NodeCount, nodeIndex, Net::NodeSize);
        if (!nodeRecord) return false;

        const int profileIndex = *Memory::At<const std::int16_t>(nodeRecord, Net::NodeProfile);
        const std::uint8_t* profile = Record(Net::Profiles, Net::ProfileCount, profileIndex, Net::ProfileSize);
        if (!profile) return false;

        const int lanes = profile[0];
        const int middle = profile[1];
        if (lanes <= 0 || lanes > static_cast<int>(Net::MaxLanes) || lane < 0 || lane >= lanes) return false;

        const auto laneData = *Memory::At<const std::uint32_t>(profile, Net::ProfileLanes + static_cast<unsigned>(lane) * 4u);
        if ((laneData & 0xFu) != Net::TrafficLane) return false;

        const auto flags = *Memory::At<const std::uint16_t>(segment, Net::SegmentFlags);
        const unsigned inverted = node == 0 ? (flags >> 10) & 1u : (flags >> 9) & 1u;
        const bool upper = (inverted ^ static_cast<unsigned>(node == 1)) != 0;
        return upper ? lane >= middle : lane < middle;
    }

    int NavSegment(const void* nav) {
        return *Memory::At<const std::int16_t>(nav, Nav::Segment);
    }

    int NavNode(const void* nav) {
        return *Memory::At<const std::int8_t>(nav, Nav::NodeInd);
    }

    bool SameRoad(int first, int second) {
        if (first == second) return true;

        const std::uint8_t* a = Record(Net::Segments, Net::SegmentCount, first, Net::SegmentSize);
        const std::uint8_t* b = Record(Net::Segments, Net::SegmentCount, second, Net::SegmentSize);
        if (!a || !b) return false;

        const auto a0 = *Memory::At<const std::uint16_t>(a, 0);
        const auto a1 = *Memory::At<const std::uint16_t>(a, 2);
        const auto b0 = *Memory::At<const std::uint16_t>(b, 0);
        const auto b1 = *Memory::At<const std::uint16_t>(b, 2);
        return a0 == b0 || a0 == b1 || a1 == b0 || a1 == b1;
    }

    bool Connected(int first, int second) {
        if (SameRoad(first, second)) return true;

        const std::uint8_t* segment = Record(Net::Segments, Net::SegmentCount, first, Net::SegmentSize);
        if (!segment) return false;

        for (unsigned end = 0; end < 2; ++end) {
            const int nodeIndex = *Memory::At<const std::uint16_t>(segment, end * 2u);
            const std::uint8_t* node = Record(Net::Nodes, Net::NodeCount, nodeIndex, Net::NodeSize);
            if (!node) continue;

            const unsigned count = node[Net::NodeSegmentCount];
            for (unsigned i = 0; i < count && i < Net::MaxNodeSegments; ++i) {
                const int neighbour = *Memory::At<const std::uint16_t>(node, Net::NodeSegments + i * 2u);
                if (SameRoad(neighbour, second)) return true;
            }
        }
        return false;
    }

    void ConstructNav(void* storage) {
        Memory::Call<void*>(Nav::Construct, storage);
    }

    void DestructNav(void* storage) {
        Memory::Call<void>(Nav::Destruct, storage);
    }

    void CopyNav(void* destination, const void* source) {
        Memory::Call<void>(Nav::CopyFrom, destination, source, 0);
    }

}
