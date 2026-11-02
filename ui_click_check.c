#include "ui_layout.h"
#include "city_layout.h"
#include "response_camera.h"
#include "manual_selection.h"
#include "ui_controls.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void CheckWindow(int width, int height)
{
    Rectangle viewport = FitVirtualViewport(width, height, 1600, 900);
    Vector2 virtualPoint = {0};
    const Rectangle controls[] = {
        {22,851,104,36}, {134,851,104,36}, {246,851,96,36},
        {350,851,72,36}, {430,851,100,36}, {538,851,100,36},
        {646,851,112,36}, {766,851,160,36},
        {934,851,118,36}, {1060,851,118,36}, {1186,851,104,36},
        {1298,851,112,36}, {1480,851,98,36},
        ResponseViewButtonBounds(), {730,548,58,34}, {792,548,58,34},
        {854,548,148,34}, {1006,548,96,34}, {20,150,352,80},
        {20,236,352,80}, {20,322,352,80},
        {470,256,680,56}, {470,324,680,56}, {470,392,680,56},
        {470,460,680,56}, {1020,708,140,44}, {390,110,820,680},
        {1224,268,170,34}, {1404,268,170,34},
        {1224,306,170,34}, {1404,306,170,34},
        {1224,360,108,34}, {1340,360,108,34}, {1456,360,118,34},
        {1224,430,350,42}, {1222,326,350,27}, {1222,356,350,27},
        {1507,329,21,21}, {1507,359,21,21}, {1224,600,350,28},
        {470,625,210,54}, {695,625,210,54}, {920,625,210,54},
        {430,175,740,550}, {10,88,372,750}, {392,88,806,750},
        {1208,88,382,750}, {400,130,790,462}, {392,608,806,230}
    };
    for (int i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
        Vector2 intended = {controls[i].x + controls[i].width * 0.5f,
                            controls[i].y + controls[i].height * 0.5f};
        Vector2 screenPoint = {viewport.x + intended.x * viewport.width / 1600.0f,
                               viewport.y + intended.y * viewport.height / 900.0f};
        assert(ScreenToVirtualPoint(screenPoint, viewport, 1600, 900, &virtualPoint));
        assert(fabsf(virtualPoint.x - intended.x) < 0.01f);
        assert(fabsf(virtualPoint.y - intended.y) < 0.01f);
        assert(CheckCollisionPointRec(virtualPoint, controls[i]));
    }
    if (viewport.x > 0.0f)
        assert(!ScreenToVirtualPoint((Vector2){viewport.x - 1.0f, viewport.y + 1.0f},
                                     viewport, 1600, 900, &virtualPoint));
    if (viewport.y > 0.0f)
        assert(!ScreenToVirtualPoint((Vector2){viewport.x + 1.0f, viewport.y - 1.0f},
                                     viewport, 1600, 900, &virtualPoint));

    Rectangle mapPanel = {400, 130, 790, 462};
    float mapScale = fminf(mapPanel.width / (float)CITY_LAYOUT_WIDTH, mapPanel.height / 700.0f);
    Rectangle cityView = {mapPanel.x + (mapPanel.width - (float)CITY_LAYOUT_WIDTH * mapScale) * 0.5f,
                          mapPanel.y + (mapPanel.height - 700.0f * mapScale) * 0.5f,
                          (float)CITY_LAYOUT_WIDTH * mapScale, 700.0f * mapScale};
    Vector2 intendedCity = {550.0f, 350.0f};
    Vector2 mapCanvas = {cityView.x + intendedCity.x * cityView.width / (float)CITY_LAYOUT_WIDTH,
                         cityView.y + intendedCity.y * cityView.height / 700.0f};
    Vector2 mapScreen = {viewport.x + mapCanvas.x * viewport.width / 1600.0f,
                         viewport.y + mapCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(mapScreen, viewport, 1600, 900, &virtualPoint));
    Vector2 roundTrip = {(virtualPoint.x - cityView.x) * (float)CITY_LAYOUT_WIDTH / cityView.width,
                         (virtualPoint.y - cityView.y) * 700.0f / cityView.height};
    assert(fabsf(roundTrip.x - intendedCity.x) < 0.01f);
    assert(fabsf(roundTrip.y - intendedCity.y) < 0.01f);

    Rectangle nextType = NextCallTypeSelectorBounds();
    Rectangle helpViewport = SettingsHelpViewportBounds();
    Rectangle legend = MapLegendBounds();
    assert(!CheckCollisionRecs(ResponseViewButtonBounds(), legend));
    Rectangle nextTypeScreen = {viewport.x + nextType.x * viewport.width / 1600.0f,
                                viewport.y + nextType.y * viewport.height / 900.0f,
                                nextType.width * viewport.width / 1600.0f,
                                nextType.height * viewport.height / 900.0f};
    Vector2 nextTypeCenter = {nextTypeScreen.x + nextTypeScreen.width * 0.5f,
                              nextTypeScreen.y + nextTypeScreen.height * 0.5f};
    assert(ScreenToVirtualPoint(nextTypeCenter, viewport, 1600, 900, &virtualPoint));
    assert(CheckCollisionPointRec(virtualPoint, nextType));
    Vector2 helpCenter = {helpViewport.x + helpViewport.width * 0.5f,
                          helpViewport.y + helpViewport.height * 0.5f};
    Vector2 helpScreen = {viewport.x + helpCenter.x * viewport.width / 1600.0f,
                          viewport.y + helpCenter.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(helpScreen, viewport, 1600, 900, &virtualPoint));
    assert(CheckCollisionPointRec(virtualPoint, helpViewport));
    assert(helpViewport.x >= 390 && helpViewport.x + helpViewport.width <= 1210);
    assert(helpViewport.y >= 110 && helpViewport.y + helpViewport.height <= 790);

    // Camera drag and wheel input use the same converted virtual coordinates
    // and ignore the unit selector/HUD controls drawn over the 3D viewport.
    Rectangle responseViewport = {400, 130, 790, 462};
    Rectangle reserved[] = {
        {412,166,280,30}, {730,548,58,34}, {792,548,58,34},
        {854,548,148,34}, {1006,548,96,34}
    };
    Vector2 cameraCanvas = {720.0f, 400.0f};
    Vector2 cameraScreen = {viewport.x + cameraCanvas.x * viewport.width / 1600.0f,
                            viewport.y + cameraCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(cameraScreen, viewport, 1600, 900, &virtualPoint));
    assert(ResponseCameraPointerAllowed(virtualPoint, responseViewport,
                                        reserved, (int)(sizeof(reserved) / sizeof(reserved[0]))));
    Vector2 controlCanvas = {1030.0f, 560.0f};
    Vector2 controlScreen = {viewport.x + controlCanvas.x * viewport.width / 1600.0f,
                             viewport.y + controlCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(controlScreen, viewport, 1600, 900, &virtualPoint));
    assert(!ResponseCameraPointerAllowed(virtualPoint, responseViewport,
                                        reserved, (int)(sizeof(reserved) / sizeof(reserved[0]))));

    for (int row = 0; row < 2; row++) {
        Rectangle unitRow = ManualVehicleRowBounds(row);
        Rectangle checkbox = ManualVehicleCheckboxBounds(row);
        Vector2 rowCenter = {unitRow.x + unitRow.width * 0.5f,
                             unitRow.y + unitRow.height * 0.5f};
        Vector2 checkCenter = {checkbox.x + checkbox.width * 0.5f,
                               checkbox.y + checkbox.height * 0.5f};
        assert(CheckCollisionPointRec(rowCenter, unitRow));
        assert(CheckCollisionPointRec(checkCenter, unitRow));
        Vector2 screen = {viewport.x + rowCenter.x * viewport.width / 1600.0f,
                          viewport.y + rowCenter.y * viewport.height / 900.0f};
        assert(ScreenToVirtualPoint(screen, viewport, 1600, 900, &virtualPoint));
        assert(CheckCollisionPointRec(virtualPoint, unitRow));
    }

    const Rectangle queue = {10, 88, 372, 750};
    const Rectangle map = {392, 88, 806, 750};
    const Rectangle details = {1208, 88, 382, 750};
    const Rectangle clip = {400, 130, 790, 462};
    const Rectangle roster = {392, 608, 806, 230};
    const Rectangle toolbar = {18, 842, 1564, 48};
    assert(queue.x + queue.width < map.x);
    assert(map.x + map.width < details.x);
    assert(clip.x >= map.x && clip.y >= map.y);
    assert(clip.x + clip.width <= map.x + map.width);
    assert(clip.y + clip.height < roster.y);
    assert(roster.x + roster.width <= map.x + map.width);
    assert(roster.y + roster.height <= map.y + map.height);
    assert(toolbar.y + toolbar.height <= 900);
    Rectangle responseReset = {1006, 548, 96, 34};
    assert(responseReset.x + responseReset.width <= clip.x + clip.width);
    assert(responseReset.y + responseReset.height <= clip.y + clip.height);
}

