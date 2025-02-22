#include "routing/index_router.hpp"

#include "routing/base/astar_progress.hpp"
#include "routing/base/bfs.hpp"

#include "routing/bicycle_directions.hpp"
#include "routing/fake_ending.hpp"
#include "routing/index_graph.hpp"
#include "routing/index_graph_loader.hpp"
#include "routing/index_graph_serialization.hpp"
#include "routing/index_graph_starter.hpp"
#include "routing/index_road_graph.hpp"
#include "routing/leaps_postprocessor.hpp"
#include "routing/pedestrian_directions.hpp"
#include "routing/restriction_loader.hpp"
#include "routing/route.hpp"
#include "routing/routing_exceptions.hpp"
#include "routing/routing_helpers.hpp"
#include "routing/routing_options.hpp"
#include "routing/single_vehicle_world_graph.hpp"
#include "routing/speed_camera_prohibition.hpp"
#include "routing/transit_info.hpp"
#include "routing/transit_world_graph.hpp"
#include "routing/turns_generator.hpp"
#include "routing/vehicle_mask.hpp"

#include "routing_common/bicycle_model.hpp"
#include "routing_common/car_model.hpp"
#include "routing_common/pedestrian_model.hpp"

#include "transit/transit_speed_limits.hpp"

#include "indexer/data_source.hpp"
#include "indexer/feature_altitude.hpp"

#include "platform/mwm_traits.hpp"

#include "geometry/mercator.hpp"
#include "geometry/parametrized_segment.hpp"
#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/stl_helpers.hpp"

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iterator>
#include <map>
#include <utility>

#include "defines.hpp"

using namespace routing;
using namespace std;

