#pragma once

#include "agus_maps_flutter.h"
#include "agus_open_hours.hpp"

#include "map/framework.hpp"

#include "indexer/classificator.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_utils.hpp"
#include "indexer/scales.hpp"

#include "indexer/data_source.hpp"

#include "search/reverse_geocoder.hpp"

#include "i18n/string_utf8_multilang.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <limits>
#include <string>
#include <utility>
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

using agus_open_hours::OpenStatus;

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

OpenStatus OpenStatusOf(FeatureType & feature, time_t now)
{
  return agus_open_hours::OpenStatusOfHours(
      std::string(feature.GetMetadata(feature::Metadata::FMD_OPEN_HOURS)), now);
}

// The classificator cuisine name in plain English: "thai" reads "Thai".
// The app shows English only, so the CoMaps translations, which follow the
// device language, do not apply here. The table holds the names whose
// CoMaps English text differs from the plain form.
std::string ReadableCuisine(std::string raw)
{
  static std::pair<char const *, char const *> const kEnglish[] = {
      {"beef_bowl", "Beef Bowl"},       {"bubble_tea", "Bubble Tea"},
      {"coffee_shop", "Coffee"},        {"fine_dining", "Fine Dining"},
      {"fish_and_chips", "Fish and Chips"}, {"ice_cream", "Ice Cream"},
      {"oriental", "East Asian"},       {"savory_pancakes", "Savory Pancakes"},
      {"steak_house", "Steak House"},
  };
  for (auto const & [key, english] : kEnglish)
  {
    if (raw == key)
      return english;
  }
  std::replace(raw.begin(), raw.end(), '_', ' ');
  if (!raw.empty())
    raw[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(raw[0])));
  return raw;
}

