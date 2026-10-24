#pragma once

#include "3party/opening_hours/opening_hours.hpp"

#include <cstdint>
#include <ctime>
#include <limits>
#include <string>
#include <utility>
#include <vector>

// The open state of an OSM opening_hours value for the POI query. It only
// depends on the opening hours library, so a host test can include it.
namespace agus_open_hours
{
// Mirrors the open_state values of AgusPoiResult.
enum PoiOpenState : int32_t
{
  kOpenUnknown = 0,
  kOpenNow = 1,
  kClosedNow = 2,
  kOpenAlways = 3,
};

// A next change further away than this counts as none: the opening hours
// evaluation looks one week ahead.
constexpr time_t kOpenStateHorizonSeconds = 7 * 24 * 60 * 60;

struct OpenStatus
{
  int32_t state = kOpenUnknown;
  int64_t nextChangeSeconds = 0;
};

// The local clock time [hours]:[minutes] on the day [dayOffset] days after
// the day of [now]. tm_isdst = -1 lets mktime pick summer or winter time for
// that moment, so a daylight saving switch in between moves nothing.
inline time_t AtLocalTime(time_t now, int dayOffset, int hours, int minutes)
{
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
  tm.tm_mday += dayOffset;
  tm.tm_hour = hours;
  tm.tm_min = minutes;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  return std::mktime(&tm);
}

// The first rule start, rule end or midnight within one week after [now]
// where the place becomes open ([toOpen]) or closed, or 0.
//
// osmoh::OpeningHours::GetInfo is not used for this: its GetNextTimeState
// builds each candidate with mktime on a copy of the tm of [now], which
// keeps the tm_isdst of [now], and adds whole days in seconds. Before a
// daylight saving switch every candidate after the switch moves by one hour.
// On 2026-10-25 at 01:19 in Warsaw, "Su 11:00-22:00" was checked at 10:00,
// found closed, and the next open time read Monday.
inline int64_t NextChange(osmoh::OpeningHours const & openingHours, time_t now, bool toOpen)
{
  std::vector<std::pair<int, int>> clocks{{0, 0}};
  for (auto const & rule : openingHours.GetRule())
  {
    for (auto const & span : rule.GetTimes())
    {
      for (auto const * time : {&span.GetStart(), &span.GetEnd()})
      {
        if (time->IsHoursMinutes())
          clocks.emplace_back(time->GetHoursCount(), time->GetMinutesCount());
      }
    }
  }

  time_t best = std::numeric_limits<time_t>::max();
  for (int day = 0; day <= 7; ++day)
  {
    for (auto const & [hours, minutes] : clocks)
    {
      time_t const candidate = AtLocalTime(now, day, hours, minutes);
      if (candidate == -1 || candidate <= now || candidate >= best ||
          candidate - now >= kOpenStateHorizonSeconds)
        continue;
      // The end of a range is exclusive, so the state one minute later
      // tells what starts at the candidate.
      time_t const probe = candidate + 60;
      if (toOpen ? openingHours.IsOpen(probe) : openingHours.IsClosed(probe))
        best = candidate;
    }
  }
  return best == std::numeric_limits<time_t>::max() ? 0 : static_cast<int64_t>(best);
}

// The open state of the OSM opening_hours value [hours] at [now], device
// local time, with the next open or close change. Known limitation: the
// evaluation uses the device time zone, not the time zone of the place,
// like the CoMaps place page. Only a true 24/7 rule reads as always open.
inline OpenStatus OpenStatusOfHours(std::string const & hours, time_t now)
{
  OpenStatus status;
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

  bool const open = openingHours.IsOpen(now);
  if (!open && !openingHours.IsClosed(now))
    return status;
  status.state = open ? kOpenNow : kClosedNow;
  status.nextChangeSeconds = NextChange(openingHours, now, !open);
  return status;
}
}  // namespace agus_open_hours
