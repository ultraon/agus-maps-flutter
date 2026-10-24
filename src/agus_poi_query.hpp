#pragma once

#include "agus_maps_flutter.h"

#include "map/framework.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/scales.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"

#include "3party/opening_hours/opening_hours.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <limits>
#include <string>
#include <vector>

// Reads every feature of the requested classificator types around a point,
// straight from the registered maps. Unlike the CoMaps search, it has no
// ranking cap, does not follow the camera and draws no search marks.
namespace agus_poi_query
{
namespace
{
enum PoiQueryStatus : int32_t
{
  kOk = 0,
  kNotReady = -1,
  kFailed = -2,
  kBadArguments = -3,
};

struct RequestedType
{
  uint32_t type;
  uint8_t level;
  int32_t index;
};

// Mirrors the open_state values of AgusPoiResult.
enum PoiOpenState : int32_t
{
  kOpenUnknown = 0,
  kOpenNow = 1,
  kClosedNow = 2,
  kOpenAlways = 3,
};

// A next change further away than this counts as none, as in the CoMaps
// place page: the opening hours evaluation looks one week ahead.
constexpr time_t kOpenStateHorizonSeconds = 7 * 24 * 60 * 60;

struct OpenStatus
{
  int32_t state = kOpenUnknown;
  int64_t nextChangeSeconds = 0;
};

struct PoiRow
{
  std::string name;
  int32_t typeIndex;
  double lat;
  double lon;
  double distanceMeters;
  OpenStatus open;
  std::string cuisine;
};

char * CopyPoiCString(std::string const & value)
{
  auto * out = static_cast<char *>(std::malloc(value.size() + 1));
  if (!out)
    return nullptr;
  std::memcpy(out, value.c_str(), value.size() + 1);
  return out;
}

AgusPoiResults * NewPoiResults(int32_t status)
{
  auto * results = static_cast<AgusPoiResults *>(std::calloc(1, sizeof(AgusPoiResults)));
  if (results)
    results->status = status;
  return results;
}

// A path that the classificator does not know is skipped, so one stale
// path in the app does not fail the whole category.
std::vector<RequestedType> ResolveTypes(char const * const * typePaths, int32_t typeCount)
{
  std::vector<RequestedType> types;
  auto const & c = classif();
  for (int32_t i = 0; i < typeCount; ++i)
  {
    if (!typePaths[i] || typePaths[i][0] == '\0')
      continue;
    uint32_t const type = c.GetTypeByReadableObjectName(typePaths[i]);
    if (type == IndexAndTypeMapping::INVALID_TYPE)
    {
      LOG(LWARNING, ("Agus POI query: unknown type", typePaths[i]));
      continue;
    }
    types.push_back({type, ftype::GetLevel(type), i});
  }
  return types;
}

// The index of the first requested type that one of the feature types falls
// under, or -1. A feature type deeper than the path counts, for example
// amenity-cafe-coffee_shop under amenity-cafe.
int32_t MatchType(FeatureType & feature, std::vector<RequestedType> const & types)
{
  int32_t match = -1;
  feature.ForEachType([&](uint32_t featureType)
  {
    if (match >= 0)
      return;
    for (auto const & requested : types)
    {
      if (ftype::Trunc(featureType, requested.level) == requested.type)
      {
        match = requested.index;
        return;
      }
    }
  });
  return match;
}

std::string NameOf(FeatureType & feature)
{
  auto const translated = feature.GetTranslatedName();
  if (translated.m_primary && !translated.m_primary->empty())
    return *translated.m_primary;
  return std::string(feature.GetName(localisation::kDefaultNameIndex));
}

// The open state of the OSM opening_hours value at the device local time,
// with the next open or close change. The evaluation uses the device time
// zone, like the CoMaps place page.
OpenStatus OpenStatusOf(FeatureType & feature, time_t now)
{
  OpenStatus status;
  std::string const hours(feature.GetMetadata(feature::Metadata::FMD_OPEN_HOURS));
  if (hours.empty())
    return status;
  osmoh::OpeningHours const openingHours(hours);
  if (!openingHours.IsValid())
    return status;
  if (openingHours.IsTwentyFourHours())
  {
    status.state = kOpenAlways;
    return status;
  }

  auto const info = openingHours.GetInfo(now);
  if (info.state == osmoh::RuleState::Unknown)
    return status;
  bool const open = info.state == osmoh::RuleState::Open;
  time_t const next = open ? info.nextTimeClosed : info.nextTimeOpen;
  bool const hasNext = next != std::numeric_limits<time_t>::max() && next > now &&
                       next - now < kOpenStateHorizonSeconds;
  if (open && !hasNext)
  {
    status.state = kOpenAlways;
    return status;
  }
  status.state = open ? kOpenNow : kClosedNow;
  if (hasNext)
    status.nextChangeSeconds = static_cast<int64_t>(next);
  return status;
}

// The classificator cuisine name in plain English: "fish_and_chips" reads
// "Fish and chips". The app shows English only, so the device language of
// the CoMaps translations does not apply here.
std::string ReadableCuisine(std::string raw)
{
  std::replace(raw.begin(), raw.end(), '_', ' ');
  if (!raw.empty())
    raw[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(raw[0])));
  return raw;
}

// The cuisines of the feature joined with ", ", for example "Pizza, Italian",
// or an empty string.
std::string CuisineOf(FeatureType & feature)
{
  std::string out;
  for (auto const & raw : feature::GetCuisines(feature::TypesHolder(feature)))
  {
    if (raw.empty())
      continue;
    if (!out.empty())
      out += ", ";
    out += ReadableCuisine(raw);
  }
  return out;
}

AgusPoiResults * CopyRows(std::vector<PoiRow> const & rows)
{
  auto * results = NewPoiResults(kOk);
  if (!results || rows.empty())
    return results;

  results->results = static_cast<AgusPoiResult *>(std::calloc(rows.size(), sizeof(AgusPoiResult)));
  if (!results->results)
  {
    results->status = kFailed;
    return results;
  }

  results->result_count = static_cast<int32_t>(rows.size());
  for (size_t i = 0; i < rows.size(); ++i)
  {
    auto const & row = rows[i];
    auto & out = results->results[i];
    out.name = CopyPoiCString(row.name);
    out.type_index = row.typeIndex;
    out.lat = row.lat;
    out.lon = row.lon;
    out.distance_meters = row.distanceMeters;
    out.open_state = row.open.state;
    out.next_change_seconds = row.open.nextChangeSeconds;
    out.cuisine = CopyPoiCString(row.cuisine);
  }
  return results;
}
}  // namespace

