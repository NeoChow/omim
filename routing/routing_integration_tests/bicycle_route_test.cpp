#include "testing/testing.hpp"

#include "routing/routing_callbacks.hpp"

#include "routing/routing_integration_tests/routing_test_tools.hpp"

#include "geometry/mercator.hpp"

#include "base/math.hpp"

using namespace integration;
using namespace routing;
using namespace routing::turns;

UNIT_TEST(RussiaMoscowSevTushinoParkPreferingBicycleWay)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(55.87445, 37.43711), {0., 0.},
      MercatorBounds::FromLatLon(55.87203, 37.44274), 460.0);
}

UNIT_TEST(RussiaMoscowNahimovskyLongRoute)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(55.66151, 37.63320), {0., 0.},
      MercatorBounds::FromLatLon(55.67695, 37.56220), 5670.0);
}

UNIT_TEST(RussiaDomodedovoSteps)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(55.44010, 37.77416), {0., 0.},
      MercatorBounds::FromLatLon(55.43975, 37.77272), 100.0);
}

UNIT_TEST(SwedenStockholmCyclewayPriority)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(59.33151, 18.09347), {0., 0.},
      MercatorBounds::FromLatLon(59.33052, 18.09391), 113.0);
}

// Note. If the closest to start or finish road has "bicycle=no" tag the closest road where
// it's allowed to ride bicycle will be found.
UNIT_TEST(NetherlandsAmsterdamBicycleNo)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(52.32716, 5.05932), {0., 0.},
      MercatorBounds::FromLatLon(52.32587, 5.06121), 363.4);
}

UNIT_TEST(NetherlandsAmsterdamBicycleYes)
{
  TRouteResult const routeResult =
      CalculateRoute(GetVehicleComponents(VehicleType::Bicycle),
                                  MercatorBounds::FromLatLon(52.32872, 5.07527), {0.0, 0.0},
                                  MercatorBounds::FromLatLon(52.33853, 5.08941));

  Route const & route = *routeResult.first;
  RouterResultCode const result = routeResult.second;
  TEST_EQUAL(result, RouterResultCode::NoError, ());
  TEST(base::AlmostEqualAbs(route.GetTotalTimeSec(), 334.69, 1.0), (route.GetTotalTimeSec()));
}

// This test on tag cycleway=opposite for a streets which have oneway=yes.
// It means bicycles may go in the both directions.
UNIT_TEST(NetherlandsAmsterdamSingelStCyclewayOpposite)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(52.37571, 4.88591), {0., 0.},
      MercatorBounds::FromLatLon(52.37736, 4.88744), 212.8);
}

UNIT_TEST(RussiaMoscowKashirskoe16ToCapLongRoute)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(55.66230, 37.63214), {0., 0.},
      MercatorBounds::FromLatLon(55.68927, 37.70356), 7075.0);
}

// No pass through service road in Russia
UNIT_TEST(RussiaMoscowNoServicePassThrough)
{
  TRouteResult route =
        integration::CalculateRoute(integration::GetVehicleComponents(VehicleType::Bicycle),
                                    MercatorBounds::FromLatLon(55.66230, 37.63214), {0., 0.},
                                    MercatorBounds::FromLatLon(55.68895, 37.70286));
  TEST_EQUAL(route.second, RouterResultCode::RouteNotFound, ());
}

// TODO: This test doesn't pass because routing::RouteWeight::operator<
// prefer roads with less number of barriers. It will be more useful to consider
// barriers only with access=no/private/etc tag.

//UNIT_TEST(RussiaKerchStraitFerryRoute)
//{
//  CalculateRouteAndTestRouteLength(
//      GetVehicleComponents(VehicleType::Bicycle),
//      MercatorBounds::FromLatLon(45.4167, 36.7658), {0.0, 0.0},
//      MercatorBounds::FromLatLon(45.3653, 36.6161), 18000.0);
//}

// Test on building bicycle route past ferry.
UNIT_TEST(SwedenStockholmBicyclePastFerry)
{
  CalculateRouteAndTestRouteLength(
      GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(59.4725, 18.51355), {0.0, 0.0},
      MercatorBounds::FromLatLon(59.32967, 18.075), 66161.2);
}

UNIT_TEST(CrossMwmKaliningradRegionToLiepaja)
{
  integration::CalculateRouteAndTestRouteLength(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(55.15414, 20.85378), {0., 0.},
      MercatorBounds::FromLatLon(56.51119, 21.01847), 192000);
}

// Test on riding up from Adeje (sea level) to Vilaflor (altitude 1400 meters).
UNIT_TEST(SpainTenerifeAdejeVilaflor)
{
  integration::CalculateRouteAndTestRouteTime(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(28.11984, -16.72592), {0.0, 0.0},
      MercatorBounds::FromLatLon(28.15865, -16.63704), 18019.6 /* expectedTimeSeconds */);
}

// Test on riding down from Vilaflor (altitude 1400 meters) to Adeje (sea level).
UNIT_TEST(SpainTenerifeVilaflorAdeje)
{
  integration::CalculateRouteAndTestRouteTime(
      integration::GetVehicleComponents(VehicleType::Bicycle),
      MercatorBounds::FromLatLon(28.15865, -16.63704), {0.0, 0.0},
      MercatorBounds::FromLatLon(28.11984, -16.72592), 8868.36 /* expectedTimeSeconds */);
}
