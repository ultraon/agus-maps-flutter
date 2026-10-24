#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if _WIN32
#define FFI_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FFI_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They will block the Dart execution while running the native function, so
// only do this for native functions which are guaranteed to be short-lived.
FFI_PLUGIN_EXPORT int sum(int a, int b);

// A longer lived native function, which occupies the thread calling it.
//
// Do not call these kind of native functions in the main isolate. They will
// block Dart execution. This will cause dropped frames in Flutter applications.
// Instead, call these native functions on a separate isolate.
FFI_PLUGIN_EXPORT int sum_long_running(int a, int b);

FFI_PLUGIN_EXPORT void comaps_init(const char* apkPath, const char* storagePath);
FFI_PLUGIN_EXPORT void comaps_init_paths(const char* resourcePath, const char* writablePath);
FFI_PLUGIN_EXPORT void comaps_load_map_path(const char* path);

/// Set the locale for native POI type localization.
/// Must be called after comaps_init_paths() for proper localization.
/// If not called, the system locale is auto-detected.
/// Can be called at any time to change locale (affects subsequent place page requests).
/// @param localeTag BCP 47 locale tag (e.g., "en-US", "zh-Hans", "de")
FFI_PLUGIN_EXPORT void comaps_set_locale(const char* localeTag);

FFI_PLUGIN_EXPORT void comaps_set_view(double lat, double lon, int zoom);

// Get current viewport center coordinates in WGS84.
// Returns 1 when values were written, 0 when the framework is not ready.
FFI_PLUGIN_EXPORT int comaps_get_viewport_center(double* lat, double* lon);

// Get the current integer draw scale/zoom level, or -1 when unavailable.
FFI_PLUGIN_EXPORT int comaps_get_current_zoom(void);

// Converts physical screen coordinates to WGS84 coordinates using the latest
// native viewport. Returns 1 when values were written.
FFI_PLUGIN_EXPORT int comaps_screen_to_latlon(
	double physical_x,
	double physical_y,
	double* lat,
	double* lon);

// Updates the native map pointer tracker and converts it to WGS84 coordinates.
// Returns 1 when values were written.
FFI_PLUGIN_EXPORT int comaps_update_map_pointer(
	double physical_x,
	double physical_y,
	int inside_map,
	double* lat,
	double* lon);

// Converts WGS84 coordinates to physical screen coordinates using the latest
// native viewport. Returns 1 when values were written.
FFI_PLUGIN_EXPORT int comaps_latlon_to_screen(
	double lat,
	double lon,
	double* physical_x,
	double* physical_y);

// Relative zoom controls centered on the visible viewport.
FFI_PLUGIN_EXPORT void comaps_zoom_in(int animated);
FFI_PLUGIN_EXPORT void comaps_zoom_out(int animated);

// Map bearing in degrees where 0 is north-up.
FFI_PLUGIN_EXPORT double comaps_get_current_bearing(void);
FFI_PLUGIN_EXPORT void comaps_set_bearing(double degrees, int animated);
FFI_PLUGIN_EXPORT void comaps_reset_bearing(int animated);

// Enable/disable runtime 3D building rendering.
// Flutter owns persistence for this setting; the native bridge only applies
// the current process state to Drape.
FFI_PLUGIN_EXPORT void comaps_set_3d_buildings_enabled(int enabled);
FFI_PLUGIN_EXPORT int comaps_get_3d_buildings_enabled(void);

// Map theme: 0 = light, 1 = dark. Auto mode is resolved by Dart.
FFI_PLUGIN_EXPORT void comaps_set_map_theme(int dark);
FFI_PLUGIN_EXPORT int comaps_get_map_theme_is_dark(void);

// Overlay/style layers matching CoMaps mobile behavior:
// outdoors and isolines may be enabled together; subway/transit disables both.
FFI_PLUGIN_EXPORT void comaps_set_outdoors_enabled(int enabled);
FFI_PLUGIN_EXPORT void comaps_set_isolines_enabled(int enabled);
FFI_PLUGIN_EXPORT void comaps_set_subway_enabled(int enabled);
FFI_PLUGIN_EXPORT void comaps_get_map_layer_state(int* outdoors, int* isolines, int* subway);

