#pragma once

#include "agus_maps_flutter.h"

#include "map/framework.hpp"
#include "map/routing_mark.hpp"
#include "platform/distance.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/settings.hpp"
#include "routing/following_info.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"
#include "routing/routing_options.hpp"
#include "routing/speed_camera_manager.hpp"

#include "routing/route.hpp"
#include "routing/routing_session.hpp"
#include "routing/turns.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/latlon.hpp"
#include "geometry/mercator.hpp"
#include "geometry/point_with_altitude.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace agus::navigation
{
inline char * CopyString(std::string const & value)
{
  auto const size = value.size();
  auto * out = static_cast<char *>(std::malloc(size + 1));
  if (out == nullptr)
    return nullptr;
  if (size > 0)
    std::memcpy(out, value.data(), size);
  out[size] = '\0';
  return out;
}

inline void FreeString(char const * value)
{
  std::free(const_cast<char *>(value));
}

inline bool IsValidRouterType(int32_t routerType)
{
  return routerType >= 0 &&
         routerType < static_cast<int32_t>(routing::RouterType::Count);
}

inline bool IsValidRouteMarkType(int32_t markType)
{
  return markType >= static_cast<int32_t>(RouteMarkType::Start) &&
         markType <= static_cast<int32_t>(RouteMarkType::Finish);
}

inline measurement_utils::Units UnitsFromInt(int32_t units)
{
  return units == static_cast<int32_t>(measurement_utils::Units::Imperial)
             ? measurement_utils::Units::Imperial
             : measurement_utils::Units::Metric;
}

inline int32_t GetMeasurementUnits()
{
  return static_cast<int32_t>(measurement_utils::GetMeasurementUnits());
}

inline void SetMeasurementUnits(Framework * framework, int32_t unitsValue)
{
  auto const units = UnitsFromInt(unitsValue);
  settings::Set(settings::kMeasurementUnits, units);
  if (framework != nullptr)
    framework->GetRoutingManager().SetTurnNotificationsUnits(units);
}

inline int32_t GetAvoidRoutingOptions()
{
  return static_cast<int32_t>(
      routing::RoutingOptions::LoadOptionsFromSettings(routing::VehicleType::Car).GetOptions());
}

inline void SetAvoidRoutingOptions(int32_t mask)
{
  auto const allowed = static_cast<int32_t>(routing::RoutingOptions::Option::Toll) |
                       static_cast<int32_t>(routing::RoutingOptions::Option::Motorway) |
                       static_cast<int32_t>(routing::RoutingOptions::Option::Ferry) |
                       static_cast<int32_t>(routing::RoutingOptions::Option::Dirty);
  routing::RoutingOptions::SaveOptionsToSettings(
      routing::RoutingOptions(static_cast<routing::RoutingOptions::OptionType>(mask & allowed),
                              routing::VehicleType::Car));
}

inline void EnsureRoutingCallbacks(Framework * framework)
{
  if (framework == nullptr)
    return;
  auto & manager = framework->GetRoutingManager();
  manager.SetRouteBuildingListener(
      [](routing::RouterResultCode, storage::CountriesSet const &) {});
  manager.SetRouteProgressListener([](float) {});
}

inline int32_t SetRouter(Framework * framework, int32_t routerType)
{
  if (framework == nullptr)
    return -1;
  if (!IsValidRouterType(routerType))
    return -2;

  EnsureRoutingCallbacks(framework);
  framework->GetRoutingManager().SetRouter(
      static_cast<routing::RouterType>(routerType));
  return 1;
}

inline int32_t GetRouter(Framework * framework)
{
  if (framework == nullptr)
    return -1;
  return static_cast<int32_t>(framework->GetRoutingManager().GetRouter());
}

inline int32_t AddRoutePoint(Framework * framework, int32_t markType,
                             char const * title, char const * subtitle,
                             double lat, double lon,
                             int32_t intermediateIndex, int32_t isMyPosition,
                             int32_t reorderIntermediatePoints)
{
  if (framework == nullptr)
    return -1;
  if (!IsValidRouteMarkType(markType))
    return -2;

  EnsureRoutingCallbacks(framework);
  RouteMarkData data;
  data.m_title = title == nullptr ? std::string() : std::string(title);
  data.m_subTitle = subtitle == nullptr ? std::string() : std::string(subtitle);
  data.m_pointType = static_cast<RouteMarkType>(markType);
  data.m_intermediateIndex = intermediateIndex < 0
                                 ? 0
                                 : static_cast<size_t>(intermediateIndex);
  data.m_isMyPosition = isMyPosition != 0;
  data.m_position = mercator::FromLatLon(lat, lon);
  framework->GetRoutingManager().AddRoutePoint(
      std::move(data), reorderIntermediatePoints != 0);
  return 1;
}

inline void RemoveRoutePoint(Framework * framework, int32_t markType,
                             int32_t intermediateIndex)
{
  if (framework == nullptr || !IsValidRouteMarkType(markType))
    return;
  framework->GetRoutingManager().RemoveRoutePoint(
      static_cast<RouteMarkType>(markType),
      intermediateIndex < 0 ? 0 : static_cast<size_t>(intermediateIndex));
}

inline void ClearRoutePoints(Framework * framework)
{
  if (framework != nullptr)
    framework->GetRoutingManager().RemoveRoutePoints();
}

inline int32_t BuildRoute(Framework * framework)
{
  if (framework == nullptr)
    return -1;
  EnsureRoutingCallbacks(framework);
  framework->GetRoutingManager().BuildRoute();
  return 1;
}

inline int32_t FollowRoute(Framework * framework)
{
  if (framework == nullptr)
    return -1;
  EnsureRoutingCallbacks(framework);
  framework->GetRoutingManager().FollowRoute();
  return framework->GetRoutingManager().IsRoutingFollowing() ? 1 : 0;
}

inline void CloseRoute(Framework * framework, int32_t removeRoutePoints)
{
  if (framework != nullptr)
    framework->GetRoutingManager().CloseRouting(removeRoutePoints != 0);
}

inline int32_t IsActive(Framework * framework)
{
  return framework != nullptr && framework->GetRoutingManager().IsRoutingActive();
}

inline int32_t IsBuilt(Framework * framework)
{
  return framework != nullptr && framework->GetRoutingManager().IsRouteBuilt();
}

inline int32_t IsBuilding(Framework * framework)
{
  return framework != nullptr && framework->GetRoutingManager().IsRouteBuilding();
}

inline int32_t IsFollowing(Framework * framework)
{
  return framework != nullptr && framework->GetRoutingManager().IsRoutingFollowing();
}

inline void FillDistance(platform::Distance const & distance, double & value,
                         int32_t & units)
{
  value = distance.GetDistance();
  units = static_cast<int32_t>(distance.GetUnits());
}

inline AgusNavigationStatus * CopyStatus(Framework * framework)
{
  auto * status = static_cast<AgusNavigationStatus *>(
      std::calloc(1, sizeof(AgusNavigationStatus)));
  if (status == nullptr)
    return nullptr;

  status->router_type = framework == nullptr
                            ? -1
                            : static_cast<int32_t>(
                                  framework->GetRoutingManager().GetRouter());
  status->route_session_state =
      static_cast<int32_t>(routing::SessionState::NoValidRoute);
  status->turn = 0;
  status->next_turn = 0;
  status->pedestrian_turn = 0;
  status->speed_limit_mps = -1.0;

  if (framework == nullptr)
    return status;

  auto & manager = framework->GetRoutingManager();
  status->is_active = manager.IsRoutingActive() ? 1 : 0;
  status->is_built = manager.IsRouteBuilt() ? 1 : 0;
  status->is_building = manager.IsRouteBuilding() ? 1 : 0;
  status->is_following = manager.IsRoutingFollowing() ? 1 : 0;
  status->is_valid = manager.IsRouteValid() ? 1 : 0;

  if (!manager.IsRoutingActive())
    return status;

  routing::FollowingInfo info;
  manager.GetRouteFollowingInfo(info);
  status->route_session_state = static_cast<int32_t>(info.m_routingSessionState);
  if (!info.IsValid())
    return status;

  status->has_following_info = 1;
  status->turn = static_cast<int32_t>(info.m_turn);
  status->next_turn = static_cast<int32_t>(info.m_nextTurn);
  status->pedestrian_turn = static_cast<int32_t>(info.m_pedestrianTurn);
  status->exit_number = static_cast<int32_t>(info.m_exitNum);
  status->total_time_seconds = info.m_time;
  status->completion_percent = info.m_completionPercent;
  status->speed_limit_mps = info.m_speedLimitMps;
  status->time_to_next_stop_seconds = info.m_timeToNextStop;
  status->index_of_next_stop = info.m_indexOfNextStop;
  FillDistance(info.m_distToTarget, status->distance_to_target,
               status->distance_to_target_units);
  FillDistance(info.m_distToTurn, status->distance_to_turn,
               status->distance_to_turn_units);
  FillDistance(info.m_distToNextStop, status->distance_to_next_stop,
               status->distance_to_next_stop_units);
  status->current_street = CopyString(info.m_currentStreetName);
  status->next_street = CopyString(info.m_nextStreetName);
  status->next_next_street = CopyString(info.m_nextNextStreetName);
  return status;
}

inline void FreeStatus(AgusNavigationStatus * status)
{
  if (status == nullptr)
    return;
  FreeString(status->current_street);
  FreeString(status->next_street);
  FreeString(status->next_next_street);
  std::free(status);
}

// The route of the session while it can be followed, or null. Route
// methods such as GetNextTurns CHECK-fail on a route that is not valid.
inline std::shared_ptr<routing::Route> NavigableRoute(Framework * framework)
{
  if (framework == nullptr)
    return nullptr;
  auto & manager = framework->GetRoutingManager();
  if (!manager.IsRoutingActive())
    return nullptr;
  auto const & session = manager.RoutingSession();
  if (!session.IsNavigable())
    return nullptr;
  auto route = session.GetRouteForTests();
  if (!route || !route->IsValid())
    return nullptr;
  return route;
}

inline int32_t GetNextTurns(Framework * framework, AgusNavigationTurns * out)
{
  if (out == nullptr)
    return -1;
  *out = AgusNavigationTurns{};
  out->count = -1;

  auto const route = NavigableRoute(framework);
  if (!route)
    return -1;

  std::vector<routing::turns::TurnItemDist> turns;
  route->GetNextTurns(turns);
  auto const count = std::min<size_t>(turns.size(), AGUS_NAVIGATION_MAX_TURNS);
  for (size_t i = 0; i < count; ++i)
  {
    out->turns[i].pedestrian_direction =
        static_cast<int32_t>(turns[i].m_turnItem.m_pedestrianTurn);
    out->turns[i].distance_meters = turns[i].m_distMeters;
  }
  out->count = static_cast<int32_t>(count);
  return out->count;
}

// Smaller steps would allocate millions of points for a city route.
inline constexpr double kMinRoutePointStepMeters = 1.0;

inline AgusRoutePoints * CopyRoutePoints(Framework * framework, double maxStepMeters)
{
  if (!NavigableRoute(framework))
    return nullptr;

  std::vector<geometry::PointWithAltitude> junctions;
  if (!framework->GetRoutingManager().RoutingSession().GetRouteJunctionPoints(junctions) ||
      junctions.empty())
    return nullptr;

  bool const interpolate = maxStepMeters > 0.0;
  double const step = std::max(maxStepMeters, kMinRoutePointStepMeters);
  std::vector<ms::LatLon> points;
  points.reserve(junctions.size());
  points.push_back(junctions[0].ToLatLon());
  for (size_t i = 1; i < junctions.size(); ++i)
  {
    if (interpolate)
    {
      double const dist =
          ms::DistanceOnEarth(junctions[i - 1].ToLatLon(), junctions[i].ToLatLon());
      if (dist > step)
      {
        int const steps = static_cast<int>(dist / step) + 1;
        for (int s = 1; s < steps; ++s)
        {
          points.push_back(junctions[i - 1]
                               .Interpolate(junctions[i], static_cast<double>(s) / steps)
                               .ToLatLon());
        }
      }
    }
    points.push_back(junctions[i].ToLatLon());
  }

  auto * out = static_cast<AgusRoutePoints *>(std::calloc(1, sizeof(AgusRoutePoints)));
  if (out == nullptr)
    return nullptr;
  out->lat_lon = static_cast<double *>(std::malloc(points.size() * 2 * sizeof(double)));
  if (out->lat_lon == nullptr)
  {
    std::free(out);
    return nullptr;
  }
  for (size_t i = 0; i < points.size(); ++i)
  {
    out->lat_lon[2 * i] = points[i].m_lat;
    out->lat_lon[2 * i + 1] = points[i].m_lon;
  }
  out->count = static_cast<int32_t>(points.size());
  return out;
}

inline void FreeRoutePoints(AgusRoutePoints * points)
{
  if (points == nullptr)
    return;
  std::free(points->lat_lon);
  std::free(points);
}

inline void SetTurnNotificationsEnabled(Framework * framework, int32_t enabled)
{
  if (framework != nullptr)
    framework->GetRoutingManager().EnableTurnNotifications(enabled != 0);
}

inline int32_t GetTurnNotificationsEnabled(Framework * framework)
{
  return framework != nullptr &&
         framework->GetRoutingManager().AreTurnNotificationsEnabled();
}

inline void SetTurnNotificationsLocale(Framework * framework, char const * locale)
{
  if (framework != nullptr)
  {
    framework->GetRoutingManager().SetTurnNotificationsLocale(
        locale == nullptr ? std::string() : std::string(locale));
  }
}

inline routing::SpeedCameraManagerMode SpeedCameraModeFromInt(int32_t mode)
{
  if (mode == static_cast<int32_t>(routing::SpeedCameraManagerMode::Always))
    return routing::SpeedCameraManagerMode::Always;
  if (mode == static_cast<int32_t>(routing::SpeedCameraManagerMode::Never))
    return routing::SpeedCameraManagerMode::Never;
  return routing::SpeedCameraManagerMode::Auto;
}

inline void SetSpeedCameraMode(Framework * framework, int32_t mode)
{
  if (framework != nullptr)
    framework->GetRoutingManager().GetSpeedCamManager().SetMode(
        SpeedCameraModeFromInt(mode));
}

inline int32_t GetSpeedCameraMode(Framework * framework)
{
  if (framework == nullptr)
    return static_cast<int32_t>(routing::SpeedCameraManagerMode::Auto);
  return static_cast<int32_t>(
      framework->GetRoutingManager().GetSpeedCamManager().GetMode());
}
}  // namespace agus::navigation