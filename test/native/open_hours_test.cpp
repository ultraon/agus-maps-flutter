// Host test of src/agus_open_hours.hpp against the CoMaps opening hours
// library. Run it with test/native/run_open_hours_test.sh; the script sets
// TZ=Europe/Warsaw, where summer time ends on 2026-10-25 at 03:00.

#include "agus_open_hours.hpp"

#include <cstdio>
#include <ctime>
#include <string>

namespace
{
using agus_open_hours::OpenStatusOfHours;

int g_failures = 0;

time_t Local(int year, int month, int day, int hour, int minute)
{
  std::tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_isdst = -1;
  return std::mktime(&tm);
}

std::string Format(int64_t seconds)
{
  if (seconds == 0)
    return "none";
  time_t const t = static_cast<time_t>(seconds);
  std::tm tm{};
  localtime_r(&t, &tm);
  char text[32];
  std::strftime(text, sizeof(text), "%a %Y-%m-%d %H:%M", &tm);
  return text;
}

void Expect(char const * name, std::string const & hours, time_t now, int32_t state,
            time_t nextChange)
{
  auto const status = OpenStatusOfHours(hours, now);
  bool const ok = status.state == state && status.nextChangeSeconds == static_cast<int64_t>(nextChange);
  std::printf("%s %s: state %d next %s (expected state %d next %s)\n", ok ? "PASS" : "FAIL", name,
              status.state, Format(status.nextChangeSeconds).c_str(), state,
              Format(static_cast<int64_t>(nextChange)).c_str());
  if (!ok)
    ++g_failures;
}
}  // namespace

int main()
{
  using namespace agus_open_hours;
  std::string const deli = "Mo-Sa 10:00-22:00; Su 11:00-22:00";
  std::string const office = "Mo-Fr 09:00-17:00";

  // Closed before its opening time on the same day, on the day summer time
  // ends: it opens today at 11:00, not Monday.
  Expect("deli on the switch day", deli, Local(2026, 10, 25, 1, 19), kClosedNow,
         Local(2026, 10, 25, 11, 0));
  Expect("closed before opening on a normal day", office, Local(2026, 10, 21, 7, 0), kClosedNow,
         Local(2026, 10, 21, 9, 0));
  Expect("open reads the close time", office, Local(2026, 10, 21, 12, 0), kOpenNow,
         Local(2026, 10, 21, 17, 0));
  // Saturday before the switch: Monday's opening is after the switch.
  Expect("weekend across the switch", office, Local(2026, 10, 24, 23, 0), kClosedNow,
         Local(2026, 10, 26, 9, 0));
  Expect("open past midnight", "Mo-Su 18:00-02:00", Local(2026, 10, 21, 23, 0), kOpenNow,
         Local(2026, 10, 22, 2, 0));
  Expect("24/7", "24/7", Local(2026, 10, 25, 1, 19), kOpenAlways, 0);
  Expect("no hours", "", Local(2026, 10, 25, 1, 19), kOpenUnknown, 0);

  std::printf(g_failures == 0 ? "All open hours tests passed\n" : "%d open hours tests failed\n",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