// The cuisines of the feature joined with ", ", for example "Pizza, Italian",
// or an empty string. CoMaps stores diet:vegetarian and diet:vegan as
// cuisine types; they are diets, not cuisines, so they are skipped, as in
// the CoMaps editor.
std::string CuisineOf(FeatureType & feature)
{
  std::string out;
  for (auto const & raw : feature::GetCuisines(feature::TypesHolder(feature)))
  {
    if (raw.empty() || raw == "vegetarian" || raw == "vegan")
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

namespace
{
// How far from the requested position a feature may lie. The explore rows
// carry the feature centre, so the right feature is normally at 0 m.
constexpr double kDetailsMatchMeters = 15.0;
constexpr int32_t kDetailsNotFound = -4;

using DetailRow = std::pair<std::string, std::string>;

AgusPoiDetails * NewPoiDetails(int32_t status)
{
  auto * details = static_cast<AgusPoiDetails *>(std::calloc(1, sizeof(AgusPoiDetails)));
  if (details)
    details->status = status;
  return details;
}

AgusPoiDetails * CopyDetails(std::vector<DetailRow> const & rows)
{
  auto * details = NewPoiDetails(kOk);
  if (!details || rows.empty())
    return details;

  details->details = static_cast<AgusPoiDetail *>(std::calloc(rows.size(), sizeof(AgusPoiDetail)));
  if (!details->details)
  {
    details->status = kFailed;
    return details;
  }
  details->detail_count = static_cast<int32_t>(rows.size());
  for (size_t i = 0; i < rows.size(); ++i)
  {
    details->details[i].key = CopyPoiCString(rows[i].first);
    details->details[i].value = CopyPoiCString(rows[i].second);
  }
  return details;
}

// The OSM description in English, or in the default language when the
// feature has no English one.
std::string DescriptionOf(FeatureType & feature)
{
  std::string_view const raw = feature.GetMetadata(feature::Metadata::FMD_DESCRIPTION);
  if (raw.empty())
    return {};
  auto const texts = StringUtf8Multilang::FromBuffer(std::string(raw));
  std::string text;
  if (texts.GetString(localisation::kEnglishLanguageIndex, text) && !text.empty())
    return text;
  texts.GetString(localisation::kDefaultNameIndex, text);
  return text;
}

// "yes", "limited", "no", or empty from the wheelchair-* type.
std::string WheelchairOf(FeatureType & feature)
{
  std::string value = feature::GetReadableWheelchairType(feature::TypesHolder(feature));
  std::string const prefix = "wheelchair-";
  if (value.rfind(prefix, 0) == 0)
    value.erase(0, prefix.size());
  return value;
}

// Only the address of the feature itself: an address found near the point
// can belong to a neighbouring building.
std::string AddressOf(DataSource const & dataSource, FeatureType & feature)
{
  search::ReverseGeocoder const coder(dataSource);
  search::ReverseGeocoder::Address address;
  if (!coder.GetExactAddress(feature, address))
    return {};
  return address.FormatAddress();
}

std::vector<DetailRow> DetailsOf(DataSource const & dataSource, FeatureType & feature)
{
  std::vector<DetailRow> rows;
  auto const add = [&rows](char const * key, std::string value)
  {
    if (!value.empty())
      rows.emplace_back(key, std::move(value));
  };
  auto const meta = [&feature](feature::Metadata::EType type)
  { return std::string(feature.GetMetadata(type)); };

  add("opening_hours", meta(feature::Metadata::FMD_OPEN_HOURS));
  add("cuisine", CuisineOf(feature));
  add("address", AddressOf(dataSource, feature));
  add("phone", meta(feature::Metadata::FMD_PHONE_NUMBER));
  add("website", meta(feature::Metadata::FMD_WEBSITE));
  add("email", meta(feature::Metadata::FMD_EMAIL));
  add("wheelchair", WheelchairOf(feature));
  add("internet_access", meta(feature::Metadata::FMD_INTERNET));
  add("outdoor_seating", meta(feature::Metadata::FMD_OUTDOOR_SEATING));
  add("operator", meta(feature::Metadata::FMD_OPERATOR));
  add("brand", meta(feature::Metadata::FMD_BRAND));
  add("description", DescriptionOf(feature));
  return rows;
}

struct DetailsMatch
{
  FeatureID id;
  double distanceMeters = std::numeric_limits<double>::max();
};
}  // namespace

static inline AgusPoiDetails * GetPoiDetails(Framework * framework, double lat, double lon, char const * name,
                                             char const * const * typePaths, int32_t typeCount)
{
  if (!framework)
    return NewPoiDetails(kNotReady);
  if (typeCount < 0 || (typeCount > 0 && !typePaths))
    return NewPoiDetails(kBadArguments);

  try
  {
    auto const types = typeCount > 0 ? ResolveTypes(typePaths, typeCount) : std::vector<RequestedType>{};
    if (typeCount > 0 && types.empty())
      return NewPoiDetails(kBadArguments);
    std::string const wantedName = name ? name : "";
    m2::PointD const centre = mercator::FromLatLon(lat, lon);
    m2::RectD const rect = mercator::RectByCenterXYAndSizeInMeters(centre, kDetailsMatchMeters);

    DetailsMatch best;
    framework->GetDataSource().ForEachInRect([&](FeatureType & feature)
    {
      if (!types.empty() && MatchType(feature, types) < 0)
        return;
      double const distance = mercator::DistanceOnEarth(centre, feature::GetCenter(feature));
      if (distance > kDetailsMatchMeters)
        return;
      // With a wanted name only that name matches, so a neighbour of the
      // same type never lends its facts to the place.
      if (!wantedName.empty() && NameOf(feature) != wantedName)
        return;
      if (distance < best.distanceMeters)
        best = {feature.GetID(), distance};
    }, rect, scales::GetUpperScale());

    if (!best.id.IsValid())
      return NewPoiDetails(kDetailsNotFound);

    // The feature is read again outside ForEachInRect, because the address
    // lookup reads the data source itself.
    FeaturesLoaderGuard const guard(framework->GetDataSource(), best.id.m_mwmId);
    auto feature = guard.GetFeatureByIndex(best.id.m_index);
    if (!feature)
      return NewPoiDetails(kDetailsNotFound);
    return CopyDetails(DetailsOf(framework->GetDataSource(), *feature));
  }
  catch (std::exception const & e)
  {
    LOG(LERROR, ("Agus POI details failed:", e.what()));
    return NewPoiDetails(kFailed);
  }
  catch (...)
  {
    LOG(LERROR, ("Agus POI details failed with an unknown exception"));
    return NewPoiDetails(kFailed);
  }
}

static inline void FreePoiDetails(AgusPoiDetails * data)
{
  if (!data)
    return;
  if (data->details)
  {
    for (int32_t i = 0; i < data->detail_count; ++i)
    {
      std::free(const_cast<char *>(data->details[i].key));
      std::free(const_cast<char *>(data->details[i].value));
    }
    std::free(data->details);
  }
  std::free(data);
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