static inline AgusPoiResults * QueryPois(Framework * framework, double lat, double lon, double radiusMeters,
                                         char const * const * typePaths, int32_t typeCount, int32_t maxResults)
{
  if (!framework)
    return NewPoiResults(kNotReady);
  if (!typePaths || typeCount <= 0 || radiusMeters <= 0.0 || maxResults <= 0)
    return NewPoiResults(kBadArguments);

  try
  {
    auto const types = ResolveTypes(typePaths, typeCount);
    if (types.empty())
      return NewPoiResults(kBadArguments);

    m2::PointD const centre = mercator::FromLatLon(lat, lon);
    m2::RectD const rect = mercator::RectByCenterXYAndSizeInMeters(centre, radiusMeters);

    time_t const now = std::time(nullptr);
    std::vector<PoiRow> rows;
    framework->GetDataSource().ForEachInRect([&](FeatureType & feature)
    {
      int32_t const typeIndex = MatchType(feature, types);
      if (typeIndex < 0)
        return;
      m2::PointD const point = feature::GetCenter(feature);
      double const distance = mercator::DistanceOnEarth(centre, point);
      if (distance > radiusMeters)
        return;
      auto const latLon = mercator::ToLatLon(point);
      rows.push_back({NameOf(feature), typeIndex, latLon.m_lat, latLon.m_lon, distance,
                      OpenStatusOf(feature, now), CuisineOf(feature)});
    }, rect, scales::GetUpperScale());

    std::sort(rows.begin(), rows.end(),
              [](PoiRow const & a, PoiRow const & b) { return a.distanceMeters < b.distanceMeters; });
    if (rows.size() > static_cast<size_t>(maxResults))
      rows.resize(static_cast<size_t>(maxResults));
    return CopyRows(rows);
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Agus POI query failed:", e.what()));
    return NewPoiResults(kFailed);
  }
  catch (...)
  {
    LOG(LERROR, ("Agus POI query failed with an unknown exception"));
    return NewPoiResults(kFailed);
  }
}

static inline void FreePoiResults(AgusPoiResults * data)
{
  if (!data)
    return;

  if (data->results)
  {
    for (int32_t i = 0; i < data->result_count; ++i)
    {
      std::free(const_cast<char *>(data->results[i].name));
      std::free(const_cast<char *>(data->results[i].cuisine));
    }
    std::free(data->results);
  }

  std::free(data);
}
}  // namespace agus_poi_query
