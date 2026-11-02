#ifndef CITY_LAYOUT_H
#define CITY_LAYOUT_H

#include "raylib.h"
#include <math.h>

// Shared city-space geometry. The 2D map, route planner, and Street View all
// use these same coordinates (x east/west, y north/south).
#define CITY_LAYOUT_WIDTH 1150
#define CITY_LAYOUT_HEIGHT 700
#define CITY_LAYOUT_STREET_SCALE 0.045f
#define CITY_LAYOUT_ROAD_NODE_COUNT 11
#define CITY_LAYOUT_ROAD_EDGE_COUNT 10
#define CITY_LAYOUT_BUILDING_COUNT 21
#define CITY_LAYOUT_SIDEWALK_COUNT 8
#define CITY_LAYOUT_CROSSWALK_LEFT_OFFSET (-61)
#define CITY_LAYOUT_CROSSWALK_RIGHT_OFFSET 41
#define CITY_LAYOUT_CROSSWALK_UP_OFFSET (-64)
#define CITY_LAYOUT_CROSSWALK_DOWN_OFFSET 37
#define CITY_LAYOUT_CROSSWALK_HALF_WIDTH 33
#define CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE 10
#define CITY_LAYOUT_HOSPITAL_BAY_COUNT 4
#define CITY_LAYOUT_STREET_LAMP_COUNT 4
#define CITY_LAYOUT_SIDEWALK_SURFACE_Y 0.03f
#define CITY_LAYOUT_LAMP_BASE_BOTTOM_Y (-0.08f)
#define CITY_LAYOUT_LAMP_BASE_TOP_Y 0.20f
#define CITY_LAYOUT_LAMP_POLE_BOTTOM_Y (-0.10f)
#define CITY_LAYOUT_LAMP_POLE_TOP_Y 3.18f
#define CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y 3.04f
#define CITY_LAYOUT_LAMP_HEAD_TOP_Y 3.34f
#define CITY_LAYOUT_LAMP_LENS_BOTTOM_Y 2.99f
#define CITY_LAYOUT_LAMP_LENS_TOP_Y 3.06f
#define CITY_LAYOUT_LAMP_BASE_RADIUS 0.10f
#define CITY_LAYOUT_LAMP_FOOTING_SIZE 0.20f
#define CITY_LAYOUT_LAMP_FOOTING_BOTTOM_Y 0.00f
#define CITY_LAYOUT_LAMP_FOOTING_TOP_Y 0.12f
#define CITY_LAYOUT_STREET_GROUND_Y (-0.08f)
#define CITY_LAYOUT_LAMP_POLE_RADIUS 0.095f
#define CITY_LAYOUT_LAMP_HEAD_LENGTH 0.70f
#define CITY_LAYOUT_LAMP_HEAD_WIDTH 0.18f

typedef struct {
    int from;
    int to;
} CityRoadEdge;

static const Vector2 CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_ROAD_NODE_COUNT] = {
    {0, 326}, {396, 326}, {771, 326}, {1150, 326},
    {396, 80}, {396, 630}, {771, 80}, {771, 630},
    {1065, 80}, {1065, 326}, {1065, 630}
};

static const CityRoadEdge CITY_LAYOUT_ROAD_EDGES[CITY_LAYOUT_ROAD_EDGE_COUNT] = {
    {0, 1}, {1, 2}, {2, 9}, {9, 3},
    {1, 4}, {1, 5}, {2, 6}, {2, 7}, {9, 8}, {9, 10}
};

static inline float CityLayoutRoadWidth(int edgeIndex)
{
    if (edgeIndex < 0 || edgeIndex >= CITY_LAYOUT_ROAD_EDGE_COUNT) return 72.0f;
    // Narrow East Ward access road: two lanes fit with a safe vehicle margin.
    if (edgeIndex >= 8) return 46.0f;
    CityRoadEdge edge = CITY_LAYOUT_ROAD_EDGES[edgeIndex];
    Vector2 from = CITY_LAYOUT_ROAD_NODES[edge.from];
    Vector2 to = CITY_LAYOUT_ROAD_NODES[edge.to];
    return fabsf(to.x - from.x) >= fabsf(to.y - from.y) ? 72.0f : 82.0f;
}

