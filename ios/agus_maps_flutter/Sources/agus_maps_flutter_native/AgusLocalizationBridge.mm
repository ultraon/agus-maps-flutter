// Copyright (c) Agus Maps
//
// iOS localization bridge - provides platform::GetLocalizedXxx implementations
// for the CoMaps engine. These functions are called by libcomaps.a.
//
// This file bridges the CoMaps localization API with the cross-platform
// implementation in agus_localization.cpp.

#import <Foundation/Foundation.h>

#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "agus_type_names.hpp"

#include "platform/localization.hpp"
#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/settings.hpp"

// ============================================================================
// Namespace-scope state (same as agus_localization.cpp)
// ============================================================================
namespace {
std::mutex g_mutex;
std::unordered_map<std::string, std::string> g_typeTranslations;
std::unordered_map<std::string, std::string> g_stringTranslations;
std::string g_loadedLocaleTag;
std::string g_explicitLocaleTag;
// Locale and resource folder of the last load, so a failed load is not
// repeated on every call.
std::string g_attemptedLoadKey;

/// Get the current locale tag, e.g. "it-IT" or "zh-Hans-CN".
std::string GetCurrentLocaleTag()
{
	if (!g_explicitLocaleTag.empty()) {
		return g_explicitLocaleTag;
	}

	@autoreleasepool {
		NSString * langCode = [[NSLocale preferredLanguages] firstObject];
		if (langCode) {
			return std::string([langCode UTF8String]);
		}
	}
	return "en";
}

/// Path of a strings table in the main bundle, kept for apps that ship
/// LocalizedStrings/<locale> through the podspec resources.
std::string MainBundleTablePath(std::string const & localeTag, std::string const & table)
{
	@autoreleasepool {
		NSString * path = [[NSBundle mainBundle]
			pathForResource:@(table.c_str())
					 ofType:@"strings"
				inDirectory:[NSString stringWithFormat:@"LocalizedStrings/%s",
					localeTag.c_str()]];
		return path ? std::string([path UTF8String]) : std::string();
	}
}

/// Ensure translations are loaded for the current locale.
void EnsureTranslationsLoaded()
{
	std::string const currentLocale = GetCurrentLocaleTag();
	// comaps_init_paths moves the resource folder from the app bundle to the
	// extracted data, so a lookup before it must not block the later load.
	std::string const resourcesDir = GetPlatform().ResourcesDir();
	std::string const loadKey = currentLocale + "|" + resourcesDir;
	if (loadKey == g_attemptedLoadKey) {
		return;
	}

	g_attemptedLoadKey = loadKey;
	g_typeTranslations.clear();
	g_stringTranslations.clear();
	g_loadedLocaleTag = agus_type_names::LoadTables(
		currentLocale,
		{
			[&resourcesDir](std::string const & locale, std::string const & table) {
				return agus_type_names::LocalizedTablePath(resourcesDir, locale, table);
			},
			MainBundleTablePath,
		},
		g_typeTranslations, g_stringTranslations);
}

}  // namespace

// ============================================================================
// Static storage for functions returning const references
// ============================================================================
namespace {
platform::LocalizedUnits g_distanceUnitsMetric{"m", "km"};
platform::LocalizedUnits g_distanceUnitsImperial{"ft", "mi"};
platform::LocalizedUnits g_altitudeUnitsMetric{"m", "m"};
platform::LocalizedUnits g_altitudeUnitsImperial{"ft", "ft"};
std::string g_speedUnitsMetric = "km/h";
std::string g_speedUnitsImperial = "mph";
}  // namespace

// ============================================================================
// platform:: namespace implementations required by CoMaps
// ============================================================================
namespace platform
{

#if defined(__clang__)
#define AGUS_WEAK __attribute__((weak))
#else
#define AGUS_WEAK
#endif

AGUS_WEAK std::string GetLocalizedTypeName(std::string const & type)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	EnsureTranslationsLoaded();

	return agus_type_names::LookupTypeName(g_typeTranslations, type);
}

AGUS_WEAK std::string GetLocalizedBrandName(std::string const & brand)
{
	return brand;
}

AGUS_WEAK std::string GetLocalizedString(std::string const & key)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	EnsureTranslationsLoaded();

	auto it = g_stringTranslations.find(key);
	if (it != g_stringTranslations.end()) {
		return it->second;
	}

	it = g_typeTranslations.find(key);
	if (it != g_typeTranslations.end()) {
		return it->second;
	}

	return key;
}

AGUS_WEAK std::string GetCurrencySymbol(std::string const & currencyCode)
{
	@autoreleasepool {
		NSLocale * locale = [NSLocale currentLocale];
		NSString * code = [NSString stringWithUTF8String:currencyCode.c_str()];
		NSString * symbol =
			[locale displayNameForKey:NSLocaleCurrencySymbol value:code];
		if (symbol) {
			return std::string([symbol UTF8String]);
		}
	}
	return currencyCode;
}

AGUS_WEAK std::string GetLocalizedMyPositionBookmarkName()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	EnsureTranslationsLoaded();

	auto it = g_stringTranslations.find("my_position");
	if (it != g_stringTranslations.end()) {
		return it->second;
	}

	return "My Position";
}

AGUS_WEAK LocalizedUnits const & GetLocalizedDistanceUnits()
{
	auto units = measurement_utils::Units::Metric;
	settings::TryGet(settings::kMeasurementUnits, units);
	return (units == measurement_utils::Units::Metric)
		? g_distanceUnitsMetric
		: g_distanceUnitsImperial;
}

AGUS_WEAK LocalizedUnits const & GetLocalizedAltitudeUnits()
{
	auto units = measurement_utils::Units::Metric;
	settings::TryGet(settings::kMeasurementUnits, units);
	return (units == measurement_utils::Units::Metric)
		? g_altitudeUnitsMetric
		: g_altitudeUnitsImperial;
}

AGUS_WEAK std::string const & GetLocalizedSpeedUnits(measurement_utils::Units units)
{
	return (units == measurement_utils::Units::Metric)
		? g_speedUnitsMetric
		: g_speedUnitsImperial;
}

AGUS_WEAK std::string const & GetLocalizedSpeedUnits()
{
	auto units = measurement_utils::Units::Metric;
	settings::TryGet(settings::kMeasurementUnits, units);
	return GetLocalizedSpeedUnits(units);
}

}  // namespace platform

#undef AGUS_WEAK

// ============================================================================
// C API for setting locale from Dart FFI
// ============================================================================
extern "C" {

__attribute__((visibility("default")))
void agus_localization_set_locale(const char * localeTag)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_explicitLocaleTag = localeTag ? localeTag : "";
	g_typeTranslations.clear();
	g_stringTranslations.clear();
	g_loadedLocaleTag.clear();
	g_attemptedLoadKey.clear();
}

__attribute__((visibility("default")))
const char * agus_localization_get_locale()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	EnsureTranslationsLoaded();

	if (g_loadedLocaleTag.empty()) {
		return nullptr;
	}

	static thread_local std::string s_buffer;
	s_buffer = g_loadedLocaleTag;
	return s_buffer.c_str();
}

}  // extern "C"
