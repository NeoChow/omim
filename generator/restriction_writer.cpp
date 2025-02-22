#include "generator/restriction_writer.hpp"

#include "generator/intermediate_elements.hpp"
#include "generator/osm_element.hpp"
#include "generator/restriction_collector.hpp"

#include "routing/restrictions_serialization.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"
#include "base/logging.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace routing;

std::vector<std::pair<std::string, Restriction::Type>> const kRestrictionTypes ={
    {"no_entry", Restriction::Type::No},
    {"no_exit", Restriction::Type::No},
    {"no_left_turn", Restriction::Type::No},
    {"no_right_turn", Restriction::Type::No},
    {"no_straight_on", Restriction::Type::No},
    {"no_u_turn", Restriction::Type::NoUTurn},
    {"only_left_turn", Restriction::Type::Only},
    {"only_right_turn", Restriction::Type::Only},
    {"only_straight_on", Restriction::Type::Only},
    {"only_u_turn", Restriction::Type::OnlyUTurn}
};

/// \brief Converts restriction type form string to RestrictionCollector::Type.
/// \returns true if conversion was successful and false otherwise.
bool TagToType(std::string const & tag, Restriction::Type & type)
{
  auto const it = std::find_if(kRestrictionTypes.cbegin(), kRestrictionTypes.cend(),
                          [&tag](std::pair<std::string, Restriction::Type> const & v) {
    return v.first == tag;
  });
  if (it == kRestrictionTypes.cend())
    return false; // Unsupported restriction type.

  type = it->second;
  return true;
}

std::vector<RelationElement::Member> GetMembersByTag(RelationElement const & relationElement,
                                                     std::string const & tag)
{
  std::vector<RelationElement::Member> result;
  for (auto const & member : relationElement.ways)
  {
    if (member.second == tag)
      result.emplace_back(member);
  }

  for (auto const & member : relationElement.nodes)
  {
    if (member.second == tag)
      result.emplace_back(member);
  }

  return result;
};

OsmElement::EntityType GetType(RelationElement const & relationElement, uint64_t osmId)
{
  for (auto const & member : relationElement.ways)
  {
    if (member.first == osmId)
      return OsmElement::EntityType::Way;
  }

  for (auto const & member : relationElement.nodes)
  {
    if (member.first == osmId)
      return OsmElement::EntityType::Node;
  }

  UNREACHABLE();
};
}  // namespace

namespace routing
{
std::string const RestrictionWriter::kNodeString = "node";
std::string const RestrictionWriter::kWayString = "way";

RestrictionWriter::RestrictionWriter(std::string const & fullPath,
                                     generator::cache::IntermediateDataReader const & cache)
  : m_cache(cache)
{
  Open(fullPath);
}

//static
RestrictionWriter::ViaType RestrictionWriter::ConvertFromString(std::string const & str)
{
  if (str == kNodeString)
    return ViaType::Node;
  else if (str == kWayString)
    return ViaType::Way;

  CHECK(false, ("Bad via type in restrictons:", str));
  UNREACHABLE();
}

void RestrictionWriter::Open(std::string const & fullPath)
{
  LOG(LINFO, ("Saving road restrictions in osm id terms to", fullPath));
  m_stream.open(fullPath, std::ofstream::out);

  if (!IsOpened())
    LOG(LINFO, ("Cannot open file", fullPath));

  m_stream << std::setprecision(20);
}

bool ValidateOsmRestriction(std::vector<RelationElement::Member> & from,
                            std::vector<RelationElement::Member> & via,
                            std::vector<RelationElement::Member> & to,
                            RelationElement const & relationElement)
{
  if (relationElement.GetType() != "restriction")
    return false;

  from = GetMembersByTag(relationElement, "from");
  to = GetMembersByTag(relationElement, "to");
  via = GetMembersByTag(relationElement, "via");

  // TODO (@gmoryes) |from| and |to| can have size more than 1 in case of "no_entry", "no_exit"
  if (from.size() != 1 || to.size() != 1 || via.empty())
    return false;

  // Either single node is marked as via or one or more ways are marked as via.
  // https://wiki.openstreetmap.org/wiki/Relation:restriction#Members
  if (via.size() != 1)
  {
    bool const allMembersAreWays =
      std::all_of(via.begin(), via.end(),
                  [&](auto const & member)
                  {
                    return GetType(relationElement, member.first) == OsmElement::EntityType::Way;
                  });

    if (!allMembersAreWays)
      return false;
  }

  return true;
}

void RestrictionWriter::CollectRelation(RelationElement const & relationElement)
{
  if (!IsOpened())
  {
    LOG(LWARNING, ("Tried to write to a closed restrictions writer"));
    return;
  }

  std::vector<RelationElement::Member> from;
  std::vector<RelationElement::Member> via;
  std::vector<RelationElement::Member> to;

  if (!ValidateOsmRestriction(from, via, to, relationElement))
    return;

  uint64_t const fromOsmId = from.back().first;
  uint64_t const toOsmId = to.back().first;

  // Extracting type of restriction.
  auto const tagIt = relationElement.tags.find("restriction");
  if (tagIt == relationElement.tags.end())
    return;

  Restriction::Type type = Restriction::Type::No;
  if (!TagToType(tagIt->second, type))
    return;

  auto const viaType =
      GetType(relationElement, via.back().first) == OsmElement::EntityType::Node ? ViaType::Node
                                                                                 : ViaType::Way;

  auto const printHeader = [&]() { 
    m_stream << DebugPrint(type) << "," << DebugPrint(viaType) << ",";
  };

  if (viaType == ViaType::Way)
  {
    printHeader();
    m_stream << fromOsmId << ",";
    for (auto const & viaMember : via)
      m_stream << viaMember.first << ",";
  }
  else
  {
    double y = 0.0;
    double x = 0.0;
    uint64_t const viaNodeOsmId = via.back().first;
    if (!m_cache.GetNode(viaNodeOsmId, y, x))
      return;

    printHeader();
    m_stream << x << "," << y << ",";
    m_stream << fromOsmId << ",";
  }

  m_stream << toOsmId << '\n';
}

bool RestrictionWriter::IsOpened() const { return m_stream && m_stream.is_open(); }

std::string DebugPrint(RestrictionWriter::ViaType const & type)
{
  switch (type)
  {
  case RestrictionWriter::ViaType::Node: return RestrictionWriter::kNodeString;
  case RestrictionWriter::ViaType::Way: return RestrictionWriter::kWayString;
  case RestrictionWriter::ViaType::Count: UNREACHABLE();
  }
  UNREACHABLE();
}
}  // namespace routing