// Keep order stable: fire cascade/scenario fields refer to these indexes.
static const Rectangle CITY_LAYOUT_BUILDINGS[CITY_LAYOUT_BUILDING_COUNT] = {
    {48,145,92,82}, {158,112,145,66}, {169,190,110,49},
    {462,108,92,54}, {565,105,119,52}, {464,174,35,72}, {640,176,47,64},
    {839,113,88,100}, {943,116,99,83}, {843,216,88,37},
    {48,410,100,110}, {163,389,122,82}, {164,484,120,81},
    {463,390,111,76}, {586,389,101,83}, {467,482,92,86}, {570,489,116,73},
    {839,394,91,63}, {944,393,99,101}, {840,483,87,83}, {948,516,95,69}
};

// These pavement strips are the sidewalks/access strips drawn by the 2D map.
static const Rectangle CITY_LAYOUT_SIDEWALKS[CITY_LAYOUT_SIDEWALK_COUNT] = {
    {0,274,CITY_LAYOUT_WIDTH,16}, {0,362,CITY_LAYOUT_WIDTH,16},
    {339,80,16,550}, {437,80,16,550}, {714,80,16,550}, {812,80,16,550},
    {1032,80,10,550}, {1088,80,12,550}
};

// Service buildings use the same map-space locations as their 2D landmarks.
static const Rectangle CITY_LAYOUT_FIRE_STATION = { 240,242,54,32 };
static const Rectangle CITY_LAYOUT_POLICE_STATION = { 590,590,62,32 };
// The hospital sits in the open southeast corner, clear of regular buildings.
// Ambulances route to its receiving entrance, connected by a short spur to
// East Ward road endpoint node 10. These coordinates are shared by the map,
// Street View, transport routing, and layout checks.
static const Rectangle CITY_LAYOUT_HOSPITAL_FOOTPRINT = { 936,620,80,64 };
static const Vector2 CITY_LAYOUT_HOSPITAL_POSITION = { 976,652 };
// Ambulances stop in the marked receiving apron, directly outside the east
// entrance. The apron fills the gap between the hospital and the road edge.
static const Rectangle CITY_LAYOUT_HOSPITAL_BAY = { 1016,610,26,80 };
static const Vector2 CITY_LAYOUT_HOSPITAL_DOOR = { 1016,650 };
static const Vector2 CITY_LAYOUT_HOSPITAL_ENTRANCE = { 1029,650 };

// Streetlights stand on the same pavement strips shown by the 2D map.
static const Vector2 CITY_LAYOUT_STREET_LAMPS[CITY_LAYOUT_STREET_LAMP_COUNT] = {
    {180,276.3f}, {580,287.7f}, {930,276.3f}, {1000,375.7f}
};

// Render-only spacing for the short hospital receiving stop. The simulation
// still routes every ambulance to CITY_LAYOUT_HOSPITAL_ENTRANCE.
static inline Vector2 CityLayoutHospitalArrivalSlot(int ordinal, int count)
{
    if (count < 1) count = 1;
    if (count > CITY_LAYOUT_HOSPITAL_BAY_COUNT) count = CITY_LAYOUT_HOSPITAL_BAY_COUNT;
    if (ordinal < 0) ordinal = 0;
    if (ordinal >= count) ordinal = count - 1;
    float centered = (float)ordinal - ((float)count - 1.0f) * 0.5f;
    return (Vector2) {
        CITY_LAYOUT_HOSPITAL_ENTRANCE.x,
            CITY_LAYOUT_HOSPITAL_ENTRANCE.y + centered * 22.0f
    };
}

static inline int CityLayoutPointInsideRect(Vector2 point, Rectangle rect,
    float clearance)
{
    return point.x > rect.x - clearance &&
        point.x < rect.x + rect.width + clearance &&
        point.y > rect.y - clearance &&
        point.y < rect.y + rect.height + clearance;
}

// Keep the Street View camera out of building footprints while preserving its
// height and direction. A small clearance prevents camera clipping at walls.
static inline Vector2 CityLayoutKeepPointOutsideBuildings(Vector2 point,
    float clearance)
{
    Rectangle facilities[] = {
        CITY_LAYOUT_FIRE_STATION, CITY_LAYOUT_POLICE_STATION,
        CITY_LAYOUT_HOSPITAL_FOOTPRINT
    };
    for (int pass = 0; pass < 2; pass++) {
        for (int index = 0; index < CITY_LAYOUT_BUILDING_COUNT + 3; index++) {
            Rectangle rect = index < CITY_LAYOUT_BUILDING_COUNT ?
                CITY_LAYOUT_BUILDINGS[index] :
                facilities[index - CITY_LAYOUT_BUILDING_COUNT];
            if (!CityLayoutPointInsideRect(point, rect, clearance)) continue;
            float left = point.x - (rect.x - clearance);
            float right = rect.x + rect.width + clearance - point.x;
            float top = point.y - (rect.y - clearance);
            float bottom = rect.y + rect.height + clearance - point.y;
            float nearest = fminf(fminf(left, right), fminf(top, bottom));
            if (nearest == left) point.x = rect.x - clearance;
            else if (nearest == right) point.x = rect.x + rect.width + clearance;
            else if (nearest == top) point.y = rect.y - clearance;
            else point.y = rect.y + rect.height + clearance;
        }
    }
    if (point.x < 0.5f) point.x = 0.5f;
    if (point.x > CITY_LAYOUT_WIDTH - 0.5f) point.x = CITY_LAYOUT_WIDTH - 0.5f;
    if (point.y < 0.5f) point.y = 0.5f;
    if (point.y > CITY_LAYOUT_HEIGHT - 0.5f) point.y = CITY_LAYOUT_HEIGHT - 0.5f;
    return point;
}