namespace
{
size_t constexpr kMaxRoadCandidates = 12;
double constexpr kProgressInterval = 0.5;
uint32_t constexpr kVisitPeriodForLeaps = 10;
uint32_t constexpr kVisitPeriod = 40;

double constexpr kLeapsStageContribution = 0.15;

// If user left the route within this range(meters), adjust the route. Else full rebuild.
double constexpr kAdjustRangeM = 5000.0;
// Full rebuild if distance(meters) is less.
double constexpr kMinDistanceToFinishM = 10000;
// Limit of adjust in seconds.
double constexpr kAdjustLimitSec = 5 * 60;

double CalcMaxSpeed(NumMwmIds const & numMwmIds,
                    VehicleModelFactoryInterface const & vehicleModelFactory,
                    VehicleType vehicleType)
{
  if (vehicleType == VehicleType::Transit)
    return transit::kTransitMaxSpeedKMpH;

  double maxSpeed = 0.0;
  numMwmIds.ForEachId([&](NumMwmId id) {
    string const & country = numMwmIds.GetFile(id).GetName();
    double const mwmMaxSpeed =
        vehicleModelFactory.GetVehicleModelForCountry(country)->GetMaxWeightSpeed();
    maxSpeed = max(maxSpeed, mwmMaxSpeed);
  });
  CHECK_GREATER(maxSpeed, 0.0, ("Most likely |numMwmIds| is empty."));
  return maxSpeed;
}

double CalcOffroadSpeed(VehicleModelFactoryInterface const & vehicleModelFactory)
{
  return vehicleModelFactory.GetVehicleModel()->GetOffroadSpeed();
}

shared_ptr<VehicleModelFactoryInterface> CreateVehicleModelFactory(
    VehicleType vehicleType, CountryParentNameGetterFn const & countryParentNameGetterFn)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit:
    return make_shared<PedestrianModelFactory>(countryParentNameGetterFn);
  case VehicleType::Bicycle: return make_shared<BicycleModelFactory>(countryParentNameGetterFn);
  case VehicleType::Car: return make_shared<CarModelFactory>(countryParentNameGetterFn);
  case VehicleType::Count:
    CHECK(false, ("Can't create VehicleModelFactoryInterface for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

unique_ptr<IDirectionsEngine> CreateDirectionsEngine(VehicleType vehicleType,
                                                     shared_ptr<NumMwmIds> numMwmIds,
                                                     DataSource & dataSource)
{
  switch (vehicleType)
  {
  case VehicleType::Pedestrian:
  case VehicleType::Transit: return make_unique<PedestrianDirectionsEngine>(numMwmIds);
  case VehicleType::Bicycle:
  // @TODO Bicycle turn generation engine is used now. It's ok for the time being.
  // But later a special car turn generation engine should be implemented.
  case VehicleType::Car: return make_unique<BicycleDirectionsEngine>(dataSource, numMwmIds);
  case VehicleType::Count:
    CHECK(false, ("Can't create DirectionsEngine for", vehicleType));
    return nullptr;
  }
  UNREACHABLE();
}

shared_ptr<TrafficStash> CreateTrafficStash(VehicleType vehicleType, shared_ptr<NumMwmIds> numMwmIds,
                                            traffic::TrafficCache const & trafficCache)
{
  if (vehicleType != VehicleType::Car)
    return nullptr;

  return make_shared<TrafficStash>(trafficCache, numMwmIds);
}

bool IsDeadEnd(Segment const & segment, bool isOutgoing, WorldGraph & worldGraph)
{
  size_t constexpr kDeadEndTestLimit = 50;

  auto const getVertexByEdgeFn = [](SegmentEdge const & edge) {
    return edge.GetTarget();
  };

  // Note. If |isOutgoing| == true outgoing edges are looked for.
  // If |isOutgoing| == false it's the finish. So ingoing edges are looked for.
  auto const getOutgoingEdgesFn = [isOutgoing](WorldGraph & graph, Segment const & u,
                                               vector<SegmentEdge> & edges) {
    graph.GetEdgeList(u, isOutgoing, edges);
  };

  return !CheckGraphConnectivity(segment, kDeadEndTestLimit, worldGraph,
                                 getVertexByEdgeFn, getOutgoingEdgesFn);
}

/// \returns true if the mwm is ready for index graph routing and cross mwm index graph routing.
/// It means the mwm contains routing and cross_mwm sections. In terms of production mwms
/// the method returns false for mwms 170328 and earlier, and returns true for mwms 170428 and
/// later.
bool MwmHasRoutingData(version::MwmTraits const & traits)
{
  return traits.HasRoutingIndex() && traits.HasCrossMwmSection();
}

void GetOutdatedMwms(DataSource & dataSource, vector<string> & outdatedMwms)
{
  outdatedMwms.clear();
  vector<shared_ptr<MwmInfo>> infos;
  dataSource.GetMwmsInfo(infos);

  for (auto const & info : infos)
  {
    if (info->GetType() != MwmInfo::COUNTRY)
      continue;

    if (!MwmHasRoutingData(version::MwmTraits(info->m_version)))
      outdatedMwms.push_back(info->GetCountryName());
  }
}

void PushPassedSubroutes(Checkpoints const & checkpoints, vector<Route::SubrouteAttrs> & subroutes)
{
  for (size_t i = 0; i < checkpoints.GetPassedIdx(); ++i)
  {
    subroutes.emplace_back(Junction(checkpoints.GetPoint(i), feature::kDefaultAltitudeMeters),
                           Junction(checkpoints.GetPoint(i + 1), feature::kDefaultAltitudeMeters),
                           0 /* beginSegmentIdx */, 0 /* endSegmentIdx */);
  }
}

bool GetLastRealOrPart(IndexGraphStarter const & starter, vector<Segment> const & route,
                       Segment & real)
{
  for (auto rit = route.rbegin(); rit != route.rend(); ++rit)
  {
    real = *rit;
    if (starter.ConvertToReal(real))
      return true;
  }
  return false;
}
}  // namespace

namespace routing
{
// IndexRouter::BestEdgeComparator ----------------------------------------------------------------
IndexRouter::BestEdgeComparator::BestEdgeComparator(m2::PointD const & point, m2::PointD const & direction)
  : m_point(point), m_direction(direction)
{
}

int IndexRouter::BestEdgeComparator::Compare(Edge const & edge1, Edge const & edge2) const
{
  if (IsDirectionValid())
  {
    bool const isEdge1Codirectional = IsAlmostCodirectional(edge1);
    if (isEdge1Codirectional != IsAlmostCodirectional(edge2))
      return isEdge1Codirectional ? -1 : 1;
  }

  double const squaredDistFromEdge1 = GetSquaredDist(edge1);
  double const squaredDistFromEdge2 = GetSquaredDist(edge2);
  if (squaredDistFromEdge1 == squaredDistFromEdge2)
    return 0;

  return squaredDistFromEdge1 < squaredDistFromEdge2 ? -1 : 1;
}

bool IndexRouter::BestEdgeComparator::IsAlmostCodirectional(Edge const & edge) const
{
  CHECK(IsDirectionValid(), ());

  auto const edgeDirection = edge.GetDirection();
  if (edgeDirection.IsAlmostZero())
    return false;

  double const cosAng = m2::DotProduct(m_direction.Normalize(), edgeDirection.Normalize());
  // Note. acos(0.97) ≈ 14 degrees.
  double constexpr kMinCosAngForAlmostCodirectionalVectors = 0.97;
  return cosAng >= kMinCosAngForAlmostCodirectionalVectors;
}

double IndexRouter::BestEdgeComparator::GetSquaredDist(Edge const & edge) const
{
  m2::ParametrizedSegment<m2::PointD> const segment(edge.GetStartJunction().GetPoint(),
                                                    edge.GetEndJunction().GetPoint());
  return segment.SquaredDistanceToPoint(m_point);
}

// IndexRouter ------------------------------------------------------------------------------------
IndexRouter::IndexRouter(VehicleType vehicleType, bool loadAltitudes,
                         CountryParentNameGetterFn const & countryParentNameGetterFn,
                         TCountryFileFn const & countryFileFn, CourntryRectFn const & countryRectFn,
                         shared_ptr<NumMwmIds> numMwmIds, unique_ptr<m4::Tree<NumMwmId>> numMwmTree,
                         traffic::TrafficCache const & trafficCache, DataSource & dataSource)
  : m_vehicleType(vehicleType)
  , m_loadAltitudes(loadAltitudes)
  , m_name("astar-bidirectional-" + ToString(m_vehicleType))
  , m_dataSource(dataSource)
  , m_vehicleModelFactory(CreateVehicleModelFactory(m_vehicleType, countryParentNameGetterFn))
  , m_countryFileFn(countryFileFn)
  , m_countryRectFn(countryRectFn)
  , m_numMwmIds(move(numMwmIds))
  , m_numMwmTree(move(numMwmTree))
  , m_trafficStash(CreateTrafficStash(m_vehicleType, m_numMwmIds, trafficCache))
  , m_roadGraph(m_dataSource,
                vehicleType == VehicleType::Pedestrian || vehicleType == VehicleType::Transit
                    ? IRoadGraph::Mode::IgnoreOnewayTag
                    : IRoadGraph::Mode::ObeyOnewayTag,
                m_vehicleModelFactory)
  , m_estimator(EdgeEstimator::Create(
        m_vehicleType, CalcMaxSpeed(*m_numMwmIds, *m_vehicleModelFactory, m_vehicleType),
        CalcOffroadSpeed(*m_vehicleModelFactory), m_trafficStash))
  , m_directionsEngine(CreateDirectionsEngine(m_vehicleType, m_numMwmIds, m_dataSource))
{
  CHECK(!m_name.empty(), ());
  CHECK(m_numMwmIds, ());
  CHECK(m_numMwmTree, ());
  CHECK(m_vehicleModelFactory, ());
  CHECK(m_estimator, ());
  CHECK(m_directionsEngine, ());
}

unique_ptr<WorldGraph> IndexRouter::MakeSingleMwmWorldGraph()
{
  auto worldGraph = MakeWorldGraph();
  worldGraph->SetMode(WorldGraphMode::SingleMwm);
  return worldGraph;
}

bool IndexRouter::FindBestSegment(m2::PointD const & point, m2::PointD const & direction,
                                  bool isOutgoing, WorldGraph & worldGraph,
                                  Segment & bestSegment)
{
  bool dummy;
  return FindBestSegment(point, direction, isOutgoing, worldGraph, bestSegment,
                         dummy /* best segment is almost codirectional */);
}

void IndexRouter::ClearState()
{
  m_roadGraph.ClearState();
  m_directionsEngine->Clear();
}

RouterResultCode IndexRouter::CalculateRoute(Checkpoints const & checkpoints,
                                             m2::PointD const & startDirection,
                                             bool adjustToPrevRoute,
                                             RouterDelegate const & delegate, Route & route)
{
  vector<string> outdatedMwms;
  GetOutdatedMwms(m_dataSource, outdatedMwms);

  if (!outdatedMwms.empty())
  {
    for (string const & mwm : outdatedMwms)
      route.AddAbsentCountry(mwm);

    return RouterResultCode::FileTooOld;
  }

  auto const & startPoint = checkpoints.GetStart();
  auto const & finalPoint = checkpoints.GetFinish();

  try
  {
    SCOPE_GUARD(featureRoadGraphClear, [this]{
      this->ClearState();
    });

    if (adjustToPrevRoute && m_lastRoute && m_lastFakeEdges &&
        finalPoint == m_lastRoute->GetFinish())
    {
      double const distanceToRoute = m_lastRoute->CalcDistance(startPoint);
      double const distanceToFinish = MercatorBounds::DistanceOnEarth(startPoint, finalPoint);
      if (distanceToRoute <= kAdjustRangeM && distanceToFinish >= kMinDistanceToFinishM)
      {
        auto const code = AdjustRoute(checkpoints, startDirection, delegate, route);
        if (code != RouterResultCode::RouteNotFound)
          return code;

        LOG(LWARNING, ("Can't adjust route, do full rebuild, prev start:",
          MercatorBounds::ToLatLon(m_lastRoute->GetStart()), ", start:",
          MercatorBounds::ToLatLon(startPoint), ", finish:",
          MercatorBounds::ToLatLon(finalPoint)));
      }
    }

    return DoCalculateRoute(checkpoints, startDirection, delegate, route);
  }
  catch (RootException const & e)
  {
    LOG(LERROR, ("Can't find path from", MercatorBounds::ToLatLon(startPoint), "to",
      MercatorBounds::ToLatLon(finalPoint), ":\n ", e.what()));
    return RouterResultCode::InternalError;
  }
}

RouterResultCode IndexRouter::DoCalculateRoute(Checkpoints const & checkpoints,
                                               m2::PointD const & startDirection,
                                               RouterDelegate const & delegate, Route & route)
{
  m_lastRoute.reset();

  for (auto const & checkpoint : checkpoints.GetPoints())
  {
    string const countryName = m_countryFileFn(checkpoint);
    if (countryName.empty())
    {
      LOG(LWARNING, ("For point", MercatorBounds::ToLatLon(checkpoint),
                   "CountryInfoGetter returns an empty CountryFile(). It happens when checkpoint"
                   "is put at gaps between mwm."));
      return RouterResultCode::InternalError;
    }

    auto const country = platform::CountryFile(countryName);
    if (!m_dataSource.IsLoaded(country))
      route.AddAbsentCountry(country.GetName());
  }

  if (!route.GetAbsentCountries().empty())
    return RouterResultCode::NeedMoreMaps;

  TrafficStash::Guard guard(m_trafficStash);
  auto graph = MakeWorldGraph();

  vector<Segment> segments;

  Segment startSegment;
  bool startSegmentIsAlmostCodirectionalDirection = false;
  if (!FindBestSegment(checkpoints.GetPointFrom(), startDirection, true /* isOutgoing */, *graph,
                       startSegment, startSegmentIsAlmostCodirectionalDirection))
  {
    return RouterResultCode::StartPointNotFound;
  }

  size_t subrouteSegmentsBegin = 0;
  vector<Route::SubrouteAttrs> subroutes;
  PushPassedSubroutes(checkpoints, subroutes);
  unique_ptr<IndexGraphStarter> starter;
  AStarProgress progress;
  double const checkpointsLength = checkpoints.GetSummaryLengthBetweenPointsMeters();

  for (size_t i = checkpoints.GetPassedIdx(); i < checkpoints.GetNumSubroutes(); ++i)
  {
    bool const isFirstSubroute = i == checkpoints.GetPassedIdx();
    bool const isLastSubroute = i == checkpoints.GetNumSubroutes() - 1;
    auto const & startCheckpoint = checkpoints.GetPoint(i);
    auto const & finishCheckpoint = checkpoints.GetPoint(i + 1);

    Segment finishSegment;
    bool dummy = false;
    if (!FindBestSegment(finishCheckpoint, m2::PointD::Zero() /* direction */,
                         false /* isOutgoing */, *graph, finishSegment,
                         dummy /* bestSegmentIsAlmostCodirectional */))
    {
      return isLastSubroute ? RouterResultCode::EndPointNotFound
                            : RouterResultCode::IntermediatePointNotFound;
    }

    bool isStartSegmentStrictForward = (m_vehicleType == VehicleType::Car);
    if (isFirstSubroute)
      isStartSegmentStrictForward = startSegmentIsAlmostCodirectionalDirection;

    IndexGraphStarter subrouteStarter(MakeFakeEnding(startSegment, startCheckpoint, *graph),
                                      MakeFakeEnding(finishSegment, finishCheckpoint, *graph),
                                      starter ? starter->GetNumFakeSegments() : 0,
                                      isStartSegmentStrictForward, *graph);

    vector<Segment> subroute;
    static double constexpr kEpsAlmostZero = 1e-7;
    double const contributionCoef =
        !base::AlmostEqualAbs(checkpointsLength, 0.0, kEpsAlmostZero) ?
          MercatorBounds::DistanceOnEarth(startCheckpoint, finishCheckpoint) / checkpointsLength :
          kEpsAlmostZero;

    AStarSubProgress subProgress(startCheckpoint, finishCheckpoint, contributionCoef);
    progress.AppendSubProgress(subProgress);
    SCOPE_GUARD(eraseProgress, [&progress]() {
      progress.PushAndDropLastSubProgress();
    });

    auto const result = CalculateSubroute(checkpoints, i, delegate, progress, subrouteStarter,
                                          subroute);

    if (result != RouterResultCode::NoError)
      return result;

    IndexGraphStarter::CheckValidRoute(subroute);

    segments.insert(segments.end(), subroute.begin(), subroute.end());

    size_t subrouteSegmentsEnd = segments.size();
    subroutes.emplace_back(subrouteStarter.GetStartJunction(), subrouteStarter.GetFinishJunction(),
                           subrouteSegmentsBegin, subrouteSegmentsEnd);
    subrouteSegmentsBegin = subrouteSegmentsEnd;
    bool const hasRealOrPart = GetLastRealOrPart(subrouteStarter, subroute, startSegment);
    CHECK(hasRealOrPart, ("No real or part of real segments in route."));
    if (!starter)
      starter = make_unique<IndexGraphStarter>(move(subrouteStarter));
    else
      starter->Append(FakeEdgesContainer(move(subrouteStarter)));
  }

  route.SetCurrentSubrouteIdx(checkpoints.GetPassedIdx());
  route.SetSubroteAttrs(move(subroutes));

  IndexGraphStarter::CheckValidRoute(segments);

  // TODO (@gmoryes) https://jira.mail.ru/browse/MAPSME-10694
  //  We should do RedressRoute for each subroute separately.
  auto redressResult = RedressRoute(segments, delegate, *starter, route);
  if (redressResult != RouterResultCode::NoError)
    return redressResult;

  LOG(LINFO, ("Route length:", route.GetTotalDistanceMeters(), "meters. ETA:",
      route.GetTotalTimeSec(), "seconds."));

  m_lastRoute = make_unique<SegmentedRoute>(checkpoints.GetStart(), checkpoints.GetFinish(),
                                            route.GetSubroutes());
  for (Segment const & segment : segments)
    m_lastRoute->AddStep(segment, starter->GetPoint(segment, true /* front */));

  m_lastFakeEdges = make_unique<FakeEdgesContainer>(move(*starter));

  return RouterResultCode::NoError;
}

vector<Segment> ProcessJoints(vector<JointSegment> const & jointsPath,
                              IndexGraphStarterJoints<IndexGraphStarter> & jointStarter)
{
  CHECK(!jointsPath.empty(), ());

  vector<Segment> path;
  for (auto const & joint : jointsPath)
  {
    vector<Segment> jointPath = jointStarter.ReconstructJoint(joint);
    if (jointPath.empty())
      continue;

    if (path.empty())
    {
      path = move(jointPath);
      continue;
    }

    path.insert(path.end(),
                path.back() == jointPath.front() ? jointPath.begin() + 1 : jointPath.begin(),
                jointPath.end());
  }

  return path;
}

RouterResultCode IndexRouter::CalculateSubroute(Checkpoints const & checkpoints,
                                                size_t subrouteIdx,
                                                RouterDelegate const & delegate,
                                                AStarProgress & progress,
                                                IndexGraphStarter & starter,
                                                vector<Segment> & subroute)
{
  subroute.clear();

  // We use leaps for cars only. Other vehicle types do not have weights in their cross-mwm sections.
  switch (m_vehicleType)
  {
    case VehicleType::Pedestrian:
    case VehicleType::Bicycle:
      starter.GetGraph().SetMode(WorldGraphMode::Joints);
      break;
    case VehicleType::Transit:
      starter.GetGraph().SetMode(WorldGraphMode::NoLeaps);
      break;
    case VehicleType::Car:
      starter.GetGraph().SetMode(AreMwmsNear(starter.GetMwms()) ? WorldGraphMode::Joints
                                                                : WorldGraphMode::LeapsOnly);
      break;
    case VehicleType::Count:
      CHECK(false, ("Unknown vehicle type:", m_vehicleType));
      break;
  }

  LOG(LINFO, ("Routing in mode:", starter.GetGraph().GetMode()));

  auto checkLength = [&starter](RouteWeight const & weight) { return starter.CheckLength(weight); };

  base::HighResTimer timer;
  WorldGraphMode const mode = starter.GetGraph().GetMode();
  if (mode == WorldGraphMode::Joints)
  {
    IndexGraphStarterJoints<IndexGraphStarter> jointStarter(starter, starter.GetStartSegment(), starter.GetFinishSegment());
    RoutingResult<JointSegment, RouteWeight> routingResult;

    uint32_t visitCount = 0;
    double lastValue = progress.GetLastPercent();
    auto const onVisitJunctionJoints = [&](JointSegment const & from, JointSegment const & to)
    {
      ++visitCount;
      if (visitCount % kVisitPeriod != 0)
        return;

      m2::PointD const & pointFrom = jointStarter.GetPoint(from, true /* front */);
      m2::PointD const & pointTo = jointStarter.GetPoint(to, true /* front */);
      auto const newValue = progress.UpdateProgress(pointFrom, pointTo);
      if (newValue - lastValue > kProgressInterval)
      {
        lastValue = newValue;
        delegate.OnProgress(newValue);
      }

      delegate.OnPointCheck(pointFrom);
    };

    using Vertex = IndexGraphStarterJoints<IndexGraphStarter>::Vertex;
    using Edge = IndexGraphStarterJoints<IndexGraphStarter>::Edge;
    using Weight = IndexGraphStarterJoints<IndexGraphStarter>::Weight;

    AStarAlgorithm<Vertex, Edge, Weight>::Params params(
      jointStarter, jointStarter.GetStartJoint(), jointStarter.GetFinishJoint(), nullptr /* prevRoute */,
      delegate, onVisitJunctionJoints, checkLength);

    set<NumMwmId> const mwmIds = starter.GetMwms();
    RouterResultCode const result = FindPath<Vertex, Edge, Weight>(params, mwmIds, routingResult, mode);
    if (result != RouterResultCode::NoError)
      return result;

    subroute = ProcessJoints(routingResult.m_path, jointStarter);
  }
  else
  {
    using Vertex = IndexGraphStarter::Vertex;
    using Edge = IndexGraphStarter::Edge;
    using Weight = IndexGraphStarter::Weight;

    uint32_t visitCount = 0;
    auto lastValue = progress.GetLastPercent();
    if (mode == WorldGraphMode::LeapsOnly)
    {
      AStarSubProgress leapsProgress(checkpoints.GetPoint(subrouteIdx),
                                     checkpoints.GetPoint(subrouteIdx + 1),
                                     kLeapsStageContribution);
      progress.AppendSubProgress(leapsProgress);
    }

    auto eachCount = mode == WorldGraphMode::LeapsOnly ? kVisitPeriodForLeaps : kVisitPeriod;
    auto onVisitJunction = [&](Segment const & from, Segment const & to) {
      if (++visitCount % eachCount != 0)
        return;

      m2::PointD const & pointFrom = starter.GetPoint(from, true /* front */);
      m2::PointD const & pointTo = starter.GetPoint(to, true /* front */);
      auto const newValue = progress.UpdateProgress(pointFrom, pointTo);
      if (newValue - lastValue > kProgressInterval)
      {
        lastValue = newValue;
        delegate.OnProgress(newValue);
      }

      delegate.OnPointCheck(pointFrom);
    };

    RoutingResult<Segment, RouteWeight> routingResult;
    AStarAlgorithm<Vertex, Edge, Weight>::Params params(
      starter, starter.GetStartSegment(), starter.GetFinishSegment(), nullptr /* prevRoute */,
      delegate, onVisitJunction, checkLength);

    set<NumMwmId> const mwmIds = starter.GetMwms();
    RouterResultCode const result = FindPath<Vertex, Edge, Weight>(params, mwmIds, routingResult, mode);

    if (mode == WorldGraphMode::LeapsOnly)
      progress.PushAndDropLastSubProgress();

    if (result != RouterResultCode::NoError)
      return result;

    vector<Segment> subrouteWithoutPostprocessing;
    RouterResultCode const leapsResult = ProcessLeapsJoints(routingResult.m_path, delegate, 
                                                            starter.GetGraph().GetMode(),
                                                            starter, progress,
                                                            subrouteWithoutPostprocessing);

    if (leapsResult != RouterResultCode::NoError)
      return leapsResult;

    if (mode == WorldGraphMode::LeapsOnly)
    {
      LeapsPostProcessor leapsPostProcessor(subrouteWithoutPostprocessing, starter);
      subroute = leapsPostProcessor.GetProcessedPath();
    }
    else
    {
      subroute = move(subrouteWithoutPostprocessing);
    }
  }

  LOG(LINFO, ("Time for routing in mode:", starter.GetGraph().GetMode(), "is",
              timer.ElapsedNano() / 1e6, "ms"));

  return RouterResultCode::NoError;
}

RouterResultCode IndexRouter::AdjustRoute(Checkpoints const & checkpoints,
                                          m2::PointD const & startDirection,
                                          RouterDelegate const & delegate, Route & route)
{
  base::Timer timer;
  TrafficStash::Guard guard(m_trafficStash);
  auto graph = MakeWorldGraph();
  graph->SetMode(WorldGraphMode::NoLeaps);

  Segment startSegment;
  m2::PointD const & pointFrom = checkpoints.GetPointFrom();
  bool bestSegmentIsAlmostCodirectional = false;
  if (!FindBestSegment(pointFrom, startDirection, true /* isOutgoing */, *graph, startSegment,
                       bestSegmentIsAlmostCodirectional))
  {
    return RouterResultCode::StartPointNotFound;
  }

  auto const & lastSubroutes = m_lastRoute->GetSubroutes();
  CHECK(!lastSubroutes.empty(), ());
  auto const & lastSubroute = m_lastRoute->GetSubroute(checkpoints.GetPassedIdx());

  auto const & steps = m_lastRoute->GetSteps();
  CHECK(!steps.empty(), ());

  FakeEnding dummy;
  IndexGraphStarter starter(MakeFakeEnding(startSegment, pointFrom, *graph), dummy,
                            m_lastFakeEdges->GetNumFakeEdges(), bestSegmentIsAlmostCodirectional,
                            *graph);

  starter.Append(*m_lastFakeEdges);

  vector<SegmentEdge> prevEdges;
  CHECK_LESS_OR_EQUAL(lastSubroute.GetEndSegmentIdx(), steps.size(), ());
  for (size_t i = lastSubroute.GetBeginSegmentIdx(); i < lastSubroute.GetEndSegmentIdx(); ++i)
  {
    auto const & step = steps[i];
    prevEdges.emplace_back(step.GetSegment(), starter.CalcSegmentWeight(step.GetSegment()));
  }

  uint32_t visitCount = 0;

  auto onVisitJunction = [&](Segment const & /* start */, Segment const & vertex) {
    if (visitCount++ % kVisitPeriod != 0)
      return;

    m2::PointD const & point = starter.GetPoint(vertex, true /* front */);
    delegate.OnPointCheck(point);
  };

  auto const checkLength = [&starter](RouteWeight const & weight) {
    return weight <= RouteWeight(kAdjustLimitSec) && starter.CheckLength(weight);
  };

  using Vertex = IndexGraphStarter::Vertex;
  using Edge = IndexGraphStarter::Edge;
  using Weight = IndexGraphStarter::Weight;

  AStarAlgorithm<Vertex, Edge, Weight> algorithm;
  AStarAlgorithm<Vertex, Edge, Weight>::Params params(starter, starter.GetStartSegment(),
                                                      {} /* finalVertex */, &prevEdges, delegate,
                                                      onVisitJunction, checkLength);
  RoutingResult<Segment, RouteWeight> result;
  auto const resultCode =
      ConvertResult<Vertex, Edge, Weight>(algorithm.AdjustRoute(params, result));
  if (resultCode != RouterResultCode::NoError)
    return resultCode;

  CHECK_GREATER_OR_EQUAL(result.m_path.size(), 2, ());
  CHECK(IndexGraphStarter::IsFakeSegment(result.m_path.front()), ());
  CHECK(IndexGraphStarter::IsFakeSegment(result.m_path.back()), ());

  vector<Route::SubrouteAttrs> subroutes;
  PushPassedSubroutes(checkpoints, subroutes);

  size_t subrouteOffset = result.m_path.size();
  subroutes.emplace_back(starter.GetStartJunction(), starter.GetFinishJunction(),
                         0 /* beginSegmentIdx */, subrouteOffset);

  for (size_t i = checkpoints.GetPassedIdx() + 1; i < lastSubroutes.size(); ++i)
  {
    auto const & subroute = lastSubroutes[i];

    for (size_t j = subroute.GetBeginSegmentIdx(); j < subroute.GetEndSegmentIdx(); ++j)
      result.m_path.push_back(steps[j].GetSegment());

    subroutes.emplace_back(subroute, subrouteOffset);
    subrouteOffset = subroutes.back().GetEndSegmentIdx();
  }

  CHECK_EQUAL(result.m_path.size(), subrouteOffset, ());

  route.SetCurrentSubrouteIdx(checkpoints.GetPassedIdx());
  route.SetSubroteAttrs(move(subroutes));

  auto const redressResult = RedressRoute(result.m_path, delegate, starter, route);
  if (redressResult != RouterResultCode::NoError)
    return redressResult;

  LOG(LINFO, ("Adjust route, elapsed:", timer.ElapsedSeconds(), ", prev start:", checkpoints,
              ", prev route:", steps.size(), ", new route:", result.m_path.size()));

  return RouterResultCode::NoError;
}

unique_ptr<WorldGraph> IndexRouter::MakeWorldGraph()
{
  RoutingOptions routingOptions;
  if (m_vehicleType == VehicleType::Car)
  {
    routingOptions = RoutingOptions::LoadCarOptionsFromSettings();
    LOG(LINFO, ("Avoid next roads:", routingOptions));
  }

  auto crossMwmGraph = make_unique<CrossMwmGraph>(
      m_numMwmIds, m_numMwmTree, m_vehicleModelFactory,
      m_vehicleType == VehicleType::Transit ? VehicleType::Pedestrian : m_vehicleType,
      m_countryRectFn, m_dataSource);

  auto indexGraphLoader = IndexGraphLoader::Create(
      m_vehicleType == VehicleType::Transit ? VehicleType::Pedestrian : m_vehicleType,
      m_loadAltitudes, m_numMwmIds, m_vehicleModelFactory, m_estimator, m_dataSource,
      routingOptions);

  if (m_vehicleType != VehicleType::Transit)
  {
    auto graph = make_unique<SingleVehicleWorldGraph>(move(crossMwmGraph), move(indexGraphLoader),
                                                      m_estimator);
    graph->SetRoutingOptions(routingOptions);
    return graph;
  }

  auto transitGraphLoader = TransitGraphLoader::Create(m_dataSource, m_numMwmIds, m_estimator);
  return make_unique<TransitWorldGraph>(move(crossMwmGraph), move(indexGraphLoader),
                                        move(transitGraphLoader), m_estimator);
}

bool IndexRouter::FindBestSegment(m2::PointD const & point, m2::PointD const & direction,
                                  bool isOutgoing, WorldGraph & worldGraph, Segment & bestSegment,
                                  bool & bestSegmentIsAlmostCodirectional) const
{
  auto const file = platform::CountryFile(m_countryFileFn(point));
  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(MwmIsNotAliveException, ("Can't get mwm handle for", file));

  auto const mwmId = MwmSet::MwmId(handle.GetInfo());
  NumMwmId const numMwmId = m_numMwmIds->GetId(file);

  vector<pair<Edge, Junction>> candidates;
  m_roadGraph.FindClosestEdges(point, kMaxRoadCandidates, candidates);

  auto const getSegmentByEdge = [&numMwmId](Edge const & edge) {
    return Segment(numMwmId, edge.GetFeatureId().m_index, edge.GetSegId(), edge.IsForward());
  };

  // Getting rid of knowingly bad candidates.
  base::EraseIf(candidates, [&](pair<Edge, Junction> const & p) {
    Edge const & edge = p.first;
    return edge.GetFeatureId().m_mwmId != mwmId ||
           IsDeadEnd(getSegmentByEdge(edge), isOutgoing, worldGraph);
  });

  if (candidates.empty())
    return false;

  BestEdgeComparator bestEdgeComparator(point, direction);
  Edge bestEdge = candidates[0].first;
  for (size_t i = 1; i < candidates.size(); ++i)
  {
    Edge const & edge = candidates[i].first;
    if (bestEdgeComparator.Compare(edge, bestEdge) < 0)
      bestEdge = edge;
  }

  bestSegmentIsAlmostCodirectional =
      bestEdgeComparator.IsDirectionValid() && bestEdgeComparator.IsAlmostCodirectional(bestEdge);
  bestSegment = getSegmentByEdge(bestEdge);
  return true;
}

RouterResultCode IndexRouter::ProcessLeapsJoints(vector<Segment> const & input,
                                                 RouterDelegate const & delegate,
                                                 WorldGraphMode prevMode,
                                                 IndexGraphStarter & starter,
                                                 AStarProgress & progress,
                                                 vector<Segment> & output)
{
  if (prevMode != WorldGraphMode::LeapsOnly)
  {
    output = input;
    return RouterResultCode::NoError;
  }

  SCOPE_GUARD(progressGuard, [&progress]() {
    progress.PushAndDropLastSubProgress();
  });

  progress.AppendSubProgress(AStarSubProgress(1.0 - kLeapsStageContribution));

  CHECK_GREATER_OR_EQUAL(input.size(), 4,
                         ("Route in LeapsOnly mode must have at least start and finish leaps."));

  LOG(LINFO, ("Start process leaps with Joints."));
  WorldGraph & worldGraph = starter.GetGraph();

  // For all leaps except the first leap which connects start to mwm exit in LeapsOnly mode we need
  // to drop first segment of the leap because we already have its twin from the previous mwm.
  bool dropFirstSegment = false;

  // While route building in LeapOnly mode we estimate start to mwm exit distance. This estimation may
  // be incorrect and it causes appearance of unneeded loops: route goes from start to wrongly selected mwm exit,
  // then we have small unneeded leap in other mwm which returns us to start mwm, then we have leap in start mwm
  // to the correct start mwm exit and then we have normal route.
  // |input| mwm ids for such case look like
  // { fake, startId, otherId, otherId, startId, startId, .. pairs of ids for other leaps .. , finishId, fake}.
  // To avoid this behavior we collapse all leaps from start to  last occurrence of startId to one leap and
  // use WorldGraph with NoLeaps mode to proccess these leap. Unlike SingleMwm mode used to process ordinary leaps
  // NoLeaps allows to use all mwms so if we really need to visit other mwm we will.
  auto const firstMwmId = input[1].GetMwmId();
  auto const startLeapEndReverseIt = find_if(input.rbegin() + 2, input.rend(),
                                             [firstMwmId](Segment const & s) { return s.GetMwmId() == firstMwmId; });
  auto const startLeapEndIt = startLeapEndReverseIt.base() - 1;
  auto const startLeapEnd = static_cast<size_t>(distance(input.begin(), startLeapEndIt));

  // The last leap processed the same way. See the comment above.
  auto const lastMwmId = input[input.size() - 2].GetMwmId();
  auto const finishLeapStartIt = find_if(startLeapEndIt, input.end(),
                                         [lastMwmId](Segment const & s) { return s.GetMwmId() == lastMwmId; });
  auto const finishLeapStart = static_cast<size_t>(distance(input.begin(), finishLeapStartIt));

  auto checkLength = [&starter](RouteWeight const & weight) {
    return starter.CheckLength(weight);
  };

  auto fillMwmIds = [&](size_t start, size_t end, set<NumMwmId> & mwmIds)
  {
    CHECK_LESS(start, input.size(), ());
    CHECK_LESS(end, input.size(), ());
    mwmIds.clear();

    if (start == startLeapEnd)
      mwmIds = starter.GetStartMwms();

    if (end == finishLeapStart)
      mwmIds = starter.GetFinishMwms();

    for (size_t i = start; i <= end; ++i)
    {
      if (input[i].GetMwmId() != kFakeNumMwmId)
        mwmIds.insert(input[i].GetMwmId());
    }
  };

  set<NumMwmId> mwmIds;
  IndexGraphStarterJoints<IndexGraphStarter> jointStarter(starter);
  size_t maxStart = 0;

  auto const runAStarAlgorithm = [&](size_t start, size_t end, WorldGraphMode mode,
                                     RoutingResult<JointSegment, RouteWeight> & routingResult)
  {
    // Clear previous loaded graphs to not spend too much memory at one time.
    worldGraph.ClearCachedGraphs();

    // Clear previous info about route.
    routingResult.Clear();
    jointStarter.Reset();

    worldGraph.SetMode(mode);
    jointStarter.Init(input[start], input[end]);

    fillMwmIds(start, end, mwmIds);

    using Vertex = IndexGraphStarterJoints<IndexGraphStarter>::Vertex;
    using Edge = IndexGraphStarterJoints<IndexGraphStarter>::Edge;
    using Weight = IndexGraphStarterJoints<IndexGraphStarter>::Weight;

    maxStart = max(maxStart, start);
    auto const contribCoef = static_cast<double>(end - maxStart + 1) / (input.size());
    auto const startPoint = starter.GetPoint(input[start], true /* front */);
    auto const endPoint = starter.GetPoint(input[end], true /* front */);
    progress.AppendSubProgress({startPoint, endPoint, contribCoef});

    RouterResultCode resultCode = RouterResultCode::NoError;
    SCOPE_GUARD(progressGuard, [&]() {
      if (resultCode == RouterResultCode::NoError)
        progress.PushAndDropLastSubProgress();
      else
        progress.DropLastSubProgress();
    });

    uint32_t visitCount = 0;
    auto lastValue = progress.GetLastPercent();
    auto const onVisitJunctionJoints = [&](JointSegment const & from, JointSegment const & to)
    {
      ++visitCount;
      if (visitCount % kVisitPeriod != 0)
        return;

      m2::PointD const & pointFrom = jointStarter.GetPoint(from, true /* front */);
      m2::PointD const & pointTo = jointStarter.GetPoint(to, true /* front */);
      auto const newValue = progress.UpdateProgress(pointFrom, pointTo);
      if (newValue - lastValue > kProgressInterval)
      {
        lastValue = newValue;
        delegate.OnProgress(newValue);
      }

      delegate.OnPointCheck(pointFrom);
    };

    AStarAlgorithm<Vertex, Edge, Weight>::Params params(
      jointStarter, jointStarter.GetStartJoint(), jointStarter.GetFinishJoint(),
      nullptr /* prevRoute */, delegate,
      onVisitJunctionJoints, checkLength);

    resultCode = FindPath<Vertex, Edge, Weight>(params, mwmIds, routingResult, mode);
    return resultCode;
  };

  deque<vector<Segment>> paths;
  size_t prevStart = numeric_limits<size_t>::max();
  auto const tryBuildRoute = [&](size_t start, size_t end, WorldGraphMode mode,
                                 RoutingResult<JointSegment, RouteWeight> & routingResult)
  {
    RouterResultCode const result = runAStarAlgorithm(start, end, mode, routingResult);
    if (result == RouterResultCode::NoError)
    {
      vector<Segment> subroute = ProcessJoints(routingResult.m_path, jointStarter);

      CHECK(!subroute.empty(), ());
      if (start == prevStart && !paths.empty())
        paths.pop_back();

      ASSERT(!subroute.empty(), ());
      paths.emplace_back(vector<Segment>(dropFirstSegment ? subroute.cbegin() + 1
                                                          : subroute.cbegin(), subroute.cend()));

      dropFirstSegment = true;
      prevStart = start;
      return true;
    }

    LOG(LINFO, ("Can not find path",
      "from:",
      MercatorBounds::ToLatLon(starter.GetPoint(input[start], input[start].IsForward())),
      "to:",
      MercatorBounds::ToLatLon(starter.GetPoint(input[end], input[end].IsForward()))));

    return false;
  };

  size_t lastPrev = 0;
  size_t prev = 0;
  size_t next = 0;
  RoutingResult<JointSegment, RouteWeight> routingResult;

  for (size_t i = startLeapEnd; i <= finishLeapStart; ++i)
  {
    if (i == startLeapEnd)
    {
      prev = 0;
      next = i;
    }
    else if (i == finishLeapStart)
    {
      prev = i;
      next = input.size() - 1;
    }
    else
    {
      prev = i;
      next = i + 1;
    }

    if (!tryBuildRoute(prev, next, WorldGraphMode::JointSingleMwm, routingResult))
    {
      auto const prevPoint = starter.GetPoint(input[next], true);
      // |next + 1| - is the twin of |next|
      // |next + 2| - is the next exit.
      while (next + 2 < finishLeapStart && next != finishLeapStart)
      {
        auto const point = starter.GetPoint(input[next + 2], true);
        double const distBetweenExistsMeters = MercatorBounds::DistanceOnEarth(point, prevPoint);

        static double constexpr kMinDistBetweenExitsM = 100000;  // 100 km
        if (distBetweenExistsMeters > kMinDistBetweenExitsM)
          break;

        LOG(LINFO, ("Exit:", MercatorBounds::ToLatLon(point),
                    "too close(", distBetweenExistsMeters / 1000, "km ), try get next."));
        next += 2;
      }

      if (next + 2 > finishLeapStart || next == finishLeapStart)
        next = input.size() - 1;
      else
        next += 2;

      if (!tryBuildRoute(prev, next, WorldGraphMode::Joints, routingResult))
      {
        // Already in start
        if (prev == 0)
          return RouterResultCode::RouteNotFound;

        prev = lastPrev;
        if (prev == 0)
          dropFirstSegment = false;

        CHECK_GREATER_OR_EQUAL(prev, 0, ());
        if (!tryBuildRoute(prev, next, WorldGraphMode::Joints, routingResult))
          return RouterResultCode::RouteNotFound;
      }
    }

    lastPrev = prev;
    i = next;
  }

  while (!paths.empty())
  {
    using Iterator = vector<Segment>::iterator;
    output.insert(output.end(),
                  move_iterator<Iterator>(paths.front().begin()),
                  move_iterator<Iterator>(paths.front().end()));
    paths.pop_front();
  }

  return RouterResultCode::NoError;
}

RouterResultCode IndexRouter::RedressRoute(vector<Segment> const & segments,
                                           RouterDelegate const & delegate,
                                           IndexGraphStarter & starter, Route & route) const
{
  CHECK(!segments.empty(), ());
  vector<Junction> junctions;
  size_t const numPoints = IndexGraphStarter::GetRouteNumPoints(segments);
  junctions.reserve(numPoints);

  for (size_t i = 0; i < numPoints; ++i)
    junctions.emplace_back(starter.GetRouteJunction(segments, i));

  IndexRoadGraph roadGraph(m_numMwmIds, starter, segments, junctions, m_dataSource);
  starter.GetGraph().SetMode(WorldGraphMode::NoLeaps);

  Route::TTimes times;
  times.reserve(segments.size());
  double time = 0.0;
  times.emplace_back(static_cast<uint32_t>(0), 0.0);

  for (size_t i = 0; i + 1 < numPoints; ++i)
  {
    time += starter.CalcSegmentETA(segments[i]);
    times.emplace_back(static_cast<uint32_t>(i + 1), time);
  }

  CHECK(m_directionsEngine, ());
  ReconstructRoute(*m_directionsEngine, roadGraph, m_trafficStash, delegate, junctions, move(times),
                   route);

  CHECK(m_numMwmIds, ());
  auto & worldGraph = starter.GetGraph();
  for (auto & routeSegment : route.GetRouteSegments())
  {
    routeSegment.SetTransitInfo(worldGraph.GetTransitInfo(routeSegment.GetSegment()));

    // Removing speed cameras from the route with method AreSpeedCamerasProhibited(...)
    // at runtime is necessary for maps from Jan 2019 with speed cameras where it's prohibited
    // to use them.
    if (m_vehicleType == VehicleType::Car)
    {
      auto & segment = routeSegment.GetSegment();
      routeSegment.SetRoadTypes(starter.GetRoutingOptions(segment));

      if (segment.IsRealSegment())
      {
        if (!AreSpeedCamerasProhibited(m_numMwmIds->GetFile(segment.GetMwmId())))
          routeSegment.SetSpeedCameraInfo(worldGraph.GetSpeedCamInfo(segment));
      }
      else
      {
        starter.ConvertToReal(segment);
      }
    }
  }

  vector<platform::CountryFile> speedCamProhibited;
  FillSpeedCamProhibitedMwms(segments, speedCamProhibited);
  route.SetMwmsPartlyProhibitedForSpeedCams(move(speedCamProhibited));

  if (delegate.IsCancelled())
    return RouterResultCode::Cancelled;

  if (!route.IsValid())
  {
    LOG(LERROR, ("RedressRoute failed. Segments:", segments.size()));
    return RouterResultCode::RouteNotFoundRedressRouteError;
  }

  return RouterResultCode::NoError;
}

bool IndexRouter::AreMwmsNear(set<NumMwmId> const & mwmIds) const
{
  for (auto const & outerId : mwmIds)
  {
    m2::RectD const rect = m_countryRectFn(m_numMwmIds->GetFile(outerId).GetName());
    size_t found = 0;
    m_numMwmTree->ForEachInRect(rect, [&](NumMwmId id) { found += mwmIds.count(id); });
    if (found != mwmIds.size())
      return false;
  }
  return true;
}

bool IndexRouter::DoesTransitSectionExist(NumMwmId numMwmId) const
{
  CHECK(m_numMwmIds, ());
  platform::CountryFile const & file = m_numMwmIds->GetFile(numMwmId);

  MwmSet::MwmHandle handle = m_dataSource.GetMwmHandleByCountryFile(file);
  if (!handle.IsAlive())
    MYTHROW(RoutingException, ("Can't get mwm handle for", file));

  MwmValue const & mwmValue = *handle.GetValue<MwmValue>();
  return mwmValue.m_cont.IsExist(TRANSIT_FILE_TAG);
}

RouterResultCode IndexRouter::ConvertTransitResult(set<NumMwmId> const & mwmIds,
                                                   RouterResultCode resultCode) const
{
  if (m_vehicleType != VehicleType::Transit || resultCode != RouterResultCode::RouteNotFound)
    return resultCode;

  for (auto const mwmId : mwmIds)
  {
    CHECK_NOT_EQUAL(mwmId, kFakeNumMwmId, ());
    if (!DoesTransitSectionExist(mwmId))
      return RouterResultCode::TransitRouteNotFoundNoNetwork;
  }

  return RouterResultCode::TransitRouteNotFoundTooLongPedestrian;
}

void IndexRouter::FillSpeedCamProhibitedMwms(vector<Segment> const & segments,
                                             vector<platform::CountryFile> & speedCamProhibitedMwms) const
{
  CHECK(m_numMwmIds, ());

  set<NumMwmId> mwmIds;
  for (auto const & s : segments)
    mwmIds.emplace(s.GetMwmId());

  for (auto const id : mwmIds)
  {
    if (id == kFakeNumMwmId)
      continue;

    auto const & country = m_numMwmIds->GetFile(id);
    if (AreSpeedCamerasPartlyProhibited(country))
      speedCamProhibitedMwms.emplace_back(country);
  }
}
}  // namespace routing