static void CheckManualUnitRows(void)
{
    Simulation simulation;
    SimulationInit(&simulation);
    simulation.dispatchMode = DISPATCH_MANUAL;
    int incidentIndex = SimulationCreateIncident(&simulation, INCIDENT_FIRE,
                                                (Vector2){700, 300});
    assert(incidentIndex >= 0);
    Incident *incident = &simulation.incidents[incidentIndex];
    incident->requiredUnitsByService[INCIDENT_FIRE] = 2;
    incident->requiredUnitsByService[INCIDENT_CRIME] = 0;
    incident->requiredUnitsByService[INCIDENT_ACCIDENT] = 0;
    incident->requiredUnits = 2;
    incident->totalRequiredUnits = 2;

    int vehicleRows[MANUAL_VEHICLE_ROW_COUNT];
    int rowCount = ManualVehicleRows(&simulation, incident, vehicleRows);
    assert(rowCount >= 2);
    unsigned int mask = 0;
    Rectangle first = ManualVehicleRowBounds(0);
    Vector2 firstClick = {first.x + first.width * 0.5f,
                          first.y + first.height * 0.5f};
    mask = ManualVehicleToggleSelection(&simulation, incident, incidentIndex,
                                        0, firstClick, mask);
    assert(mask == (1u << vehicleRows[0]));
    Rectangle second = ManualVehicleRowBounds(1);
    Vector2 secondClick = {second.x + second.width * 0.5f,
                           second.y + second.height * 0.5f};
    mask = ManualVehicleToggleSelection(&simulation, incident, incidentIndex,
                                        1, secondClick, mask);
    assert(mask == ((1u << vehicleRows[0]) | (1u << vehicleRows[1])));
    mask = ManualVehicleToggleSelection(&simulation, incident, incidentIndex,
                                        0, firstClick, mask);
    assert(mask == (1u << vehicleRows[1]));
    mask = ManualVehicleToggleSelection(&simulation, incident, incidentIndex,
                                        1, secondClick, mask);
    assert(mask == 0);
}

