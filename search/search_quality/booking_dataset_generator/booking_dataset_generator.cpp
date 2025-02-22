#include "search/result.hpp"
#include "search/search_quality/helpers.hpp"
#include "search/search_quality/sample.hpp"
#include "search/utils.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/storage.hpp"
#include "storage/storage_defines.hpp"

#include "platform/local_country_file.hpp"
#include "platform/local_country_file_utils.hpp"
#include "platform/platform.hpp"

#include "geometry/mercator.hpp"

#include "base/file_name_utils.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "3party/gflags/src/gflags/gflags.h"

#include "defines.hpp"

using namespace search::search_quality;
using namespace search;
using namespace std;
using namespace storage;

DEFINE_string(data_path, "", "Path to data directory (resources dir)");
DEFINE_string(mwm_path, "", "Path to mwm files (writable dir)");
DEFINE_string(out_path, "samples.json", "Path to output samples file");

string GetSampleString(FeatureType & hotel, m2::PointD const & userPos)
{
  Sample sample;
  string hotelName;
  double constexpr kViewportRadiusM = 1000.0;
  if (!hotel.GetName(StringUtf8Multilang::kEnglishCode, hotelName) &&
      !hotel.GetName(StringUtf8Multilang::kDefaultCode, hotelName))
  {
    LOG(LINFO, ("Cannot get name for", hotel.GetID()));
    return "";
  }

  sample.m_query = strings::MakeUniString(hotelName + " ");
  sample.m_locale = "en";
  sample.m_pos = userPos;
  sample.m_viewport = MercatorBounds::RectByCenterXYAndSizeInMeters(userPos, kViewportRadiusM);
  sample.m_results.push_back(Sample::Result::Build(hotel, Sample::Result::Relevance::Vital));
  string json;
  Sample::SerializeToJSONLines({sample}, json);
  return json;
}

int main(int argc, char * argv[])
{
  ChangeMaxNumberOfOpenFiles(kMaxOpenFiles);
  CheckLocale();

  google::SetUsageMessage("Booking dataset generator.");
  google::ParseCommandLineFlags(&argc, &argv, true);

  SetPlatformDirs(FLAGS_data_path, FLAGS_mwm_path);

  classificator::Load();

  FrozenDataSource dataSource;
  InitDataSource(dataSource, "" /* mwmListPath */);

  ofstream out;
  out.open(FLAGS_out_path);
  if (!out.is_open())
  {
    LOG(LERROR, ("Can't open output file", FLAGS_out_path));
    return -1;
  }

  auto const & hotelChecker = ftypes::IsBookingHotelChecker::Instance();

  // For all airports from World.mwm (international or other important airports) and all
  // hotels which are closer than 100 km from airport we create sample with query=|hotel name| and
  // viewport and position in the airport.
  double constexpr kDistanceToHotelM = 1e5;
  std::set<FeatureID> hotelsNextToAirport;
  {
    auto const handle = FindWorld(dataSource);
    if (!handle.IsAlive())
    {
      LOG(LERROR, ("Cannot find World.mwm"));
      return -1;
    }

    auto const & airportChecker = ftypes::IsAirportChecker::Instance();
    FeaturesLoaderGuard const guard(dataSource, handle.GetId());
    for (uint32_t i = 0; i < guard.GetNumFeatures(); ++i)
    {
      auto airport = guard.GetFeatureByIndex(i);
      if (!airportChecker(*airport))
        continue;

      auto const airportPos = feature::GetCenter(*airport);
      auto addHotel = [&](FeatureType & hotel) {
        if (!hotelChecker(hotel))
          return;

        if (MercatorBounds::DistanceOnEarth(airportPos, feature::GetCenter(hotel)) >
            kDistanceToHotelM)
        {
          return;
        }

        string json = GetSampleString(hotel, airportPos);
        if (json.empty())
          return;
        out << json;
        hotelsNextToAirport.insert(hotel.GetID());
      };

      dataSource.ForEachInRect(
          addHotel, MercatorBounds::RectByCenterXYAndSizeInMeters(airportPos, kDistanceToHotelM),
          scales::GetUpperScale());
    }
    LOG(LINFO, (hotelsNextToAirport.size(), "hotels have nearby airport."));
  }

  // For all hotels without an airport nearby we set user position 100km away from hotel.
  vector<shared_ptr<MwmInfo>> infos;
  dataSource.GetMwmsInfo(infos);
  for (auto const & info : infos)
  {
    auto handle = dataSource.GetMwmHandleById(MwmSet::MwmId(info));
    if (!handle.IsAlive())
    {
      LOG(LERROR, ("Mwm reading error", info));
      return -1;
    }
    FeaturesLoaderGuard const guard(dataSource, handle.GetId());
    for (uint32_t i = 0; i < guard.GetNumFeatures(); ++i)
    {
      auto hotel = guard.GetFeatureByIndex(i);
      if (!hotelChecker(*hotel))
        continue;
      if (hotelsNextToAirport.count(hotel->GetID()) != 0)
        continue;

      static double kRadiusToHotelM = kDistanceToHotelM / sqrt(2.0);
      string json = GetSampleString(
          *hotel,
          MercatorBounds::GetSmPoint(feature::GetCenter(*hotel), kRadiusToHotelM, kRadiusToHotelM));

      if (!json.empty())
        out << json;
    }
  }

  return 0;
}