// Map label language code. Empty/null = auto; "default" = local/native names.
FFI_PLUGIN_EXPORT void comaps_set_map_language(const char* languageCode);

// Invalidate the current viewport to force tile reload
FFI_PLUGIN_EXPORT void comaps_invalidate(void);

// Force complete redraw by triggering UpdateMapStyle
// This clears all render groups and forces complete tile re-request
// Call after registering maps to ensure tiles are loaded
FFI_PLUGIN_EXPORT void comaps_force_redraw(void);

// Touch event handling
// type: 1=TOUCH_DOWN, 2=TOUCH_MOVE, 3=TOUCH_UP, 4=TOUCH_CANCEL
// id1, x1, y1: first touch pointer
// id2, x2, y2: second touch pointer (use -1 for id2 if single touch)
FFI_PLUGIN_EXPORT void comaps_touch(int type, int id1, float x1, float y1, int id2, float x2, float y2);

// Place page (POI) information
// Returns 1 if a place page is available, 0 otherwise
FFI_PLUGIN_EXPORT int comaps_place_page_has_data(void);

// Structured place page payload for native consumers.
typedef struct {
	const char* mwm_name;
	int64_t mwm_version;
	int64_t index;
} AgusPlacePageFeatureId;

typedef struct {
	const char* decimal;
	const char* dms;
	const char* osm;
	const char* olc;
	const char* utm;
	const char* mgrs;
} AgusPlacePageCoordinates;

typedef struct {
	int64_t key;
	const char* value;
} AgusPlacePageIntMetadataEntry;

typedef struct {
	const char* key;
	const char* value;
} AgusPlacePageStringMetadataEntry;

typedef struct {
	AgusPlacePageFeatureId feature_id;
	int32_t object_type;
	int32_t opening_mode;
	const char* title;
	const char* secondary_title;
	const char* subtitle;
	const char* address;
	double lat;
	double lon;
	const char* wiki_description_html;
	int32_t road_type;
	int32_t is_route_point;
	AgusPlacePageCoordinates coordinates;
	const char** raw_types;
	int32_t raw_types_count;
	AgusPlacePageIntMetadataEntry* metadata;
	int32_t metadata_count;
	AgusPlacePageStringMetadataEntry* metadata_tags;
	int32_t metadata_tags_count;
	int32_t has_bookmark_id;
	int64_t bookmark_id;
	int32_t has_bookmark_category_id;
	int64_t bookmark_category_id;
	int32_t has_track_id;
	int64_t track_id;
} AgusPlacePageData;

// Returns a heap-allocated snapshot of the current place page data, or NULL.
// Call comaps_place_page_free when done.
FFI_PLUGIN_EXPORT AgusPlacePageData* comaps_place_page_copy(void);

// Frees a snapshot allocated by comaps_place_page_copy.
FFI_PLUGIN_EXPORT void comaps_place_page_free(AgusPlacePageData* data);

// Clear the current place page selection
FFI_PLUGIN_EXPORT void comaps_place_page_clear_selection(void);

// Search result types mirrored from search::Result::Type.
// 0=Feature, 1=LatLon, 2=PureSuggest, 3=SuggestFromFeature, 4=Postcode.
typedef struct {
	int32_t index;
	int32_t result_type;
	int32_t is_suggestion;
	int32_t has_point;
	const char* title;
	const char* subtitle;
	const char* address;
	const char* suggestion;
	double lat;
	double lon;
} AgusSearchResult;

// Search snapshot status values:
// 0=idle, 1=running, 2=completed, 3=cancelled, 4=error.
typedef struct {
	int32_t generation;
	int32_t status;
	int32_t result_count;
	AgusSearchResult* results;
} AgusSearchResults;

// Start native CoMaps search. Returns the search generation, or a negative
// value if the framework/query is not ready. When interactive is non-zero,
// CoMaps also starts search-in-viewport for native map result marks.
FFI_PLUGIN_EXPORT int32_t comaps_search_start(
	const char* query,
	const char* locale,
	int32_t interactive,
	int32_t isCategory);

// Returns a heap-allocated snapshot of the latest native search state.
// Call comaps_search_results_free when done.
FFI_PLUGIN_EXPORT AgusSearchResults* comaps_search_copy_results(void);

// Frees a snapshot allocated by comaps_search_copy_results.
FFI_PLUGIN_EXPORT void comaps_search_results_free(AgusSearchResults* data);