int main(void)
{
    // Keep the same shared manual hit-test helpers linked into this geometry
    // check; selection semantics themselves are covered by dispatch checks.
    int (*classifyHit)(const Simulation *, int, Vector2) = ManualClassifyButtonHit;
    unsigned int (*toggleSelection)(const Simulation *, const Incident *, int,
                                    int, Vector2, unsigned int) = ManualVehicleToggleSelection;
    (void)classifyHit;
    (void)toggleSelection;
    assert(IncidentTypeForNumberKey(KEY_ONE) == INCIDENT_FIRE);
    assert(IncidentTypeForNumberKey(KEY_TWO) == INCIDENT_CRIME);
    assert(IncidentTypeForNumberKey(KEY_THREE) == INCIDENT_ACCIDENT);
    assert(IncidentTypeForNumberKey(KEY_FOUR) == INCIDENT_MEDICAL);
    assert(CycleNextCallType(INCIDENT_MEDICAL) == INCIDENT_FIRE);
    assert(CycleNextCallType(INCIDENT_FIRE) == INCIDENT_CRIME);
    assert(SETTINGS_HELP_LINE_COUNT > SETTINGS_HELP_VISIBLE_LINES);
    assert(SettingsHelpScrollByWheel(0, 1.0f) == 0);
    assert(SettingsHelpScrollByWheel(0, -1.0f) == 1);
    assert(SettingsHelpScrollByWheel(SETTINGS_HELP_LINE_COUNT, -1.0f) ==
           SETTINGS_HELP_LINE_COUNT - SETTINGS_HELP_VISIBLE_LINES);
    CheckWindow(1120, 700);
    CheckWindow(1440, 810);
    CheckWindow(1920, 1080);
    CheckManualUnitRows();
    puts("resized-window click mapping and manual multi-unit selection checks passed");
    return 0;
}
