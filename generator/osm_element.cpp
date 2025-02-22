#include "generator/osm_element.hpp"

#include "base/string_utils.hpp"
#include "coding/parse_xml.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

std::string DebugPrint(OsmElement::EntityType type)
{
  switch (type)
  {
  case OsmElement::EntityType::Unknown:
    return "unknown";
  case OsmElement::EntityType::Way:
    return "way";
  case OsmElement::EntityType::Tag:
    return "tag";
  case OsmElement::EntityType::Relation:
    return "relation";
  case OsmElement::EntityType::Osm:
    return "osm";
  case OsmElement::EntityType::Node:
    return "node";
  case OsmElement::EntityType::Nd:
    return "nd";
  case OsmElement::EntityType::Member:
    return "member";
  }
  UNREACHABLE();
}

void OsmElement::AddTag(std::string const & key, std::string const & value)
{
  // Seems like source osm data has empty values. They are useless for us.
  if (key.empty() || value.empty())
    return;

#define SKIP_KEY(skippedKey) if (strncmp(key.data(), skippedKey, sizeof(key)-1) == 0) return;
  // OSM technical info tags
  SKIP_KEY("created_by");
  SKIP_KEY("source");
  SKIP_KEY("odbl");
  SKIP_KEY("note");
  SKIP_KEY("fixme");
  SKIP_KEY("iemv");

  // Skip tags for speedup, now we don't use it
  SKIP_KEY("not:");
  SKIP_KEY("artist_name");
  SKIP_KEY("whitewater"); // https://wiki.openstreetmap.org/wiki/Whitewater_sports

  // In future we can use this tags for improve our search
  SKIP_KEY("old_name");
  SKIP_KEY("alt_name");
  SKIP_KEY("nat_name");
  SKIP_KEY("reg_name");
  SKIP_KEY("loc_name");
  SKIP_KEY("lock_name");
  SKIP_KEY("local_name");
  SKIP_KEY("short_name");
  SKIP_KEY("official_name");
#undef SKIP_KEY

  std::string val{std::string{value}};
  strings::Trim(val);
  m_tags.emplace_back(std::string{key}, std::move(val));
}

bool OsmElement::HasTag(std::string const & key) const
{
  return std::any_of(m_tags.begin(), m_tags.end(), [&](auto const & t) {
    return t.m_key == key;
  });
}

bool OsmElement::HasTag(std::string const & key, std::string const & value) const
{
  return std::any_of(m_tags.begin(), m_tags.end(), [&](auto const & t) {
    return t.m_key == key && t.m_value == value;
  });
}

bool OsmElement::HasAnyTag(std::unordered_multimap<std::string, std::string> const & tags) const
{
  return std::any_of(std::begin(m_tags), std::end(m_tags), [&](auto const & t) {
    auto beginEnd = tags.equal_range(t.m_key);
    for (auto it = beginEnd.first; it != beginEnd.second; ++it)
    {
      if (it->second == t.m_value)
        return true;
    }

    return false;
  });
}

std::string OsmElement::ToString(std::string const & shift) const
{
  std::stringstream ss;
  ss << (shift.empty() ? "\n" : shift);
  switch (m_type)
  {
  case EntityType::Node:
    ss << "Node: " << m_id << " (" << std::fixed << std::setw(7) << m_lat << ", " << m_lon << ")"
       << " tags: " << m_tags.size();
    break;
  case EntityType::Nd:
    ss << "Nd ref: " << m_ref;
    break;
  case EntityType::Way:
    ss << "Way: " << m_id << " nds: " << m_nodes.size() << " tags: " << m_tags.size();
    if (!m_nodes.empty())
    {
      std::string shift2 = shift;
      shift2 += shift2.empty() ? "\n  " : "  ";
      for (auto const & e : m_nodes)
        ss << shift2 << e;
    }
    break;
  case EntityType::Relation:
    ss << "Relation: " << m_id << " members: " << m_members.size() << " tags: " << m_tags.size();
    if (!m_members.empty())
    {
      std::string shift2 = shift;
      shift2 += shift2.empty() ? "\n  " : "  ";
      for (auto const & e : m_members)
        ss << shift2 << e.m_ref << " " << DebugPrint(e.m_type) << " " << e.m_role;
    }
    break;
  case EntityType::Tag:
    ss << "Tag: " << m_k << " = " << m_v;
    break;
  case EntityType::Member:
    ss << "Member: " << m_ref << " type: " << DebugPrint(m_memberType) << " role: " << m_role;
    break;
  case EntityType::Unknown:
  case EntityType::Osm:
    UNREACHABLE();
    break;
  }

  if (!m_tags.empty())
  {
    std::string shift2 = shift;
    shift2 += shift2.empty() ? "\n  " : "  ";
    for (auto const & e : m_tags)
      ss << shift2 << e.m_key << " = " << e.m_value;
  }
  return ss.str();
}

std::string OsmElement::GetTag(std::string const & key) const
{
  auto const it = std::find_if(m_tags.cbegin(), m_tags.cend(),
                               [&key](Tag const & tag) { return tag.m_key == key; });

  return it == m_tags.cend() ? std::string() : it->m_value;
}

std::string OsmElement::GetTagValue(std::string const & key,
                                    std::string const & defaultValue) const
{
  auto const it = std::find_if(m_tags.cbegin(), m_tags.cend(),
                               [&key](Tag const & tag) { return tag.m_key == key; });

  return it != m_tags.cend() ? it->m_value : defaultValue;
}

std::string DebugPrint(OsmElement const & element)
{
  return element.ToString();
}

std::string DebugPrint(OsmElement::Tag const & tag)
{
  std::stringstream ss;
  ss << tag.m_key << '=' << tag.m_value;
  return ss.str();
}

base::GeoObjectId GetGeoObjectId(OsmElement const & element)
{
  switch (element.m_type)
  {
  case OsmElement::EntityType::Node:
    return base::MakeOsmNode(element.m_id);
  case OsmElement::EntityType::Way:
    return base::MakeOsmWay(element.m_id);
  case OsmElement::EntityType::Relation:
    return base::MakeOsmRelation(element.m_id);
  case OsmElement::EntityType::Member:
  case OsmElement::EntityType::Nd:
  case OsmElement::EntityType::Osm:
  case OsmElement::EntityType::Tag:
  case OsmElement::EntityType::Unknown:
    UNREACHABLE();
    return base::GeoObjectId();
  }
  UNREACHABLE();
}