// Selects a non-suggestion result from the latest native search result list.
// Returns 1 on success, 0 if the index points to a suggestion, or negative on
// invalid state/index.
FFI_PLUGIN_EXPORT int32_t comaps_search_show_result(int32_t index);

// Cancels active native search requests and clears cached search results.
FFI_PLUGIN_EXPORT void comaps_search_cancel(void);

// One feature found by comaps_query_pois. type_index is the position of the
// matching path in the request. name is empty when the feature has none.
// open_state comes from the OSM opening_hours at the device local time
// (known limitation: the device time zone, not the place's):
// 0=unknown or no hours, 1=open, 2=closed, 3=open 24/7 (a 24/7 rule only).
// next_change_seconds is the Unix time of the next close (when open) or the
// next open (when closed), or 0 when none is known within one week.
// cuisine is the cuisines in English joined with ", ", or empty.
typedef struct {
	const char* name;
	int32_t type_index;
	double lat;
	double lon;
	double distance_meters;
	int32_t open_state;
	int64_t next_change_seconds;
	const char* cuisine;
} AgusPoiResult;

// Status values: 0=ok, -1=framework not ready, -2=query failed,
// -3=bad arguments or no known type path.
typedef struct {
	int32_t status;
	int32_t result_count;
	AgusPoiResult* results;
} AgusPoiResults;

// Reads every feature of the given classificator types (dash paths such as
// "amenity-cafe") within radius_meters of (lat, lon) from the registered
// maps, nearest first, at most max_results. It does not move the camera or
// touch the search state, so a background thread may call it. Call
// comaps_poi_results_free when done.
FFI_PLUGIN_EXPORT AgusPoiResults* comaps_query_pois(
	double lat,
	double lon,
	double radius_meters,
	const char** type_paths,
	int32_t type_count,
	int32_t max_results);

// Frees a result allocated by comaps_query_pois.
FFI_PLUGIN_EXPORT void comaps_poi_results_free(AgusPoiResults* data);

// One fact about a place found by comaps_get_poi_details. key is one of
// "opening_hours" (the raw OSM value), "cuisine" (English, joined with
// ", "), "address", "phone", "website", "email", "wheelchair" ("yes",
// "limited" or "no"), "internet_access", "outdoor_seating", "operator",
// "brand" or "description". Only facts with a value are listed.
typedef struct {
	const char* key;
	const char* value;
} AgusPoiDetail;

// Status values: 0=ok, -1=framework not ready, -2=read failed,
// -3=bad arguments, -4=no matching place near the position.
typedef struct {
	int32_t status;
	int32_t detail_count;
	AgusPoiDetail* details;
} AgusPoiDetails;

// Reads the facts of one place: the feature of the given classificator
// types (as for comaps_query_pois) within a few meters of (lat, lon),
// preferring the one whose name equals name. An empty name or no type
// paths widen the match to any name or any type. It does not move the
// camera or touch the search state, so a background thread may call it.
// Call comaps_poi_details_free when done.
FFI_PLUGIN_EXPORT AgusPoiDetails* comaps_get_poi_details(
	double lat,
	double lon,
	const char* name,
	const char** type_paths,
	int32_t type_count);

// Frees a result allocated by comaps_get_poi_details.
FFI_PLUGIN_EXPORT void comaps_poi_details_free(AgusPoiDetails* data);

// Navigation route point types mirrored from RouteMarkType:
// 0=Start, 1=Intermediate, 2=Finish.
// Router types mirror routing::RouterType: 0=Vehicle, 1=Pedestrian,
// 2=Bicycle, 3=Transit, 4=Ruler.
// Measurement units mirror measurement_utils::Units: 0=Metric, 1=Imperial.
// Speed camera modes mirror routing::SpeedCameraManagerMode:
// 0=Auto, 1=Always, 2=Never.
// Avoid route options are a bitmask of routing::RoutingOptions::Option values:
// Toll=2, Motorway=4, Ferry=8, Dirty/unpaved=16.
typedef struct {
	int32_t is_active;
	int32_t is_built;
	int32_t is_building;
	int32_t is_following;
	int32_t is_valid;
	int32_t has_following_info;
	int32_t router_type;
	int32_t route_session_state;
	int32_t turn;
	int32_t next_turn;
	int32_t pedestrian_turn;
	int32_t exit_number;
	int32_t total_time_seconds;
	double completion_percent;
	double speed_limit_mps;
	double distance_to_target;
	int32_t distance_to_target_units;
	double distance_to_turn;
	int32_t distance_to_turn_units;
	double distance_to_next_stop;
	int32_t distance_to_next_stop_units;
	int32_t time_to_next_stop_seconds;
	int32_t index_of_next_stop;
	const char* current_street;
	const char* next_street;
	const char* next_next_street;
} AgusNavigationStatus;

