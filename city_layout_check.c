#include "city_layout.h"
#include "street_color.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void CheckMapLocation(Vector2 mapPoint)
{
    assert(mapPoint.x >= 0.0f && mapPoint.x <= CITY_LAYOUT_WIDTH);
    assert(mapPoint.y >= 0.0f && mapPoint.y <= CITY_LAYOUT_HEIGHT);
    Vector3 streetPoint = CityLayoutMapPointToStreet(mapPoint);
    Vector2 roundTrip = CityLayoutStreetPointToMap(streetPoint);
    assert(fabsf(roundTrip.x - mapPoint.x) < 0.001f);
    assert(fabsf(roundTrip.y - mapPoint.y) < 0.001f);
}

static int RectanglesOverlap(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

static void CheckRectInCity(Rectangle rectangle)
{
    assert(rectangle.x >= 0.0f && rectangle.y >= 0.0f);
    assert(rectangle.x + rectangle.width <= CITY_LAYOUT_WIDTH);
    assert(rectangle.y + rectangle.height <= CITY_LAYOUT_HEIGHT);
}

static int PointInsideRect(Vector2 point, Rectangle rect)
{
    return point.x > rect.x && point.x < rect.x + rect.width &&
           point.y > rect.y && point.y < rect.y + rect.height;
}

static void CheckRoadGraphConnectedFromHospitalAccess(void)
{
    int visited[CITY_LAYOUT_ROAD_NODE_COUNT] = {0};
    int queue[CITY_LAYOUT_ROAD_NODE_COUNT];
    int head = 0, tail = 0;
    queue[tail++] = CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE;
    visited[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE] = 1;
    while (head < tail) {
        int node = queue[head++];
        for (int edge = 0; edge < CITY_LAYOUT_ROAD_EDGE_COUNT; edge++) {
            int next = -1;
            if (CITY_LAYOUT_ROAD_EDGES[edge].from == node)
                next = CITY_LAYOUT_ROAD_EDGES[edge].to;
            else if (CITY_LAYOUT_ROAD_EDGES[edge].to == node)
                next = CITY_LAYOUT_ROAD_EDGES[edge].from;
            if (next >= 0 && !visited[next]) {
                visited[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    for (int node = 0; node < CITY_LAYOUT_ROAD_NODE_COUNT; node++)
        assert(visited[node]);
}

static void CheckStreetConditionColors(void)
{
    const Color source = {120, 140, 160, 255};
    const Color clear = StreetConditionColor(source, CITY_CLEAR);
    const Color rain = StreetConditionColor(source, CITY_RAIN);
    const Color fogTint = StreetConditionColor(source, CITY_FOG);
    const Color night = StreetConditionColor(source, CITY_NIGHT);
    const Color snowTint = StreetConditionColor(source, CITY_SNOW);
    assert(clear.r == source.r && clear.g == source.g && clear.b == source.b);
    assert(rain.r == 105 && rain.g == 131 && rain.b == 172);
    assert(fogTint.r == source.r && fogTint.g == source.g && fogTint.b == source.b);
    assert(night.r == 54 && night.g == 72 && night.b == 105);
    assert(snowTint.r == source.r && snowTint.g == source.g && snowTint.b == source.b);
    assert(rain.a == source.a && fogTint.a == source.a && night.a == source.a &&
           snowTint.a == source.a);

    // Fog itself does not wash the viewport. Only StreetDistanceColor fades
    // objects with distance, leaving nearby surfaces and responders crisp.
    Color nearFog = StreetDistanceColor(source, (Vector3){0}, (Vector3){0}, CITY_FOG);
    Color middleFog = StreetDistanceColor(source, (Vector3){16, 0, 0},
                                          (Vector3){0}, CITY_FOG);
    Color farFog = StreetDistanceColor(source, (Vector3){30, 0, 0},
                                       (Vector3){0}, CITY_FOG);
    Color rainyDistance = StreetDistanceColor(source, (Vector3){30, 0, 0},
                                              (Vector3){0}, CITY_RAIN);
    assert(nearFog.r == source.r && nearFog.g == source.g && nearFog.b == source.b);
    assert(middleFog.r > nearFog.r && middleFog.r < farFog.r);
    assert(middleFog.g > nearFog.g && middleFog.g < farFog.g);
    assert(farFog.r > nearFog.r && farFog.g > nearFog.g && farFog.b > nearFog.b);
    assert(farFog.r < 148 && farFog.g < 169 && farFog.b < 178);
    assert(rainyDistance.r == rain.r && rainyDistance.g == rain.g &&
           rainyDistance.b == rain.b);
}

int main(void)
{
    CheckStreetConditionColors();
    // Representative destinations in different neighborhoods.
    Vector2 northMarket = {
        CITY_LAYOUT_BUILDINGS[4].x + CITY_LAYOUT_BUILDINGS[4].width * 0.5f,
        CITY_LAYOUT_BUILDINGS[4].y + CITY_LAYOUT_BUILDINGS[4].height * 0.5f
    };
    Vector2 eastWard = {
        CITY_LAYOUT_BUILDINGS[18].x + CITY_LAYOUT_BUILDINGS[18].width * 0.5f,
        CITY_LAYOUT_BUILDINGS[18].y + CITY_LAYOUT_BUILDINGS[18].height * 0.5f
    };
    CheckMapLocation(northMarket);
    CheckMapLocation(eastWard);
    CheckMapLocation(CITY_LAYOUT_HOSPITAL_POSITION);
    CheckMapLocation(CITY_LAYOUT_HOSPITAL_ENTRANCE);
    assert(fabsf(northMarket.x - eastWard.x) > 300.0f);
    assert(fabsf(northMarket.y - eastWard.y) > 200.0f);

    // Regular building order remains stable; the hospital is separate from
    // the generic buildings and clear of the East Ward road edge.
    assert(CITY_LAYOUT_BUILDING_COUNT == 21);
    assert(CITY_LAYOUT_BUILDINGS[19].x == 840.0f &&
           CITY_LAYOUT_BUILDINGS[19].y == 483.0f);
    for (int i = 0; i < CITY_LAYOUT_BUILDING_COUNT; i++) {
        CheckRectInCity(CITY_LAYOUT_BUILDINGS[i]);
        for (int j = i + 1; j < CITY_LAYOUT_BUILDING_COUNT; j++)
            assert(!RectanglesOverlap(CITY_LAYOUT_BUILDINGS[i],
                                      CITY_LAYOUT_BUILDINGS[j]));
    }

    const Rectangle facilities[] = {
        CITY_LAYOUT_FIRE_STATION, CITY_LAYOUT_POLICE_STATION,
        CITY_LAYOUT_HOSPITAL_FOOTPRINT
    };
    for (int facility = 0; facility < 3; facility++) {
        CheckRectInCity(facilities[facility]);
        for (int building = 0; building < CITY_LAYOUT_BUILDING_COUNT; building++)
            assert(!RectanglesOverlap(facilities[facility],
                                      CITY_LAYOUT_BUILDINGS[building]));
        for (int other = facility + 1; other < 3; other++)
            assert(!RectanglesOverlap(facilities[facility], facilities[other]));
    }

    // The receiving point is in the marked ambulance apron, outside the
    // hospital wall and outside the connected road surface.
    Rectangle hospital = CITY_LAYOUT_HOSPITAL_FOOTPRINT;
    assert(CITY_LAYOUT_HOSPITAL_POSITION.x == hospital.x + hospital.width * 0.5f);
    assert(CITY_LAYOUT_HOSPITAL_POSITION.y == hospital.y + hospital.height * 0.5f);
    assert(CITY_LAYOUT_HOSPITAL_DOOR.x == hospital.x + hospital.width);
    assert(CITY_LAYOUT_HOSPITAL_DOOR.y >= hospital.y &&
           CITY_LAYOUT_HOSPITAL_DOOR.y <= hospital.y + hospital.height);
    CheckRectInCity(CITY_LAYOUT_HOSPITAL_BAY);
    assert(!RectanglesOverlap(hospital, CITY_LAYOUT_HOSPITAL_BAY));
    assert(CITY_LAYOUT_HOSPITAL_ENTRANCE.x > hospital.x + hospital.width &&
           CITY_LAYOUT_HOSPITAL_ENTRANCE.x < CITY_LAYOUT_HOSPITAL_BAY.x +
                                                   CITY_LAYOUT_HOSPITAL_BAY.width);
    assert(CITY_LAYOUT_HOSPITAL_ENTRANCE.y >= CITY_LAYOUT_HOSPITAL_BAY.y &&
           CITY_LAYOUT_HOSPITAL_ENTRANCE.y <= CITY_LAYOUT_HOSPITAL_BAY.y +
                                                   CITY_LAYOUT_HOSPITAL_BAY.height);
    Vector2 accessNode =
        CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE];
    float accessLength = hypotf(accessNode.x - CITY_LAYOUT_HOSPITAL_ENTRANCE.x,
                                accessNode.y - CITY_LAYOUT_HOSPITAL_ENTRANCE.y);
    assert(accessLength >= 25.0f && accessLength < 55.0f);
    float roadEdge = accessNode.x - CityLayoutRoadWidth(9) * 0.5f;
    assert(CITY_LAYOUT_HOSPITAL_FOOTPRINT.x +
           CITY_LAYOUT_HOSPITAL_FOOTPRINT.width < roadEdge);
    assert(CITY_LAYOUT_HOSPITAL_BAY.x + CITY_LAYOUT_HOSPITAL_BAY.width <= roadEdge);
    for (int building = 0; building < CITY_LAYOUT_BUILDING_COUNT; building++)
        assert(!RectanglesOverlap(CITY_LAYOUT_HOSPITAL_BAY,
                                  CITY_LAYOUT_BUILDINGS[building]));
    assert(!RectanglesOverlap(CITY_LAYOUT_HOSPITAL_BAY, CITY_LAYOUT_FIRE_STATION));
    assert(!RectanglesOverlap(CITY_LAYOUT_HOSPITAL_BAY, CITY_LAYOUT_POLICE_STATION));

    // Four render-only receiving slots fit inside the bay and keep ambulance
    // icons apart; the simulation route target remains the entrance above.
    Vector2 slots[CITY_LAYOUT_HOSPITAL_BAY_COUNT];
    for (int slot = 0; slot < CITY_LAYOUT_HOSPITAL_BAY_COUNT; slot++) {
        slots[slot] = CityLayoutHospitalArrivalSlot(slot,
                                                    CITY_LAYOUT_HOSPITAL_BAY_COUNT);
        assert(!PointInsideRect(slots[slot], hospital));
        assert(slots[slot].x >= CITY_LAYOUT_HOSPITAL_BAY.x + 10.0f &&
               slots[slot].x <= CITY_LAYOUT_HOSPITAL_BAY.x +
                                    CITY_LAYOUT_HOSPITAL_BAY.width - 10.0f);
        assert(slots[slot].y >= CITY_LAYOUT_HOSPITAL_BAY.y + 5.0f &&
               slots[slot].y <= CITY_LAYOUT_HOSPITAL_BAY.y +
                                    CITY_LAYOUT_HOSPITAL_BAY.height - 5.0f);
        assert(!PointInsideRect(slots[slot], CITY_LAYOUT_BUILDINGS[20]));
        for (int other = slot + 1; other < CITY_LAYOUT_HOSPITAL_BAY_COUNT; other++) {
            Vector2 next = CityLayoutHospitalArrivalSlot(other,
                                                  CITY_LAYOUT_HOSPITAL_BAY_COUNT);
            assert(hypotf(slots[slot].x - next.x, slots[slot].y - next.y) >= 22.0f);
            Rectangle firstIcon = {slots[slot].x - 10.0f, slots[slot].y - 5.0f,
                                   20.0f, 10.0f};
            Rectangle nextIcon = {next.x - 10.0f, next.y - 5.0f, 20.0f, 10.0f};
            assert(!RectanglesOverlap(firstIcon, nextIcon));
        }
    }
    CheckRoadGraphConnectedFromHospitalAccess();

    // Lamp bases remain on sidewalks, clear of lane surfaces, buildings,
    // planted trees, and the routes pedestrians use through the sidewalks.
    const float pedestrianHorizontalRoutes[] = {282.0f, 370.0f};
    const float pedestrianVerticalRoutes[] = {345.0f, 447.0f, 720.0f,
                                               822.0f, 1094.0f};
    const Vector2 trees[] = {
        {68,76}, {119,108}, {92,116}, {131,78},
        {520,185}, {606,222}, {573,181}, {546,225},
        {870,493}, {943,550}
    };
    const float lampBaseRadius = fmaxf(CITY_LAYOUT_LAMP_BASE_RADIUS,
                                       CITY_LAYOUT_LAMP_FOOTING_SIZE * 0.5f);
    const float lampFootprintMapRadius = lampBaseRadius /
                                         CITY_LAYOUT_STREET_SCALE;
    const float lampHeadHalfLength = CITY_LAYOUT_LAMP_HEAD_LENGTH /
                                     (2.0f * CITY_LAYOUT_STREET_SCALE);
    const float lampHeadHalfWidth = CITY_LAYOUT_LAMP_HEAD_WIDTH /
                                    (2.0f * CITY_LAYOUT_STREET_SCALE);
    for (int lamp = 0; lamp < CITY_LAYOUT_STREET_LAMP_COUNT; lamp++) {
        Vector2 point = CITY_LAYOUT_STREET_LAMPS[lamp];
        int onSidewalk = 0;
        for (int sidewalk = 0; sidewalk < CITY_LAYOUT_SIDEWALK_COUNT; sidewalk++)
            if (PointInsideRect(point, CITY_LAYOUT_SIDEWALKS[sidewalk])) {
                Rectangle pavement = CITY_LAYOUT_SIDEWALKS[sidewalk];
                onSidewalk = 1;
                assert(point.x - lampFootprintMapRadius >= pavement.x);
                assert(point.x + lampFootprintMapRadius <=
                       pavement.x + pavement.width || pavement.width == CITY_LAYOUT_WIDTH);
                assert(point.y - lampFootprintMapRadius >= pavement.y);
                assert(point.y + lampFootprintMapRadius <=
                       pavement.y + pavement.height);
            }
        assert(onSidewalk);
        Vector2 headCenter = point;
        Rectangle headFootprint = {headCenter.x - lampHeadHalfLength,
                                   headCenter.y - lampHeadHalfWidth,
                                   lampHeadHalfLength * 2.0f,
                                   lampHeadHalfWidth * 2.0f};
        int headOverSidewalk = 0;
        for (int sidewalk = 0; sidewalk < CITY_LAYOUT_SIDEWALK_COUNT; sidewalk++) {
            Rectangle pavement = CITY_LAYOUT_SIDEWALKS[sidewalk];
            if (headFootprint.x >= pavement.x &&
                headFootprint.x + headFootprint.width <= pavement.x + pavement.width &&
                headFootprint.y >= pavement.y &&
                headFootprint.y + headFootprint.height <= pavement.y + pavement.height)
                headOverSidewalk = 1;
        }
        assert(headOverSidewalk);
        for (int building = 0; building < CITY_LAYOUT_BUILDING_COUNT; building++)
            assert(!RectanglesOverlap(headFootprint, CITY_LAYOUT_BUILDINGS[building]));
        for (int facility = 0; facility < 3; facility++)
            assert(!RectanglesOverlap(headFootprint, facilities[facility]));
        for (int building = 0; building < CITY_LAYOUT_BUILDING_COUNT; building++)
            assert(!CityLayoutPointInsideRect(point, CITY_LAYOUT_BUILDINGS[building],
                                              lampFootprintMapRadius));
        for (int facility = 0; facility < 3; facility++)
            assert(!CityLayoutPointInsideRect(point, facilities[facility],
                                              lampFootprintMapRadius));
        for (int tree = 0; tree < (int)(sizeof(trees) / sizeof(trees[0])); tree++)
            assert(hypotf(point.x - trees[tree].x,
                          point.y - trees[tree].y) > 12.0f);
        for (int route = 0; route < (int)(sizeof(pedestrianHorizontalRoutes) /
                                          sizeof(pedestrianHorizontalRoutes[0])); route++)
            assert(fabsf(point.y - pedestrianHorizontalRoutes[route]) >=
                   lampFootprintMapRadius + 0.135f / CITY_LAYOUT_STREET_SCALE);
        for (int route = 0; route < (int)(sizeof(pedestrianVerticalRoutes) /
                                          sizeof(pedestrianVerticalRoutes[0])); route++)
            assert(fabsf(point.x - pedestrianVerticalRoutes[route]) >= 8.0f);

        // The post must be on the pavement but outside the road lane footprint.
        for (int edge = 0; edge < CITY_LAYOUT_ROAD_EDGE_COUNT; edge++) {
            CityRoadEdge road = CITY_LAYOUT_ROAD_EDGES[edge];
            Vector2 a = CITY_LAYOUT_ROAD_NODES[road.from];
            Vector2 b = CITY_LAYOUT_ROAD_NODES[road.to];
            float halfWidth = CityLayoutRoadWidth(edge) * 0.5f;
            if (fabsf(a.y - b.y) < 0.5f &&
                point.x >= fminf(a.x, b.x) && point.x <= fmaxf(a.x, b.x)) {
                assert(fabsf(point.y - a.y) > halfWidth + 1.5f);
                Rectangle roadSurface = {fminf(a.x, b.x), a.y - halfWidth,
                                         fabsf(b.x - a.x), halfWidth * 2.0f};
                assert(!RectanglesOverlap(headFootprint, roadSurface));
            }
            if (fabsf(a.x - b.x) < 0.5f &&
                point.y >= fminf(a.y, b.y) && point.y <= fmaxf(a.y, b.y)) {
                assert(fabsf(point.x - a.x) > halfWidth + 1.5f);
                Rectangle roadSurface = {a.x - halfWidth, fminf(a.y, b.y),
                                         halfWidth * 2.0f, fabsf(b.y - a.y)};
                assert(!RectanglesOverlap(headFootprint, roadSurface));
            }
        }
    }
    assert(fabsf(CITY_LAYOUT_LAMP_BASE_BOTTOM_Y -
                 CITY_LAYOUT_STREET_GROUND_Y) < 0.001f);
    // DrawCylinderEx endpoints are bottom-to-top. These intervals must
    // physically overlap the scene ground, sidewalk footing and pole/head;
    // they are not center/height-derived placements.
    assert(CITY_LAYOUT_LAMP_BASE_BOTTOM_Y <= CITY_LAYOUT_STREET_GROUND_Y);
    assert(CITY_LAYOUT_LAMP_BASE_TOP_Y > CITY_LAYOUT_SIDEWALK_SURFACE_Y);
    assert(CITY_LAYOUT_LAMP_FOOTING_BOTTOM_Y <= CITY_LAYOUT_SIDEWALK_SURFACE_Y);
    assert(CITY_LAYOUT_LAMP_FOOTING_TOP_Y > CITY_LAYOUT_SIDEWALK_SURFACE_Y);
    assert(CITY_LAYOUT_LAMP_POLE_BOTTOM_Y < CITY_LAYOUT_LAMP_FOOTING_TOP_Y);
    assert(CITY_LAYOUT_LAMP_POLE_BOTTOM_Y < CITY_LAYOUT_LAMP_BASE_TOP_Y);
    assert(CITY_LAYOUT_LAMP_POLE_TOP_Y >= CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y);
    assert(CITY_LAYOUT_LAMP_LENS_BOTTOM_Y < CITY_LAYOUT_LAMP_LENS_TOP_Y);
    assert(CITY_LAYOUT_LAMP_LENS_TOP_Y >= CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y &&
           CITY_LAYOUT_LAMP_LENS_BOTTOM_Y <= CITY_LAYOUT_LAMP_HEAD_TOP_Y);
    assert(CITY_LAYOUT_LAMP_HEAD_TOP_Y > CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y);
    assert(CITY_LAYOUT_LAMP_LENS_TOP_Y > CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y);
    assert(CITY_LAYOUT_LAMP_POLE_TOP_Y > CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y);
    assert(CITY_LAYOUT_LAMP_LENS_BOTTOM_Y < CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y);

    const Vector2 cameraCandidates[] = {
        {90, 180}, {200, 450}, {270, 260}, {620, 605}, {990, 650}
    };
    for (int candidate = 0; candidate <
         (int)(sizeof(cameraCandidates) / sizeof(cameraCandidates[0])); candidate++) {
        Vector2 outside = CityLayoutKeepPointOutsideBuildings(cameraCandidates[candidate], 5.0f);
        for (int obstacle = 0; obstacle < CITY_LAYOUT_BUILDING_COUNT; obstacle++) {
            assert(!CityLayoutPointInsideRect(outside,
                CITY_LAYOUT_BUILDINGS[obstacle], 5.0f));
        }
        for (int obstacle = 0; obstacle < 3; obstacle++) {
            assert(!CityLayoutPointInsideRect(outside, facilities[obstacle], 5.0f));
        }
    }

    // An orbit request through a building must stay on the vehicle's side of
    // that obstruction rather than snapping the camera beyond the wall.
    Vector2 cameraTarget = {771.0f, 326.0f};
    Vector2 blockedCamera = {890.0f, 425.0f};
    assert(!CityLayoutCameraPathClear(cameraTarget, blockedCamera, 7.0f));
    Vector2 safeCamera = CityLayoutSafeCameraPoint(cameraTarget, blockedCamera, 7.0f);
    assert(CityLayoutCameraPointClear(safeCamera, 7.0f));
    assert(CityLayoutCameraPathClear(cameraTarget, safeCamera, 7.0f));

    for (int i = 0; i < CITY_LAYOUT_ROAD_EDGE_COUNT; i++) {
        CityRoadEdge edge = CITY_LAYOUT_ROAD_EDGES[i];
        Vector2 a = CITY_LAYOUT_ROAD_NODES[edge.from];
        Vector2 b = CITY_LAYOUT_ROAD_NODES[edge.to];
        Vector2 aRoundTrip = CityLayoutStreetPointToMap(CityLayoutMapPointToStreet(a));
        Vector2 bRoundTrip = CityLayoutStreetPointToMap(CityLayoutMapPointToStreet(b));
        assert(fabsf(aRoundTrip.x - a.x) < 0.001f);
        assert(fabsf(aRoundTrip.y - a.y) < 0.001f);
        assert(fabsf(bRoundTrip.x - b.x) < 0.001f);
        assert(fabsf(bRoundTrip.y - b.y) < 0.001f);
    }

    // The far East Ward location shown in the report screenshot is connected
    // to the arterial through the explicit x=1065 access street.
    assert(CITY_LAYOUT_ROAD_NODES[9].x == 1065.0f);
    assert(CITY_LAYOUT_ROAD_NODES[9].y == 326.0f);
    assert(CityLayoutRoadWidth(8) == 46.0f);
    assert(CityLayoutRoadWidth(9) == 46.0f);

    puts("street colors, hospital access, lamp footprint/assembly, pedestrian and lane clearance, and camera clearance passed");
    return 0;
}
