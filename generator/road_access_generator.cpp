#include "generator/road_access_generator.hpp"

#include "generator/routing_helpers.hpp"

#include "routing/road_access.hpp"
#include "routing/road_access_serialization.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/features_vector.hpp"

#include "coding/file_container.hpp"
#include "coding/file_writer.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include <initializer_list>

#include "defines.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "boost/optional.hpp"

using namespace feature;
using namespace routing;
using namespace std;

namespace
{
char constexpr kDelim[] = " \t\r\n";

using TagMapping = routing::RoadAccessTagProcessor::TagMapping;

TagMapping const kMotorCarTagMapping = {
    {OsmElement::Tag("motorcar", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motorcar", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("motorcar", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("motorcar", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kMotorVehicleTagMapping = {
    {OsmElement::Tag("motor_vehicle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("motor_vehicle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("motor_vehicle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("motor_vehicle", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kVehicleTagMapping = {
    {OsmElement::Tag("vehicle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("vehicle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("vehicle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("vehicle", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kCarBarriersTagMapping = {
    {OsmElement::Tag("barrier", "block"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "bollard"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "cycle_barrier"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "gate"), RoadAccess::Type::Private},
    {OsmElement::Tag("barrier", "lift_gate"), RoadAccess::Type::Private},
// TODO (@gmoryes) The types below should be added.
//  {OsmElement::Tag("barrier", "chain"), RoadAccess::Type::No},
//  {OsmElement::Tag("barrier", "swing_gate"), RoadAccess::Type::Private}
//  {OsmElement::Tag("barrier", "log"), RoadAccess::Type::No},
//  {OsmElement::Tag("barrier", "motorcycle_barrier"), RoadAccess::Type::No},
};

TagMapping const kPedestrianTagMapping = {
    {OsmElement::Tag("foot", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("foot", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("foot", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("foot", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kBicycleTagMapping = {
    {OsmElement::Tag("bicycle", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "designated"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("bicycle", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("bicycle", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("bicycle", "destination"), RoadAccess::Type::Destination},
};

TagMapping const kBicycleBarriersTagMapping = {
    {OsmElement::Tag("barrier", "cycle_barrier"), RoadAccess::Type::No},
    {OsmElement::Tag("barrier", "gate"), RoadAccess::Type::Private},
// TODO (@gmoryes) The types below should be added.
//  {OsmElement::Tag("barrier", "kissing_gate"), RoadAccess::Type::Private},
};

// Allow everything to keep transit section empty. We'll use pedestrian section for
// transit + pedestrian combination.
// Empty mapping leads to default RoadAccess::Type::Yes access type for all roads.
TagMapping const kTransitTagMapping = {};

TagMapping const kDefaultTagMapping = {
    {OsmElement::Tag("access", "yes"), RoadAccess::Type::Yes},
    {OsmElement::Tag("access", "permissive"), RoadAccess::Type::Yes},
    {OsmElement::Tag("access", "no"), RoadAccess::Type::No},
    {OsmElement::Tag("access", "private"), RoadAccess::Type::Private},
    {OsmElement::Tag("access", "destination"), RoadAccess::Type::Destination},
};

set<OsmElement::Tag> const kHighwaysWhereIgnorePrivateAccessForCar = {
    {OsmElement::Tag("highway", "motorway")},
    {OsmElement::Tag("highway", "motorway_link")},
    {OsmElement::Tag("highway", "primary")},
    {OsmElement::Tag("highway", "primary_link")},
    {OsmElement::Tag("highway", "secondary")},
    {OsmElement::Tag("highway", "secondary_link")},
    {OsmElement::Tag("highway", "tertiary")},
    {OsmElement::Tag("highway", "tertiary_link")},
    {OsmElement::Tag("highway", "trunk")},
    {OsmElement::Tag("highway", "trunk_link")}
};

set<OsmElement::Tag> const kHighwaysWhereIgnorePrivateAccessEmpty = {};

bool ParseRoadAccess(string const & roadAccessPath,
                     map<base::GeoObjectId, uint32_t> const & osmIdToFeatureId,
                     FeaturesVector const & featuresVector,
                     RoadAccessCollector::RoadAccessByVehicleType & roadAccessByVehicleType)
{
  ifstream stream(roadAccessPath);
  if (!stream)
  {
    LOG(LWARNING, ("Could not open", roadAccessPath));
    return false;
  }

  unordered_map<uint32_t, RoadAccess::Type> featureType[static_cast<size_t>(VehicleType::Count)];
  unordered_map<RoadPoint, RoadAccess::Type, RoadPoint::Hash> pointType[static_cast<size_t>(VehicleType::Count)];

  auto addFeature = [&](uint32_t featureId, VehicleType vehicleType,
                        RoadAccess::Type roadAccessType, uint64_t osmId) {
    auto & m = featureType[static_cast<size_t>(vehicleType)];
    auto const emplaceRes = m.emplace(featureId, roadAccessType);
    if (!emplaceRes.second && emplaceRes.first->second != roadAccessType)
    {
      LOG(LDEBUG, ("Duplicate road access info for OSM way", osmId, "vehicle:", vehicleType,
                   "access is:", emplaceRes.first->second, "tried:", roadAccessType));
    }
  };

  auto addPoint = [&](RoadPoint const & point, VehicleType vehicleType,
                      RoadAccess::Type roadAccessType) {
    auto & m = pointType[static_cast<size_t>(vehicleType)];
    auto const emplaceRes = m.emplace(point, roadAccessType);
    if (!emplaceRes.second && emplaceRes.first->second != roadAccessType)
    {
      LOG(LDEBUG, ("Duplicate road access info for road point", point, "vehicle:", vehicleType,
                   "access is:", emplaceRes.first->second, "tried:", roadAccessType));
    }
  };

  string line;
  for (uint32_t lineNo = 1;; ++lineNo)
  {
    if (!getline(stream, line))
      break;

    strings::SimpleTokenizer iter(line, kDelim);

    if (!iter)
    {
      LOG(LERROR, ("Error when parsing road access: empty line", lineNo));
      return false;
    }
    VehicleType vehicleType;
    FromString(*iter, vehicleType);
    ++iter;

    if (!iter)
    {
      LOG(LERROR, ("Error when parsing road access: no road access type at line", lineNo, "Line contents:", line));
      return false;
    }
    RoadAccess::Type roadAccessType;
    FromString(*iter, roadAccessType);
    ++iter;

    uint64_t osmId;
    if (!iter || !strings::to_uint64(*iter, osmId))
    {
      LOG(LERROR, ("Error when parsing road access: bad osm id at line", lineNo, "Line contents:", line));
      return false;
    }
    ++iter;

    uint32_t pointIdx;
    if (!iter || !strings::to_uint(*iter, pointIdx))
    {
      LOG(LERROR, ("Error when parsing road access: bad pointIdx at line", lineNo, "Line contents:", line));
      return false;
    }
    ++iter;

    auto const it = osmIdToFeatureId.find(base::MakeOsmWay(osmId));
    // Even though this osm element has a tag that is interesting for us,
    // we have not created a feature from it. Possible reasons:
    // no primary tag, unsupported type, etc.
    if (it == osmIdToFeatureId.cend())
      continue;

    uint32_t const featureId = it->second;

    if (pointIdx == 0)
      addFeature(featureId, vehicleType, roadAccessType, osmId);
    else
      addPoint(RoadPoint(featureId, pointIdx - 1), vehicleType, roadAccessType);
  }

  for (size_t i = 0; i < static_cast<size_t>(VehicleType::Count); ++i)
    roadAccessByVehicleType[i].SetAccessTypes(move(featureType[i]), move(pointType[i]));

  return true;
}

// If |elem| has access tag from |mapping|, returns corresponding RoadAccess::Type.
// Tags in |mapping| should be mutually exclusive. Caller is responsible for that. If there are
// multiple access tags from |mapping| in |elem|, returns RoadAccess::Type for any of them.
// Returns RoadAccess::Type::Count if |elem| has no access tags from |mapping|.
RoadAccess::Type GetAccessTypeFromMapping(OsmElement const & elem, TagMapping const * mapping)
{
  for (auto const & tag : elem.m_tags)
  {
    auto const it = mapping->find(tag);
    if (it != mapping->cend())
      return it->second;
  }
  return RoadAccess::Type::Count;
}
}  // namespace

namespace routing
{
// RoadAccessTagProcessor --------------------------------------------------------------------------
RoadAccessTagProcessor::RoadAccessTagProcessor(VehicleType vehicleType)
  : m_vehicleType(vehicleType)
{
  switch (vehicleType)
  {
  case VehicleType::Car:
    m_accessMappings.push_back(&kMotorCarTagMapping);
    m_accessMappings.push_back(&kMotorVehicleTagMapping);
    m_accessMappings.push_back(&kVehicleTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);
    m_barrierMappings.push_back(&kCarBarriersTagMapping);
    m_hwIgnoreBarriersWithoutAccess = &kHighwaysWhereIgnorePrivateAccessForCar;
    break;
  case VehicleType::Pedestrian:
    m_accessMappings.push_back(&kPedestrianTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);
    m_hwIgnoreBarriersWithoutAccess = &kHighwaysWhereIgnorePrivateAccessEmpty;
    break;
  case VehicleType::Bicycle:
    m_accessMappings.push_back(&kBicycleTagMapping);
    m_accessMappings.push_back(&kVehicleTagMapping);
    m_accessMappings.push_back(&kDefaultTagMapping);
    m_barrierMappings.push_back(&kBicycleBarriersTagMapping);
    m_hwIgnoreBarriersWithoutAccess = &kHighwaysWhereIgnorePrivateAccessEmpty;
    break;
  case VehicleType::Transit:
    // Use kTransitTagMapping to keep transit section empty. We'll use pedestrian section for
    // transit + pedestrian combination.
    m_accessMappings.push_back(&kTransitTagMapping);
    m_hwIgnoreBarriersWithoutAccess = &kHighwaysWhereIgnorePrivateAccessEmpty;
    break;
  case VehicleType::Count:
    CHECK(false, ("Bad vehicle type"));
    break;
  }
}

void RoadAccessTagProcessor::Process(OsmElement const & elem, ofstream & oss)
{
  // We will process all nodes before ways because of o5m format:
  // all nodes are first, then all ways, then all relations.
  if (elem.m_type == OsmElement::EntityType::Node)
  {
    RoadAccess::Type accessType = GetAccessType(elem);
    if (accessType != RoadAccess::Type::Yes)
      m_barriers[elem.m_id] = accessType;
    return;
  }

  if (elem.m_type != OsmElement::EntityType::Way)
    return;

  // All feature tags.
  auto const accessType = GetAccessType(elem);
  if (accessType != RoadAccess::Type::Yes)
    oss << ToString(m_vehicleType) << " " << ToString(accessType) << " " << elem.m_id << " "
        << 0 /* wildcard segment Idx */ << endl;

  // Barrier tags.
  for (size_t pointIdx = 0; pointIdx < elem.m_nodes.size(); ++pointIdx)
  {
    auto const it = m_barriers.find(elem.m_nodes[pointIdx]);
    if (it == m_barriers.cend())
      continue;

    RoadAccess::Type const roadAccessType = it->second;
    // idx == 0 used as wildcard segment Idx, for nodes we store |pointIdx + 1| instead of |pointIdx|.
    oss << ToString(m_vehicleType) << " " << ToString(roadAccessType) << " " << elem.m_id << " "
        << pointIdx + 1 << endl;
  }
}

bool RoadAccessTagProcessor::ShouldIgnoreBarrierWithoutAccess(OsmElement const & osmElement) const
{
  CHECK(m_hwIgnoreBarriersWithoutAccess, ());
  for (auto const & tag : osmElement.m_tags)
  {
    if (m_hwIgnoreBarriersWithoutAccess->count(tag) != 0)
      return true;
  }

  return false;
}

RoadAccess::Type RoadAccessTagProcessor::GetAccessType(OsmElement const & elem) const
{
  auto const getType = [&](vector<TagMapping const *> const & mapping)
  {
    for (auto const tagMapping : mapping)
    {
      auto const accessType = GetAccessTypeFromMapping(elem, tagMapping);
      if (accessType != RoadAccess::Type::Count)
        return boost::optional<RoadAccess::Type>(accessType);
    }

    return boost::optional<RoadAccess::Type>();
  };

  if (auto op = getType(m_accessMappings))
    return *op;

  // Apply barrier tags if we have no {vehicle = ...}, {access = ...} etc.
  if (auto op = getType(m_barrierMappings))
  {
    if (!ShouldIgnoreBarrierWithoutAccess(elem))
      return *op;
  }

  return RoadAccess::Type::Yes;
}

// RoadAccessWriter ------------------------------------------------------------
RoadAccessWriter::RoadAccessWriter(string const & filePath)
{
  for (size_t i = 0; i < static_cast<size_t>(VehicleType::Count); ++i)
    m_tagProcessors.emplace_back(static_cast<VehicleType>(i));

  Open(filePath);
}

void RoadAccessWriter::Open(string const & filePath)
{
  LOG(LINFO,
      ("Saving information about barriers and road access classes in osm id terms to", filePath));
  m_stream.open(filePath, ofstream::out);

  if (!IsOpened())
    LOG(LINFO, ("Cannot open file", filePath));
}

void RoadAccessWriter::CollectFeature(FeatureBuilder const &, OsmElement const & elem)
{
  if (!IsOpened())
  {
    LOG(LWARNING, ("Tried to write to a closed barriers writer"));
    return;
  }

  for (auto & p : m_tagProcessors)
    p.Process(elem, m_stream);
}

bool RoadAccessWriter::IsOpened() const { return m_stream && m_stream.is_open(); }

// RoadAccessCollector ----------------------------------------------------------
RoadAccessCollector::RoadAccessCollector(string const & dataFilePath, string const & roadAccessPath,
                                         string const & osmIdsToFeatureIdsPath)
{
  map<base::GeoObjectId, uint32_t> osmIdToFeatureId;
  if (!ParseRoadsOsmIdToFeatureIdMapping(osmIdsToFeatureIdsPath, osmIdToFeatureId))
  {
    LOG(LWARNING, ("An error happened while parsing feature id to osm ids mapping from file:",
                   osmIdsToFeatureIdsPath));
    m_valid = false;
    return;
  }

  FeaturesVectorTest featuresVector(dataFilePath);

  RoadAccessCollector::RoadAccessByVehicleType roadAccessByVehicleType;
  if (!ParseRoadAccess(roadAccessPath, osmIdToFeatureId, featuresVector.GetVector(),
                       roadAccessByVehicleType))
  {
    LOG(LWARNING, ("An error happened while parsing road access from file:", roadAccessPath));
    m_valid = false;
    return;
  }

  m_valid = true;
  m_roadAccessByVehicleType.swap(roadAccessByVehicleType);
}

// Functions ------------------------------------------------------------------
void BuildRoadAccessInfo(string const & dataFilePath, string const & roadAccessPath,
                         string const & osmIdsToFeatureIdsPath)
{
  LOG(LINFO, ("Generating road access info for", dataFilePath));

  RoadAccessCollector collector(dataFilePath, roadAccessPath, osmIdsToFeatureIdsPath);

  if (!collector.IsValid())
  {
    LOG(LWARNING, ("Unable to parse road access in osm terms"));
    return;
  }

  FilesContainerW cont(dataFilePath, FileWriter::OP_WRITE_EXISTING);
  FileWriter writer = cont.GetWriter(ROAD_ACCESS_FILE_TAG);

  RoadAccessSerializer::Serialize(writer, collector.GetRoadAccessAllTypes());
}
}  // namespace routing