FFI_PLUGIN_EXPORT int32_t comaps_navigation_set_router(int32_t routerType);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_router(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_add_route_point(
	int32_t markType,
	const char* title,
	const char* subtitle,
	double lat,
	double lon,
	int32_t intermediateIndex,
	int32_t isMyPosition,
	int32_t reorderIntermediatePoints);
FFI_PLUGIN_EXPORT void comaps_navigation_remove_route_point(
	int32_t markType,
	int32_t intermediateIndex);
FFI_PLUGIN_EXPORT void comaps_navigation_clear_route_points(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_build_route(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_follow_route(void);
FFI_PLUGIN_EXPORT void comaps_navigation_close_route(int32_t removeRoutePoints);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_is_active(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_is_built(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_is_building(void);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_is_following(void);
FFI_PLUGIN_EXPORT AgusNavigationStatus* comaps_navigation_copy_status(void);
FFI_PLUGIN_EXPORT void comaps_navigation_status_free(AgusNavigationStatus* status);

// One turn ahead on the followed route. pedestrian_direction mirrors
// routing::turns::PedestrianDirection: 0=None, 1=GoStraight, 2=TurnRight,
// 3=TurnLeft, 4=ReachedYourDestination. distance_meters is the distance
// along the route from the current position to the turn.
typedef struct {
	int32_t pedestrian_direction;
	double distance_meters;
} AgusNavigationTurn;

#define AGUS_NAVIGATION_MAX_TURNS 2

// The current turn and, when it is not the destination, the turn after it.
// count is 0 to AGUS_NAVIGATION_MAX_TURNS, or -1 when no navigable route
// exists.
typedef struct {
	int32_t count;
	AgusNavigationTurn turns[AGUS_NAVIGATION_MAX_TURNS];
} AgusNavigationTurns;

// Fills out with the next turns of Route::GetNextTurns, at any distance.
// Returns out->count. Call it on the thread that owns the framework.
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_next_turns(AgusNavigationTurns* out);

// The points of the built route, in route order. lat_lon holds count pairs
// of latitude and longitude in degrees.
typedef struct {
	int32_t count;
	double* lat_lon;
} AgusRoutePoints;

// Returns the junction points of the built route, with extra points so that
// no two neighbours are more than max_step_meters apart (as the Android
// nativeGetRouteJunctionPoints does). A max_step_meters of 0 or less adds no
// points; a positive value below 1 m counts as 1 m. Returns null without a
// valid route. Call comaps_navigation_route_points_free when done.
FFI_PLUGIN_EXPORT AgusRoutePoints* comaps_navigation_get_route_points(double max_step_meters);

// Frees a result allocated by comaps_navigation_get_route_points.
FFI_PLUGIN_EXPORT void comaps_navigation_route_points_free(AgusRoutePoints* points);

FFI_PLUGIN_EXPORT void comaps_navigation_set_measurement_units(int32_t units);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_measurement_units(void);
FFI_PLUGIN_EXPORT void comaps_navigation_set_turn_notifications_enabled(int32_t enabled);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_turn_notifications_enabled(void);
FFI_PLUGIN_EXPORT void comaps_navigation_set_turn_notifications_locale(const char* locale);
FFI_PLUGIN_EXPORT void comaps_navigation_set_speed_camera_mode(int32_t mode);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_speed_camera_mode(void);
FFI_PLUGIN_EXPORT void comaps_navigation_set_avoid_routing_options(int32_t mask);
FFI_PLUGIN_EXPORT int32_t comaps_navigation_get_avoid_routing_options(void);

// Scale (zoom) the map by a factor, centered on a specific pixel point.
// factor: Zoom factor (>1 zooms in, <1 zooms out). Use exp(scrollDelta) for smooth zooming.
// pixelX, pixelY: Screen coordinates to zoom towards (in physical pixels)
// animated: Whether to animate the zoom transition
// This is the preferred method for scroll wheel zoom on desktop platforms.
FFI_PLUGIN_EXPORT void comaps_scale(double factor, double pixelX, double pixelY, int animated);

// Set the visible part of the map surface in physical pixels. The engine
// places the "my position" point at the centre of this rectangle in the
// follow modes, and centres camera moves on it. Ignored below 100 px.
FFI_PLUGIN_EXPORT void comaps_set_visible_viewport(double left, double top, double right, double bottom);

// Move the point (lat, lon) to the centre of the visible viewport and zoom
// by factor (>1 zooms in) in one camera move. Keeps the follow modes.
FFI_PLUGIN_EXPORT void comaps_scale_and_center(double lat, double lon, double factor, int animated);

// Register a listener for "my position" mode changes (values as in
// comaps_set_my_location_mode). It runs once right away with the current
// mode, then on the GUI thread after each change. Pass NULL to remove it.
FFI_PLUGIN_EXPORT void comaps_set_my_position_mode_listener(void (*listener)(int mode));

// Scroll/pan the map by pixel distance
// distanceX, distanceY: Distance to scroll in physical pixels
FFI_PLUGIN_EXPORT void comaps_scroll(double distanceX, double distanceY);

// Register a single MWM map file directly by full path.
// This bypasses the version folder scanning and registers the map file
// directly with the rendering engine.
// Returns: 0 on success, -1 if framework not ready, -2 on exception, 
//          or MwmSet::RegResult value on registration failure
FFI_PLUGIN_EXPORT int comaps_register_single_map(const char* fullPath);

// Register a single MWM map file directly by full path, with an explicit
// snapshot version (e.g. 251209).
//
// IMPORTANT: LocalCountryFile::MakeTemporary() sets version=0, which can lead
// to MwmSet::RegResult::VersionTooOld when the engine expects a non-zero
// snapshot version for a given country/world file.
FFI_PLUGIN_EXPORT int comaps_register_single_map_with_version(const char* fullPath, int64_t version);

// Explicitly shutdown the CoMaps framework.
// Call this before app termination to ensure clean shutdown and avoid crashes.
FFI_PLUGIN_EXPORT void comaps_shutdown(void);

// Debug: List all registered MWMs and their bounds to logcat
FFI_PLUGIN_EXPORT void comaps_debug_list_mwms();

// Debug: Check if a lat/lon point is covered by any registered MWM
FFI_PLUGIN_EXPORT void comaps_debug_check_point(double lat, double lon);

// Deregister a map file by path. Returns 0 when deregistered, 1 when no map
// of that name is registered, -1 when the framework is not initialized and
// -2 for an empty path or an exception. Implemented on Android and iOS only;
// the desktop shims keep their stubs.
FFI_PLUGIN_EXPORT int comaps_deregister_map(const char* fullPath);

// Returns the CoMaps region id (the country file name, for example
// "Poland_Masovian Voivodeship") whose border polygon contains the point, read
// from CountryInfoGetter::GetRegionCountryId. The result does not depend on
// which maps are registered. Returns an empty string when no region contains
// the point (for example at sea), and null when the framework is not
// initialized or the read fails. Call comaps_region_id_free when done.
// Implemented on Android and iOS only.
FFI_PLUGIN_EXPORT char* comaps_get_region_at_point(double lat, double lon);

// Frees a string returned by comaps_get_region_at_point.
FFI_PLUGIN_EXPORT void comaps_region_id_free(char* regionId);

// Get the count of registered maps
FFI_PLUGIN_EXPORT int comaps_get_registered_maps_count(void);

// =============================================================================
// Location Services
// =============================================================================

// Feed a GPS location update to the native engine.
// The engine uses this to display the native "my position" marker.
// timestampMs: Unix epoch milliseconds
FFI_PLUGIN_EXPORT void comaps_on_location_update(double lat, double lon, double accuracy, double bearing, double speed, int64_t timestampMs);

// Feed a compass heading update to the native engine.
// magneticNorth: heading in degrees (0-360)
// trueNorth: heading in degrees (0-360), use -1 if unavailable
FFI_PLUGIN_EXPORT void comaps_on_compass_update(double magneticNorth, double trueNorth);

// Enable or disable the native "my position" marker.
// enable: 1 = enable, 0 = disable
FFI_PLUGIN_EXPORT void comaps_enable_my_location(int enable);

// Cycle to the next my-position mode.
// The mode parameter is reserved for future use (direct mode set).
// Currently always calls SwitchMyPositionNextMode() regardless of value.
// Native modes: PendingPosition(0), NotFollowNoPosition(1), NotFollow(2),
// Follow(3), FollowAndRotate(4).
FFI_PLUGIN_EXPORT void comaps_set_my_location_mode(int mode);

// =============================================================================
// Custom Marker Functions
// =============================================================================

// Add a custom marker at the given WGS84 coordinates.
// iconId: optional icon identifier (pass NULL for default pin)
// label: optional label text (pass NULL for no label)
// Returns: positive marker ID on success, negative on error.
FFI_PLUGIN_EXPORT int comaps_add_custom_marker(double lat, double lon, const char* iconId, const char* label);

// Remove a custom marker by its ID (returned by comaps_add_custom_marker).
FFI_PLUGIN_EXPORT void comaps_remove_custom_marker(int markerId);

// Remove all custom markers.
FFI_PLUGIN_EXPORT void comaps_clear_custom_markers(void);

// =============================================================================
// DuckDB Persistence Functions
// =============================================================================

// Returns the linked DuckDB library version, or an empty string when unavailable.
FFI_PLUGIN_EXPORT const char* agus_duckdb_library_version(void);

// Returns the last DuckDB bridge error message, or an empty string.
FFI_PLUGIN_EXPORT const char* agus_duckdb_last_error(void);

// Opens the app-instance DuckDB file at writablePath/agus_layers.duckdb.
// Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_open_app_database(const char* writablePath);

// Closes the current app-instance DuckDB connection, if any.
FFI_PLUGIN_EXPORT void agus_duckdb_close(void);

// Returns 1 when the app-instance DuckDB connection is open.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_is_open(void);

// Loads and verifies the required static extensions for layer storage/querying.
// Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_load_required_extensions(void);

// Executes unrestricted SQL against the app-instance DuckDB connection.
// Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_execute(const char* sql);

// Executes SQL and returns a JSON payload with columns, rows, and row_count.
// Returns NULL on failure; call agus_duckdb_last_error() for diagnostics.
// The returned pointer is owned by the bridge and remains valid until the next
// DuckDB query-json call or bridge close/error mutation.
FFI_PLUGIN_EXPORT const char* agus_duckdb_query_json(const char* sql);

// Validates a query-layer SQL result contract without materializing rows.
// Required columns are feature_id VARCHAR, geometry GEOMETRY, and properties JSON.
// Optional style JSON and min_zoom/max_zoom/z_index INTEGER columns are checked
// when present. Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_validate_render_query(const char* sql);

typedef struct {
	char* layer_id;
	char* feature_id;
	char* geometry_kind;
	char* geometry_wkt;
	int32_t min_zoom;
	int32_t max_zoom;
	int32_t z_index;
} AgusDuckDBRenderFeature;

// Copies currently renderable DuckDB features for a WGS84 viewport and zoom.
// The returned array and nested strings are owned by the caller and must be
// released with agus_duckdb_free_render_features(). Nullable zoom values are
// returned as -1.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_copy_render_features(
		double min_lon,
		double min_lat,
		double max_lon,
		double max_lat,
		int32_t zoom,
		AgusDuckDBRenderFeature** out_features,
		int32_t* out_count);

// Copies a page of currently renderable DuckDB features for a WGS84 viewport
// and zoom. Use limit/offset to avoid one large render allocation.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_copy_render_features_page(
		double min_lon,
		double min_lat,
		double max_lon,
		double max_lat,
		int32_t zoom,
		int32_t limit,
		int32_t offset,
		AgusDuckDBRenderFeature** out_features,
		int32_t* out_count);

// Frees render features allocated by DuckDB render-feature copy APIs.
FFI_PLUGIN_EXPORT void agus_duckdb_free_render_features(
		AgusDuckDBRenderFeature* features,
		int32_t count);

// Android, macOS, and iOS wire DuckDB-backed rows into Drape user marks.
// Returns the number of visible features submitted, or a negative value when
// the map/DuckDB state is not ready. Other platforms will wire this later.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_refresh_render_layers(void);

// Renders selected-feature edit handles in a separate native Drape user-mark
// group. The WKT is parsed for lon/lat vertex pairs and is not persisted here.
FFI_PLUGIN_EXPORT void agus_duckdb_set_edit_handles_from_wkt(const char* geometryWkt);
// Native Drape interaction state: 0 = inactive, 1 = drawing, 2 = editing.
// The WKT is transient interaction geometry; Dart/DuckDB still own persistence.
FFI_PLUGIN_EXPORT void agus_duckdb_set_interaction_geometry_from_wkt(
    int32_t interactionMode, const char* geometryWkt);
// Styleable interaction geometry path used by the Pigeon host API.
FFI_PLUGIN_EXPORT void agus_duckdb_update_interaction_geometry(
    int32_t interactionMode,
    const char* geometryWkt,
    int32_t red,
    int32_t green,
    int32_t blue,
    double opacity,
    double width,
    int32_t dashed,
    double dashLength,
    double gapLength);
FFI_PLUGIN_EXPORT void agus_duckdb_clear_edit_handles(void);

// Enables or disables viewport-driven DuckDB render refreshes on platforms that
// implement native Drape integration.
FFI_PLUGIN_EXPORT void agus_duckdb_set_rendering_enabled(int32_t enabled);

// Executes a SQL migration file against the app-instance DuckDB connection.
// Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_apply_migration_file(const char* path);

// Applies embedded app schema migrations and verifies recorded checksums.
// Returns 1 on success, 0 on failure.
FFI_PLUGIN_EXPORT int32_t agus_duckdb_run_migrations(void);

// =============================================================================
// Native Surface Functions (for Windows/Desktop)
// =============================================================================

// Create the native rendering surface
// This creates the Framework, DrapeEngine, and OpenGL context
// @param width Surface width in physical pixels
// @param height Surface height in physical pixels
// @param density Screen DPI scale factor
// @return On Linux: returns 0 on success, negative on error. On other platforms: void.
#if defined(__linux__) && !defined(__ANDROID__)
FFI_PLUGIN_EXPORT int64_t agus_native_create_surface(int32_t width, int32_t height, float density);
#else
FFI_PLUGIN_EXPORT void agus_native_create_surface(int32_t width, int32_t height, float density);
#endif

// Called when the surface size changes
FFI_PLUGIN_EXPORT void agus_native_on_size_changed(int32_t width, int32_t height);

// Called when the display scale (DPI) changes at runtime
FFI_PLUGIN_EXPORT void agus_native_set_visual_scale(float density);

// Called when the surface is destroyed
FFI_PLUGIN_EXPORT void agus_native_on_surface_destroyed(void);

// Get the D3D11 shared texture handle for Flutter (Windows only)
// Returns HANDLE that can be used to open the shared texture
FFI_PLUGIN_EXPORT void* agus_get_shared_texture_handle(void);

// Get the currently displayable DXGI shared texture handle and physical size.
// The returned legacy shared handle is owned by the native D3D resource.
FFI_PLUGIN_EXPORT int agus_get_shared_texture_descriptor_info(void** handle, int32_t* width, int32_t* height);

// Get the D3D11 device pointer (Windows only)
FFI_PLUGIN_EXPORT void* agus_get_d3d11_device(void);

// Get the D3D11 texture pointer (Windows only)
FFI_PLUGIN_EXPORT void* agus_get_d3d11_texture(void);

// Get an AddRef'd D3D11 texture pointer and its current physical size
// atomically with respect to native resize. The caller must Release() the
// returned texture pointer.
FFI_PLUGIN_EXPORT int agus_get_d3d11_texture_info(void** texture, int32_t* width, int32_t* height);

// Render a single frame (triggers frame ready callback)
FFI_PLUGIN_EXPORT void agus_render_frame(void);

// Set the frame ready callback
typedef void (*FrameReadyCallback)(void);
FFI_PLUGIN_EXPORT void agus_set_frame_ready_callback(FrameReadyCallback callback);

#ifdef __cplusplus
}
#endif