static inline int CityLayoutCameraPointClear(Vector2 point, float clearance)
{
    if (point.x < 0.0f || point.x > CITY_LAYOUT_WIDTH ||
        point.y < 0.0f || point.y > CITY_LAYOUT_HEIGHT) return 0;
    for (int index = 0; index < CITY_LAYOUT_BUILDING_COUNT + 3; index++) {
        Rectangle rect = index < CITY_LAYOUT_BUILDING_COUNT ?
            CITY_LAYOUT_BUILDINGS[index] :
            (Rectangle[]) {
            CITY_LAYOUT_FIRE_STATION,
                CITY_LAYOUT_POLICE_STATION,
                CITY_LAYOUT_HOSPITAL_FOOTPRINT
        }
        [index - CITY_LAYOUT_BUILDING_COUNT];
        if (CityLayoutPointInsideRect(point, rect, clearance)) return 0;
    }
    return 1;
}

static inline int CityLayoutCameraPathClear(Vector2 target, Vector2 camera,
    float clearance)
{
    // Sample in map space so a camera cannot be pushed to the far side of a
    // wall while still aiming at the selected vehicle through that building.
    for (int step = 1; step <= 32; step++) {
        float t = (float)step / 32.0f;
        Vector2 point = { target.x + (camera.x - target.x) * t,
                         target.y + (camera.y - target.y) * t };
        if (!CityLayoutCameraPointClear(point, clearance)) return 0;
    }
    return 1;
}

static inline Vector2 CityLayoutSafeCameraPoint(Vector2 target, Vector2 desired,
    float clearance)
{
    Vector2 offset = { desired.x - target.x, desired.y - target.y };
    float baseAngle = atan2f(offset.y, offset.x);
    float baseDistance = sqrtf(offset.x * offset.x + offset.y * offset.y);
    if (baseDistance < 0.01f) return desired;

    // Prefer the requested orbit. If a building blocks it, search nearby
    // angles and progressively closer positions, keeping the camera on the
    // visible side of the street instead of snapping it across a wall.
    for (int radius = 0; radius <= 6; radius++) {
        int variants = radius == 0 ? 1 : 2;
        for (int side = 0; side < variants; side++) {
            float sign = side == 0 ? 1.0f : -1.0f;
            float angle = baseAngle + sign * (float)radius * 12.0f * DEG2RAD;
            for (int distanceStep = 0; distanceStep < 6; distanceStep++) {
                float scale = 1.0f - (float)distanceStep * 0.09f;
                Vector2 candidate = { target.x + cosf(angle) * baseDistance * scale,
                                     target.y + sinf(angle) * baseDistance * scale };
                if (CityLayoutCameraPathClear(target, candidate, clearance))
                    return candidate;
            }
        }
    }
    // In the rare case that a vehicle is itself at a building footprint, keep
    // the camera in city bounds and outside the structure rather than failing.
    return CityLayoutKeepPointOutsideBuildings(desired, clearance);
}

static inline Vector3 CityLayoutMapPointToStreet(Vector2 point)
{
    return (Vector3) {
        (point.x - CITY_LAYOUT_WIDTH * 0.5f)* CITY_LAYOUT_STREET_SCALE,
            0.0f,
            (point.y - CITY_LAYOUT_HEIGHT * 0.5f)* CITY_LAYOUT_STREET_SCALE
    };
}

static inline Vector2 CityLayoutStreetPointToMap(Vector3 point)
{
    return (Vector2) {
        point.x / CITY_LAYOUT_STREET_SCALE + CITY_LAYOUT_WIDTH * 0.5f,
            point.z / CITY_LAYOUT_STREET_SCALE + CITY_LAYOUT_HEIGHT * 0.5f
    };
}

#endif
