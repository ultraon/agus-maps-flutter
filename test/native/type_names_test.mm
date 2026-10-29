// Host test of ios/.../agus_type_names.hpp, the helpers behind the iOS type
// name lookup. Run it with test/native/run_type_names_test.sh; the script
// passes the app's localized_types folder as the first argument.

#include "agus_type_names.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace
{
int g_failures = 0;

void Expect(char const * name, std::string const & actual, std::string const & expected)
{
  bool const ok = actual == expected;
  std::printf("%s %s: got \"%s\", expected \"%s\"\n", ok ? "PASS" : "FAIL", name,
              actual.c_str(), expected.c_str());
  if (!ok)
    ++g_failures;
}

std::string Join(std::vector<std::string> const & parts)
{
  std::string out;
  for (auto const & part : parts)
    out += (out.empty() ? "" : ",") + part;
  return out;
}

// Loads the tables and looks a CoMaps type up the way the bridge's
// GetLocalizedTypeName does.
std::string Lookup(std::string const & resourcesDir, std::string const & localeTag,
                   std::string const & type)
{
  using namespace agus_type_names;
  Translations types;
  Translations strings;
  LoadTables(localeTag,
             {[&resourcesDir](std::string const & locale, std::string const & table) {
               return LocalizedTablePath(resourcesDir, locale, table);
             }},
             types, strings);
  return LookupTypeName(types, type);
}
}  // namespace

int main(int argc, char ** argv)
{
  using namespace agus_type_names;

  Expect("dashes become dots", NormalizeTypeKey("amenity-place_of_worship-christian"),
         "type.amenity.place_of_worship.christian");
  Expect("colon becomes underscore", NormalizeTypeKey("disusedbusiness-shop:clothes"),
         "type.disusedbusiness.shop_clothes");
  Expect("prefix kept once", NormalizeTypeKey("type.amenity.cafe"), "type.amenity.cafe");

  Expect("region tag", Join(BuildLocaleCandidates("it-IT")), "it-IT,it,en");
  Expect("script tag", Join(BuildLocaleCandidates("zh-Hans-CN")), "zh-Hans-CN,zh-Hans,zh,en");
  Expect("underscore tag", Join(BuildLocaleCandidates("en_GB")), "en-GB,en");
  Expect("empty tag", Join(BuildLocaleCandidates("")), "en");

  Expect("no resource folder", LocalizedTablePath("", "en", "LocalizableTypes"), "");
  Expect("table path", LocalizedTablePath("/docs", "it", "LocalizableTypes"),
         "/docs/localized_types/it.lproj/LocalizableTypes.strings");

  if (argc < 2)
  {
    std::printf("FAIL usage: type_names_test <folder that holds localized_types>\n");
    return 1;
  }
  std::string const dir = argv[1];
  Expect("English church", Lookup(dir, "en-US", "amenity-place_of_worship-christian"), "Church");
  Expect("English subway", Lookup(dir, "en-US", "railway-station-subway-milan"), "Subway Station");
  Expect("English office", Lookup(dir, "en-US", "tourism-information-office"), "Tourist Office");
  Expect("English car park", Lookup(dir, "en-US", "amenity-parking-multi-storey"),
         "Multi Storey Parking");
  Expect("Italian church", Lookup(dir, "it-IT", "amenity-place_of_worship-christian"), "Chiesa");
  Expect("Italian subway", Lookup(dir, "it-IT", "railway-station-subway-milan"),
         "Stazione metropolitana");
  Expect("unknown language falls back to English", Lookup(dir, "xx-YY", "amenity-parking"),
         "Parking");
  Expect("regional table wins", Lookup(dir, "pt-PT", "amenity-parking"), "Parking");
  Expect("key missing in region falls back to language", Lookup(dir, "pt-PT", "amenity-planetarium"),
         "Planet\u00e1rio");
  Expect("no tables keeps the type", Lookup("/no/such/folder", "en", "amenity-cafe"), "amenity-cafe");
  Expect("unknown type stays dashed", Lookup(dir, "en", "amenity-no_such_type"),
         "amenity-no_such_type");

  std::printf(g_failures == 0 ? "All type name tests passed\n" : "%d type name tests failed\n",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
