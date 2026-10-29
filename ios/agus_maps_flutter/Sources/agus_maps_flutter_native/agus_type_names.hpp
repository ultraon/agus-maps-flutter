// Helpers for the iOS type name lookup in AgusLocalizationBridge.mm, kept in
// a header so test/native/run_type_names_test.sh can run them on the host.

#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

namespace agus_type_names
{
/// Turns a CoMaps readable type ("amenity-place_of_worship-christian") into
/// its LocalizableTypes.strings key ("type.amenity.place_of_worship.christian"),
/// the same way as src/agus_localization.cpp and upstream CoMaps iOS.
inline std::string NormalizeTypeKey(std::string const & type)
{
  std::string key = type;
  if (key.rfind("type.", 0) != 0)
    key = "type." + key;
  std::replace(key.begin(), key.end(), '-', '.');
  std::replace(key.begin(), key.end(), ':', '_');
  return key;
}

/// Locale folders to try, most specific first: "zh-Hans-CN" gives "zh-Hans-CN",
/// "zh-Hans", "zh", then "en" as the last fallback.
inline std::vector<std::string> BuildLocaleCandidates(std::string const & localeTag)
{
  std::vector<std::string> candidates;
  std::string tag = localeTag;
  std::replace(tag.begin(), tag.end(), '_', '-');
  while (!tag.empty())
  {
    candidates.push_back(tag);
    auto const separator = tag.rfind('-');
    if (separator == std::string::npos)
      break;
    tag.resize(separator);
  }
  if (std::find(candidates.begin(), candidates.end(), "en") == candidates.end())
    candidates.push_back("en");
  return candidates;
}

/// Path of a strings table that extractDataFiles copies from
/// assets/comaps_data/localized_types into the engine resource folder.
inline std::string LocalizedTablePath(std::string const & resourcesDir,
                                      std::string const & locale,
                                      std::string const & table)
{
  if (resourcesDir.empty())
    return {};
  std::string path = resourcesDir;
  if (path.back() != '/')
    path += '/';
  return path + "localized_types/" + locale + ".lproj/" + table + ".strings";
}

using Translations = std::unordered_map<std::string, std::string>;

/// Name of a CoMaps type from a loaded LocalizableTypes table; the dashed
/// type itself when the table has no entry.
inline std::string LookupTypeName(Translations const & types, std::string const & type)
{
  auto const it = types.find(NormalizeTypeKey(type));
  return it == types.end() ? type : it->second;
}

#ifdef __OBJC__
/// Adds the entries of a .strings file (an old-style property list) to the
/// map, replacing entries with the same key. Returns false when the file is
/// missing, unreadable or has no entries.
inline bool LoadStringsFile(std::string const & path, Translations & translations)
{
  if (path.empty())
    return false;
  bool loaded = false;
  @autoreleasepool
  {
    NSDictionary * dict = [NSDictionary dictionaryWithContentsOfFile:@(path.c_str())];
    for (NSString * key in dict)
    {
      id value = dict[key];
      if ([key isKindOfClass:[NSString class]] && [value isKindOfClass:[NSString class]])
      {
        translations[[key UTF8String]] = [(NSString *)value UTF8String];
        loaded = true;
      }
    }
  }
  return loaded;
}

/// Returns the path of one table for one locale, or "" when there is none.
using TablePathOf = std::function<std::string(std::string const & locale, std::string const & table)>;

/// Loads the LocalizableTypes and Localizable tables of every candidate
/// locale, "en" first and the full tag last, so a key that a regional table
/// lacks falls back to its language and then to English. For each locale the
/// first path that loads wins. Returns the most specific locale that loaded,
/// or "" when none did.
inline std::string LoadTables(std::string const & localeTag, std::vector<TablePathOf> const & pathsOf,
                              Translations & types, Translations & strings)
{
  std::string loadedLocale;
  auto const candidates = BuildLocaleCandidates(localeTag);
  for (auto it = candidates.rbegin(); it != candidates.rend(); ++it)
  {
    bool typesLoaded = false;
    for (auto const & pathOf : pathsOf)
    {
      if (LoadStringsFile(pathOf(*it, "LocalizableTypes"), types))
      {
        typesLoaded = true;
        break;
      }
    }
    if (!typesLoaded)
      continue;
    loadedLocale = *it;
    for (auto const & pathOf : pathsOf)
    {
      if (LoadStringsFile(pathOf(*it, "Localizable"), strings))
        break;
    }
  }
  return loadedLocale;
}
#endif
}  // namespace agus_type_names
