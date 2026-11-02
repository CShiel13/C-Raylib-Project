#include "raylib.h"

#include "incidents.h"

#include "simulation.h"
#include "ui_layout.h"
#include "ui_controls.h"
#include "manual_selection.h"
#include "manual_dispatch.h"
#include "response_selection.h"
#include "response_camera.h"
#include "city_layout.h"
#include "street_color.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>



#define SCREEN_WIDTH 1440
#define SCREEN_HEIGHT 810
#define UI_WIDTH 1600
#define UI_HEIGHT 900
#define UI_RENDER_SCALE 2
#define CITY_WIDTH CITY_LAYOUT_WIDTH
#define CITY_HEIGHT CITY_LAYOUT_HEIGHT
#define QUEUE_VISIBLE 8

typedef enum {
    OVERLAY_NONE = 0,
    OVERLAY_REPORTS,
    OVERLAY_SETTINGS
} UIOverlay;


static const Rectangle queuePanel = {10, 88, 372, 750};
static const Rectangle mapPanel = {392, 88, 806, 750};
static const Rectangle detailsPanel = {1208, 88, 382, 750};
static const Rectangle mapClip = {400, 130, 790, 462};
static const Rectangle responseViewButton = {1012, 94, 178, 32};
static const Rectangle responsePreviousButton = {730, 548, 58, 34};
static const Rectangle responseNextButton = {792, 548, 58, 34};
static const Rectangle responseCameraButton = {854, 548, 148, 34};
static const Rectangle responseCameraResetButton = {1006, 548, 96, 34};
static Rectangle ResponseUnitRowBounds(int row)
{
    return (Rectangle){mapClip.x + 12, mapClip.y + 36 + row * 34, 280, 30};
}
static const float queueRowsY = 150.0f;
static const float queueRowHeight = 86.0f;
static Rectangle QueueCardBounds(int row)
{
    return (Rectangle){20.0f, queueRowsY + row * queueRowHeight, 352.0f, 80.0f};
}

static void DrawStreetOrientedBox(Vector3 center, Vector3 forward,
                                  float length, float height, float width,
                                  Color color);

static int HospitalArrivalSlotForVehicle(const Simulation *simulation, int vehicleIndex,
                                         Vector2 *slot)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount)
        return 0;
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    if (vehicle->serviceType != INCIDENT_ACCIDENT ||
        fabsf(vehicle->position.x - CITY_LAYOUT_HOSPITAL_ENTRANCE.x) > 0.5f ||
        fabsf(vehicle->position.y - CITY_LAYOUT_HOSPITAL_ENTRANCE.y) > 0.5f) return 0;

    int rank = 0;
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *other = &simulation->vehicles[i];
        if (other->serviceType != INCIDENT_ACCIDENT ||
            fabsf(other->position.x - CITY_LAYOUT_HOSPITAL_ENTRANCE.x) > 0.5f ||
            fabsf(other->position.y - CITY_LAYOUT_HOSPITAL_ENTRANCE.y) > 0.5f) continue;
        if (i < vehicleIndex) rank++;
        count++;
    }
    if (slot) *slot = CityLayoutHospitalArrivalSlot(rank, count);
    return 1;
}

static Vector2 MapVehicleDisplayPosition(const Simulation *simulation, int vehicleIndex)
{
    Vector2 slot;
    if (HospitalArrivalSlotForVehicle(simulation, vehicleIndex, &slot)) return slot;
    return simulation->vehicles[vehicleIndex].position;
}

static const Rectangle compareButton = {22, 851, 104, 36};
static const Rectangle pauseButton = {134, 851, 104, 36};
static const Rectangle slowButton = {246, 851, 96, 36};
static const Rectangle normalButton = {350, 851, 72, 36};
static const Rectangle fastButton = {430, 851, 100, 36};
static const Rectangle stepButton = {538, 851, 100, 36};
static const Rectangle dispatchButton = {646, 851, 112, 36};
static const Rectangle trafficButton = {766, 851, 160, 36};
static const Rectangle dispatchModeButton = {934, 851, 118, 36};
static const Rectangle simulateButton = {1060, 851, 118, 36};
static const Rectangle reportsButton = {1186, 851, 104, 36};
static const Rectangle settingsButton = {1298, 851, 112, 36};
static const Rectangle resetButton = {1480, 851, 98, 36};
static const Rectangle dispatchSelectedButton = {1224, 600, 350, 28};
static const Rectangle callTypeButtons[4] = {
    {1224, 268, 170, 34}, {1404, 268, 170, 34},
    {1224, 306, 170, 34}, {1404, 306, 170, 34}
};
static const Rectangle callPriorityButtons[3] = {
    {1224, 360, 108, 34}, {1340, 360, 108, 34}, {1456, 360, 118, 34}
};
static const Rectangle modalPanel = {470, 210, 660, 490};
static const Rectangle modalCloseButton = {1000, 638, 100, 38};
// Settings use the roomier panel spacing from the supplied polished UI copy.
static const Rectangle settingsPanel = {390, 110, 820, 680};
static const Rectangle settingsCloseButton = {1020, 708, 140, 44};
static const Rectangle routesToggleButton = {470, 256, 680, 56};
static const Rectangle civilianToggleButton = {470, 324, 680, 56};
static const Rectangle cityConditionButton = {470, 392, 680, 56};
static const Rectangle autoIncidentsButton = {470, 460, 680, 56};
static const Rectangle operatorModalPanel = {430, 175, 740, 550};
static const Rectangle operatorFireButton = {470, 625, 210, 54};
static const Rectangle operatorPoliceButton = {695, 625, 210, 54};
static const Rectangle operatorAmbulanceButton = {920, 625, 210, 54};

static Font uiFont;
static int uiFontLoaded;

static void DrawUiText(const char *text, int x, int y, int fontSize, Color color)
{
    if (uiFontLoaded)
        DrawTextEx(uiFont, text, (Vector2){(float)x, (float)y},
                   (float)fontSize, 0.0f, color);
    else
        DrawText(text, x, y, fontSize, color);
}

#define DrawText DrawUiText

static int MeasureUiText(const char *text, int fontSize)
{
    if (uiFontLoaded)
        return (int)ceilf(MeasureTextEx(uiFont, text, (float)fontSize, 0.0f).x);
    return MeasureText(text, fontSize);
}

static void LoadUiFont(void)
{
    static const char *fontPaths[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    uiFont = GetFontDefault();
    uiFontLoaded = 0;
    for (int i = 0; i < (int)(sizeof(fontPaths) / sizeof(fontPaths[0])); i++) {
        if (!FileExists(fontPaths[i])) continue;
        Font candidate = LoadFontEx(fontPaths[i], 48, NULL, 0);
        if (candidate.texture.id == 0) continue;
        uiFont = candidate;
        uiFontLoaded = 1;
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
        return;
    }
}

static void UnloadUiFont(void)
{
    if (uiFontLoaded) UnloadFont(uiFont);
    uiFontLoaded = 0;
}

static int IsIncidentFinished(const Incident *incident);
static int CallIncidentIndex(const Simulation *simulation, int callIndex);
static int DrawWrappedText(const char *text, int x, int y, int fontSize,
                           int maxCharacters, int lineHeight, Color color);
static const char *PriorityLabel(int severity);

static int SelectedResponseIncident(const Simulation *simulation, int selectedCall,
                                    int selectedIncident)
{
    if (selectedIncident >= 0 && selectedIncident < simulation->incidentCount)
        return selectedIncident;
    int callIncident = CallIncidentIndex(simulation, selectedCall);
    return callIncident >= 0 && callIncident < simulation->incidentCount ? callIncident : -1;
}

static void DrawButton(Rectangle rect, const char *label, Color color);
static void DrawPanel(Rectangle rect);
static int ServiceUnitNumber(const Simulation *simulation, int vehicleIndex);

static int FindPendingOverrideCall(const Simulation *simulation)
{
    if (!simulation || simulation->dispatchMode != DISPATCH_AUTOMATIC) return -1;
    for (int i = 0; i < simulation->callCount; i++) {
        const EmergencyCall *call = &simulation->calls[i];
        if (call->overridePending && !call->classified && !call->dispatched) return i;
    }
    return -1;
}

static void StartPendingOverrideModal(const Simulation *simulation, bool *paused,
                                     bool *resumeWhenDone, int *modalCall,
                                     double *modalDeadline)
{
    if (!simulation || !paused || !resumeWhenDone || !modalCall || !modalDeadline ||
        *modalCall >= 0) return;
    int next = FindPendingOverrideCall(simulation);
    if (next < 0) return;
    *resumeWhenDone = !*paused;
    *paused = true;
    *modalCall = next;
    *modalDeadline = GetTime() + 12.0;
}

static IncidentType OperatorServiceChoice(int choice, IncidentType aiType)
{
    if (choice == 0) return INCIDENT_FIRE;
    if (choice == 1) return INCIDENT_CRIME;
    if (choice == 2)
        return aiType == INCIDENT_ACCIDENT || aiType == INCIDENT_MEDICAL ?
               aiType : INCIDENT_MEDICAL;
    return aiType;
}

static void AcceptOperatorOverride(Simulation *simulation, int callIndex,
                                   IncidentType acceptedType, int severity,
                                   bool *paused, bool resumeWhenDone,
                                   int *modalCall, double *modalDeadline)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount) return;
    EmergencyCall *call = &simulation->calls[callIndex];
    call->overridePending = 0;
    call->operatorOverride = 1;
    call->operatorChoiceType = acceptedType;
    call->operatorWrongService =
        SimulationServiceForIncident(call->trueType) !=
        SimulationServiceForIncident(acceptedType);
    int classified = SimulationClassifyCall(simulation, callIndex, acceptedType, severity);
    if (classified) SimulationProcessOperator(simulation);
    if (modalCall) *modalCall = -1;
    if (modalDeadline) *modalDeadline = 0.0;
    if (paused) *paused = !resumeWhenDone;
    StartPendingOverrideModal(simulation, paused, &resumeWhenDone, modalCall, modalDeadline);
}

static void UpdateCallerFeedbackToast(Simulation *simulation, char *notice,
                                      size_t noticeSize, int *noticeMatched,
                                      double *noticeUntil)
{
    if (!simulation || !notice || !noticeMatched || !noticeUntil) return;
    for (int i = 0; i < simulation->callCount; i++) {
        EmergencyCall *call = &simulation->calls[i];
        if (!call->feedbackReady || call->feedbackShown) continue;
        call->feedbackShown = 1;
        *noticeMatched = call->callerSatisfied;
        snprintf(notice, noticeSize, "CALLER %s  |  CALL #%02d %s.",
                 call->callerSatisfied ? "SATISFIED" : "DISSATISFIED", i + 1,
                 call->incidentIndex >= 0 && call->incidentIndex < simulation->incidentCount &&
                 simulation->incidents[call->incidentIndex].status == INCIDENT_RESOLVED ?
                 "RESOLVED" : "FAILED");
        *noticeUntil = GetTime() + 3.5;
        break;
    }
}

static void DrawOperatorOverrideModal(const Simulation *simulation, int callIndex,
                                      double deadline)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount) return;
    const EmergencyCall *call = &simulation->calls[callIndex];
    IncidentType suggestedType;
    int suggestedSeverity;
    if (!SimulationSuggestCallClassification(simulation, callIndex,
                                               &suggestedType, &suggestedSeverity)) return;
    DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){0, 0, 0, 205});
    DrawPanel(operatorModalPanel);
    DrawRectangleRounded((Rectangle){450, 192, 700, 58}, 0.12f, 8,
                         (Color){13, 48, 69, 255});
    DrawText("INCOMING CALL", 475, 207, 24, RAYWHITE);
    double remaining = deadline - GetTime();
    if (remaining < 0.0) remaining = 0.0;
    int seconds = (int)ceil(remaining);
    Color urgency = seconds <= 3 ? (Color){255, 77, 84, 255} :
                    seconds <= 6 ? (Color){255, 178, 63, 255} :
                    (Color){104, 208, 245, 255};
    DrawRectangleRounded((Rectangle){953, 198, 167, 46}, 0.18f, 7,
                         (Color){23, 35, 48, 255});
    DrawText(TextFormat("TIME  %02d", seconds), 974, 210, 20, urgency);

    DrawText(TextFormat("CALL #%02d  |  RECEIVED %.1f  |  LOCATION %.0f, %.0f",
                        callIndex + 1, call->receivedAt,
                        call->location.x, call->location.y), 480, 272, 13,
             (Color){143, 207, 232, 255});
    DrawText("CALLER REPORT", 480, 310, 12, (Color){119, 192, 224, 255});
    DrawRectangleRounded((Rectangle){470, 330, 660, 105}, 0.08f, 6,
                         (Color){8, 25, 39, 255});
    DrawWrappedText(call->report, 490, 348, 17, 68, 24, RAYWHITE);
    DrawText("AI OPERATOR SUGGESTION", 480, 458, 12,
             (Color){119, 192, 224, 255});
    const char *typeName = suggestedType == INCIDENT_FIRE ? "FIRE" :
                           suggestedType == INCIDENT_CRIME ? "POLICE" :
                           suggestedType == INCIDENT_ACCIDENT ? "TRAFFIC ACCIDENT" : "MEDICAL";
    DrawText(TextFormat("%s  |  %s PRIORITY  |  %d UNIT%s REQUIRED",
                        typeName, PriorityLabel(suggestedSeverity),
                        GetRequiredUnits(suggestedType, suggestedSeverity),
                        GetRequiredUnits(suggestedType, suggestedSeverity) == 1 ? "" : "S"),
             480, 481, 17, RAYWHITE);
    DrawText("Choose a service. If time expires, AUTO OP applies its suggestion.",
             480, 525, 12, (Color){164, 186, 198, 255});
    DrawButton(operatorFireButton, "FIRE", (Color){157, 52, 59, 255});
    DrawButton(operatorPoliceButton, "POLICE", (Color){33, 104, 164, 255});
    DrawButton(operatorAmbulanceButton, "AMBULANCE", (Color){25, 132, 105, 255});
}

// Draw recognizable vector symbols without depending on platform emoji fonts.
static void DrawIncidentIcon(IncidentType type, Vector2 center, float size, Color color)
{
    switch (type) {
        case INCIDENT_FIRE:
            DrawCircle((int)(center.x - size * 0.34f), (int)(center.y + size * 0.34f),
                       size * 0.48f, color);
            DrawCircle((int)(center.x + size * 0.34f), (int)(center.y + size * 0.34f),
                       size * 0.48f, color);
            DrawTriangle((Vector2){center.x, center.y - size},
                         (Vector2){center.x - size * 0.72f, center.y + size * 0.65f},
                         (Vector2){center.x + size * 0.72f, center.y + size * 0.65f}, color);
            DrawTriangle((Vector2){center.x, center.y - size * 0.22f},
                         (Vector2){center.x - size * 0.27f, center.y + size * 0.56f},
                         (Vector2){center.x + size * 0.27f, center.y + size * 0.56f},
                         (Color){255, 225, 130, 255});
            break;
        case INCIDENT_CRIME: {
            Vector2 topLeft = {center.x - size * 0.72f, center.y - size * 0.62f};
            Vector2 topRight = {center.x + size * 0.72f, center.y - size * 0.62f};
            Vector2 bottom = {center.x, center.y + size};
            DrawRectangle((int)topLeft.x, (int)topLeft.y, (int)(size * 1.44f),
                          (int)(size * 0.75f), color);
            DrawTriangle(topLeft, topRight, bottom, color);
            DrawLineEx((Vector2){center.x - size * 0.43f, center.y - size * 0.3f},
                       (Vector2){center.x, center.y + size * 0.65f}, size * 0.12f, RAYWHITE);
            DrawLineEx((Vector2){center.x + size * 0.43f, center.y - size * 0.3f},
                       (Vector2){center.x, center.y + size * 0.65f}, size * 0.12f, RAYWHITE);
            DrawLineEx((Vector2){center.x - size * 0.42f, center.y - size * 0.15f},
                       (Vector2){center.x + size * 0.42f, center.y - size * 0.15f}, size * 0.12f, RAYWHITE);
            break;
        }
        case INCIDENT_ACCIDENT:
            DrawRectangleRounded((Rectangle){center.x - size, center.y - size * 0.32f,
                                 size * 2, size * 0.68f}, 0.25f, 4, color);
            DrawTriangle((Vector2){center.x - size * 0.55f, center.y - size * 0.3f},
                         (Vector2){center.x - size * 0.23f, center.y - size * 0.78f},
                         (Vector2){center.x + size * 0.42f, center.y - size * 0.3f}, color);
            DrawCircleV((Vector2){center.x - size * 0.55f, center.y + size * 0.48f},
                        size * 0.23f, (Color){35, 43, 52, 255});
            DrawCircleV((Vector2){center.x + size * 0.55f, center.y + size * 0.48f},
                        size * 0.23f, (Color){35, 43, 52, 255});
            DrawLineEx((Vector2){center.x + size * 0.8f, center.y - size * 0.8f},
                       (Vector2){center.x + size * 1.2f, center.y - size * 1.2f}, size * 0.16f,
                       (Color){255, 214, 90, 255});
            DrawLineEx((Vector2){center.x + size * 1.1f, center.y - size * 0.65f},
                       (Vector2){center.x + size * 1.55f, center.y - size * 0.65f}, size * 0.16f,
                       (Color){255, 214, 90, 255});
            break;
        case INCIDENT_MEDICAL: {
            DrawCircleV(center, size, (Color){255, 255, 255, 245});
            float arm = size * 0.28f;
            DrawRectangle((int)(center.x - arm * 0.5f), (int)(center.y - size * 0.62f),
                          (int)arm, (int)(size * 1.24f), color);
            DrawRectangle((int)(center.x - size * 0.62f), (int)(center.y - arm * 0.5f),
                          (int)(size * 1.24f), (int)arm, color);
            break;
        }
        default:
            DrawCircleV(center, size, color);
            break;
    }
}

static void DrawActiveIncidentEffect(const Incident *incident, int index, float time,
                                     CityCondition condition)
{
    if (IsIncidentFinished(incident)) return;
    float phase = time * 5.0f + (float)index * 1.71f;
    float pulse = 0.5f + 0.5f * sinf(phase);
    if (incident->type == INCIDENT_FIRE) {
        int smokeCount = incident->fireContained ? 1 : 2 + incident->severity;
        float smokeHeight = incident->fireContained ? 26.0f : 42.0f;
        for (int puff = 0; puff < smokeCount; puff++) {
            float rise = fmodf(time * (incident->fireContained ? 12.0f : 24.0f) +
                               index * 13.0f + puff * 19.0f, smokeHeight);
            float wind = condition == CITY_WIND ? 16.0f : 0.0f;
            float drift = sinf(phase + (float)puff * 1.8f) * 5.0f +
                          wind * rise / smokeHeight;
            DrawCircleV((Vector2){incident->position.x - 5.0f + drift,
                                  incident->position.y - 24.0f - rise},
                        3.5f + (float)puff, Fade((Color){171, 182, 185, 255},
                            (incident->fireContained ? 0.13f : 0.30f) *
                            (1.0f - rise / smokeHeight)));
        }
        float flameScale = incident->fireContained ? 0.38f : 1.0f;
        float flameHeight = (10.0f + pulse * 7.0f + incident->severity * 2.0f) * flameScale;
        DrawTriangle((Vector2){incident->position.x, incident->position.y + 19.0f - flameHeight},
                     (Vector2){incident->position.x - 7.0f, incident->position.y + 17.0f},
                     (Vector2){incident->position.x + 7.0f, incident->position.y + 17.0f},
                     Fade(ORANGE, 0.85f));
        DrawTriangle((Vector2){incident->position.x, incident->position.y + 18.0f - flameHeight * 0.62f},
                     (Vector2){incident->position.x - 3.5f, incident->position.y + 16.0f},
                     (Vector2){incident->position.x + 3.5f, incident->position.y + 16.0f},
                     Fade((Color){255, 226, 132, 255}, 0.9f));
    } else if (incident->type == INCIDENT_ACCIDENT) {
        DrawCircleLines((int)incident->position.x, (int)incident->position.y,
                        14.0f + pulse * 5.0f, Fade(ORANGE, 0.55f));
        Vector2 warning = {incident->position.x + 16.0f, incident->position.y - 12.0f};
        DrawTriangle((Vector2){warning.x, warning.y - 8.0f},
                     (Vector2){warning.x - 7.0f, warning.y + 6.0f},
                     (Vector2){warning.x + 7.0f, warning.y + 6.0f}, ORANGE);
        DrawText("!", (int)warning.x - 2, (int)warning.y - 4, 9, (Color){31, 36, 39, 255});
    } else if (incident->type == INCIDENT_MEDICAL) {
        DrawCircleLines((int)incident->position.x, (int)incident->position.y,
                        13.0f + pulse * 8.0f,
                        Fade((Color){88, 235, 190, 255}, 0.35f + pulse * 0.4f));
    }
}

// Stable per-particle seeds keep weather repeatable without growing arrays or
// allocating anything in the frame loop.
static float WeatherSeed(unsigned int index, unsigned int salt)
{
    unsigned int value = index * 374761393u + salt * 668265263u + 0x9e3779b9u;
    value = (value ^ (value >> 13)) * 1274126177u;
    value ^= value >> 16;
    return (float)(value & 0x00ffffffu) / 16777215.0f;
}

static void DrawCityWeather(const Simulation *simulation)
{
    if (simulation->cityCondition == CITY_NIGHT) {
        DrawRectangle(0, 0, CITY_WIDTH, CITY_HEIGHT, (Color){4, 10, 28, 82});
        for (int i = 0; i < 15; i++) {
            int x = 42 + (i * 71) % 1000;
            int y = 120 + (i * 43) % 470;
            DrawCircle(x, y, 2.0f, (Color){255, 220, 123, 150});
        }
    } else if (simulation->cityCondition == CITY_FOG) {
        // A few thin, low-alpha ground-hugging bands suggest fog on the 2D map
        // without bleaching incidents, buildings, or vehicles.
        for (int band = 0; band < 4; band++) {
            float y = 125.0f + band * 151.0f +
                      sinf(simulation->elapsedTime * 0.22f + (float)band) * 8.0f;
            DrawRectangle(0, (int)y, CITY_WIDTH, 14,
                          (Color){173, 194, 203, 12});
        }
    } else if (simulation->cityCondition == CITY_RAIN) {
        for (unsigned int drop = 0; drop < 82; drop++) {
            float x = WeatherSeed(drop, 1u) * (float)CITY_WIDTH;
            float y = fmodf(WeatherSeed(drop, 2u) * (float)CITY_HEIGHT +
                            simulation->elapsedTime * 92.0f, (float)CITY_HEIGHT);
            // Dark edge plus bright core keeps the streak visible on both
            // pale roofs and dark roads.
            DrawLineEx((Vector2){x, y}, (Vector2){x - 5.0f, y + 10.0f}, 2.0f,
                       (Color){19, 42, 57, 115});
            DrawLineEx((Vector2){x + 0.6f, y}, (Vector2){x - 4.4f, y + 10.0f},
                       0.9f, (Color){220, 238, 248, 210});
        }
    } else if (simulation->cityCondition == CITY_WIND) {
        for (int streak = 0; streak < 8; streak++) {
            int y = 78 + streak * 67;
            int x = (int)fmodf((float)(streak * 127) + simulation->elapsedTime * 30.0f,
                               (float)CITY_WIDTH);
            DrawLineEx((Vector2){(float)x, (float)y}, (Vector2){(float)x + 22.0f, (float)y - 2.0f},
                       1.4f, (Color){171, 211, 229, 150});
        }
    } else if (simulation->cityCondition == CITY_SNOW) {
        for (unsigned int flake = 0; flake < 58; flake++) {
            float seedX = WeatherSeed(flake, 3u);
            float x = fmodf(seedX * (float)CITY_WIDTH +
                            sinf(simulation->elapsedTime * 0.9f + (float)flake) * 7.0f +
                            (float)CITY_WIDTH, (float)CITY_WIDTH);
            float y = fmodf(WeatherSeed(flake, 4u) * (float)CITY_HEIGHT +
                            simulation->elapsedTime * (15.0f + WeatherSeed(flake, 5u) * 9.0f),
                            (float)CITY_HEIGHT);
            float radius = 1.1f + WeatherSeed(flake, 6u) * 1.2f;
            DrawCircle((int)x, (int)y, radius + 0.7f,
                       (Color){26, 53, 70, 155});
            DrawCircle((int)x, (int)y, radius, (Color){226, 240, 249, 225});
        }
    }
}

static void DrawEventFeed(const Simulation *simulation)
{
    Rectangle feedPanel = {1220, 752, 354, 78};
    DrawRectangleRounded(feedPanel, 0.035f, 8, (Color){11, 29, 43, 245});
    DrawRectangleRoundedLinesEx(feedPanel, 0.035f, 8, 1, (Color){35, 69, 89, 255});
    DrawText("LIVE EVENT FEED", 1232, 757, 9, (Color){107, 190, 238, 255});
    DrawText(TextFormat("%d / %d", simulation->eventCount, MAX_SIMULATION_EVENTS),
             1536, 759, 7, (Color){135, 161, 178, 255});
    DrawLine(1232, 771, 1562, 771, (Color){43, 67, 83, 255});
    if (simulation->eventCount == 0) {
        DrawText("Waiting for simulation events", 1232, 779, 8,
                 (Color){145, 166, 180, 255});
        return;
    }
    int shown = simulation->eventCount < 3 ? simulation->eventCount : 3;
    for (int row = 0; row < shown; row++) {
        int eventIndex = simulation->eventCount - 1 - row;
        const SimulationEvent *event = &simulation->events[eventIndex];
        int y = 775 + row * 17;
        DrawCircle(1236, y + 5, 2, row == 0 ? SKYBLUE : (Color){86, 126, 148, 255});
        DrawText(TextFormat("%.0fs", event->time), 1243, y, 6,
                 (Color){128, 163, 183, 255});
        DrawText(TextFormat("%.53s", event->message), 1271, y, 6,
                 (Color){205, 218, 226, 255});
    }
}

static void DrawVehicleIcon(IncidentType service, Vector2 center)
{
    Color body = service == INCIDENT_FIRE ? (Color){210, 49, 65, 255} :
                 service == INCIDENT_CRIME ? (Color){35, 112, 220, 255} :
                 RAYWHITE;
    DrawRectangleRounded((Rectangle){center.x - 10, center.y - 5, 20, 10}, 0.25f, 4, body);
    DrawRectangle((int)(center.x - 4), (int)(center.y - 3), 7, 4,
                  service == INCIDENT_MEDICAL ? (Color){170, 215, 233, 255} :
                  (Color){194, 224, 240, 255});
    DrawCircle((int)(center.x - 6), (int)(center.y + 5), 2.2f, (Color){38, 45, 54, 255});
    DrawCircle((int)(center.x + 6), (int)(center.y + 5), 2.2f, (Color){38, 45, 54, 255});

    if (service == INCIDENT_FIRE) {
        DrawLineEx((Vector2){center.x - 7, center.y - 5},
                   (Vector2){center.x + 8, center.y - 5}, 1.3f, (Color){255, 211, 91, 255});
        DrawLineEx((Vector2){center.x - 5, center.y - 7},
                   (Vector2){center.x + 6, center.y - 7}, 1.0f, (Color){255, 211, 91, 255});
    } else if (service == INCIDENT_CRIME) {
        DrawRectangle((int)(center.x - 2), (int)(center.y - 7), 4, 2,
                      (Color){255, 72, 84, 255});
        DrawRectangle((int)(center.x + 2), (int)(center.y - 7), 4, 2,
                      (Color){81, 172, 255, 255});
    } else {
        DrawRectangle((int)(center.x - 1), (int)(center.y - 3), 2, 6, RED);
        DrawRectangle((int)(center.x - 3), (int)(center.y - 1), 6, 2, RED);
    }
    DrawRectangleLinesEx((Rectangle){center.x - 10, center.y - 5, 20, 10},
                         1.0f, (Color){25, 38, 50, 255});
}

static void DrawCivilianCar(const CivilianCar *car)
{
    Color body = car->yieldTime > 0.0f ? (Color){237, 171, 76, 255} :
                 (Color){164, 180, 191, 255};
    DrawRectanglePro((Rectangle){car->position.x - 7, car->position.y - 4, 14, 8},
                     (Vector2){7, 4}, car->headingDegrees, body);
    DrawRectanglePro((Rectangle){car->position.x - 3, car->position.y - 2, 6, 4},
                     (Vector2){3, 2}, car->headingDegrees,
                     (Color){205, 222, 230, 255});
}

static void DrawPedestrian(const Pedestrian *person, float time)
{
    float bob = sinf(time * 6.0f + person->position.x * 0.03f) * 1.2f;
    Color shirt = person->state == PEDESTRIAN_EVACUATING ? (Color){247, 145, 75, 255} :
                  person->state == PEDESTRIAN_BYSTANDING ? (Color){90, 204, 230, 255} :
                  person->state == PEDESTRIAN_RESPECTING_CORDON ? (Color){112, 167, 242, 255} :
                  person->state == PEDESTRIAN_YIELDING ? (Color){255, 216, 105, 255} :
                  person->state == PEDESTRIAN_CROSSING ||
                  person->state == PEDESTRIAN_WAITING_TO_CROSS ? (Color){229, 240, 241, 255} :
                  (Color){177, 202, 211, 255};
    float sidestep = person->state == PEDESTRIAN_YIELDING ?
                     (person->sidewalk < 3 ? -3.5f : 3.5f) : 0.0f;
    if (person->state != PEDESTRIAN_WALKING)
        DrawCircleLines((int)person->position.x, (int)person->position.y,
                        person->state == PEDESTRIAN_BYSTANDING ? 6.0f : 4.5f,
                        Fade(shirt, 0.75f));
    DrawCircle((int)person->position.x, (int)(person->position.y - 3 + bob), 2.2f,
               (Color){220, 183, 148, 255});
    DrawLineEx((Vector2){person->position.x + sidestep, person->position.y - 1 + bob},
               (Vector2){person->position.x + sidestep, person->position.y + 4 + bob}, 2.2f, shirt);
    DrawLineEx((Vector2){person->position.x + sidestep, person->position.y + 4 + bob},
               (Vector2){person->position.x - 2 + sidestep, person->position.y + 7 + bob}, 1.2f, shirt);
    DrawLineEx((Vector2){person->position.x + sidestep, person->position.y + 4 + bob},
               (Vector2){person->position.x + 2 + sidestep, person->position.y + 7 + bob}, 1.2f, shirt);
}



static void DrawCityBuilding(Rectangle rect, int style)
{
    const Color roofs[] = {
        {56, 79, 92, 255}, {66, 87, 98, 255}, {50, 74, 87, 255},
        {72, 86, 92, 255}, {58, 82, 88, 255}
    };
    Color roof = roofs[style % 5];
    DrawRectangleRounded((Rectangle){rect.x + 3, rect.y + 4, rect.width, rect.height},
                         0.04f, 4, (Color){17, 35, 44, 105});
    DrawRectangleRounded(rect, 0.04f, 4, (Color){37, 58, 67, 255});
    DrawRectangleRounded((Rectangle){rect.x + 4, rect.y + 4, rect.width - 8,
                                     rect.height - 8}, 0.04f, 4, roof);
    DrawRectangle((int)rect.x + 9, (int)rect.y + 9, (int)rect.width - 18, 2,
                  (Color){112, 131, 136, 150});

    // Use a small, regular rooftop pattern rather than scattered decoration.
    if (rect.width > 66 && rect.height > 48) {
        DrawRectangleRounded((Rectangle){rect.x + 13, rect.y + 18, 14, 9},
                             0.12f, 3, (Color){36, 55, 63, 255});
        DrawRectangleRounded((Rectangle){rect.x + 33, rect.y + 18, 14, 9},
                             0.12f, 3, (Color){36, 55, 63, 255});
        DrawRectangle((int)rect.x + 16, (int)rect.y + 20, 8, 2,
                      (Color){122, 141, 146, 160});
        DrawRectangle((int)rect.x + 36, (int)rect.y + 20, 8, 2,
                      (Color){122, 141, 146, 160});
    } else {
        DrawRectangleRounded((Rectangle){rect.x + 9, rect.y + 15, 12, 8},
                             0.12f, 3, (Color){36, 55, 63, 255});
    }
}

static void DrawCityTree(Vector2 p)
{
    DrawCircle((int)p.x + 2, (int)p.y + 3, 7, (Color){18, 38, 39, 180});
    DrawCircle((int)p.x, (int)p.y, 7, (Color){50, 105, 78, 255});
    DrawCircle((int)p.x - 2, (int)p.y - 2, 4, (Color){76, 133, 91, 255});
    DrawCircleLines((int)p.x, (int)p.y, 7, (Color){101, 151, 103, 190});
}

static void DrawCrosswalkHorizontal(int x, int y)
{
    // Vertical bars across the east-west roadway, kept outside the junction box.
    for (int stripe = 0; stripe < 5; stripe++)
        DrawRectangle(x + stripe * 5, y, 3, 66, (Color){203, 212, 209, 175});
}

static void DrawCrosswalkVertical(int x, int y)
{
    // Horizontal bars across the north-south roadway, kept outside the junction box.
    for (int stripe = 0; stripe < 5; stripe++)
        DrawRectangle(x, y + stripe * 5, 66, 3, (Color){203, 212, 209, 175});
}

static void DrawServiceLandmark(Rectangle r, const char *label, Color color)
{
    DrawRectangleRounded(r, 0.08f, 5, (Color){23, 50, 62, 255});
    DrawRectangleRoundedLinesEx(r, 0.08f, 5, 1.2f, color);
    DrawRectangle((int)r.x + 7, (int)r.y + 7, 9, 8, color);
    DrawRectangle((int)r.x + 21, (int)r.y + 7, 9, 8, color);
    DrawText(label, (int)r.x + 4, (int)r.y + r.height - 11, 7, RAYWHITE);
}

static void DrawHospitalMapLandmark(void)
{
    Rectangle bay = CITY_LAYOUT_HOSPITAL_BAY;
    Rectangle hospital = CITY_LAYOUT_HOSPITAL_FOOTPRINT;
    DrawRectangleRec(bay, (Color){54, 71, 78, 255});
    DrawRectangleLinesEx(bay, 1.5f, (Color){127, 157, 160, 255});
    for (int stripe = 0; stripe < 3; stripe++) {
        int y = (int)bay.y + 8 + stripe * 20;
        DrawLineEx((Vector2){bay.x + 3, (float)y},
                   (Vector2){bay.x + bay.width - 3, (float)y},
                   1.3f, (Color){191, 206, 205, 210});
    }
    DrawLineEx(CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE],
               CITY_LAYOUT_HOSPITAL_ENTRANCE, 10.0f, (Color){68, 82, 91, 255});
    DrawLineEx(CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE],
               CITY_LAYOUT_HOSPITAL_ENTRANCE, 1.2f, (Color){139, 151, 153, 220});

    DrawRectangleRounded(hospital, 0.08f, 5, (Color){32, 61, 70, 255});
    DrawRectangleRoundedLinesEx(hospital, 0.08f, 5, 1.6f,
                                (Color){83, 210, 181, 255});
    DrawRectangle((int)hospital.x + 5, (int)hospital.y + 5,
                  (int)hospital.width - 10, 8, (Color){72, 103, 111, 255});
    for (int window = 0; window < 3; window++)
        DrawRectangle((int)hospital.x + 9 + window * 19,
                      (int)hospital.y + 23, 12, 12,
                      (Color){107, 173, 191, 255});

    // Entrance faces the receiving apron at the footprint's east edge.
    DrawRectangle((int)CITY_LAYOUT_HOSPITAL_DOOR.x - 4,
                  (int)CITY_LAYOUT_HOSPITAL_DOOR.y - 8, 6, 16,
                  (Color){213, 229, 225, 255});
    DrawRectangle((int)CITY_LAYOUT_HOSPITAL_DOOR.x - 3,
                  (int)CITY_LAYOUT_HOSPITAL_DOOR.y - 6, 3, 12,
                  (Color){55, 99, 113, 255});
    DrawRectangle((int)hospital.x + 9, (int)hospital.y + 39, 8, 14,
                  (Color){226, 236, 231, 255});
    DrawRectangle((int)hospital.x + 12, (int)hospital.y + 40, 2, 12, RED);
    DrawRectangle((int)hospital.x + 10, (int)hospital.y + 45, 6, 2, RED);
    DrawText("HOSPITAL", (int)hospital.x + 23,
             (int)hospital.y + 41, 8, RAYWHITE);
    DrawLineEx(CITY_LAYOUT_HOSPITAL_DOOR, CITY_LAYOUT_HOSPITAL_ENTRANCE,
               2.0f, (Color){134, 219, 194, 255});
    DrawCircleLines((int)CITY_LAYOUT_HOSPITAL_ENTRANCE.x,
                    (int)CITY_LAYOUT_HOSPITAL_ENTRANCE.y, 5,
                    (Color){128, 236, 203, 255});
}

static int CountOnSceneService(const Simulation *simulation, int incidentIndex,
                              IncidentType serviceType)
{
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *unit = &simulation->vehicles[i];
        if (unit->assignedIncident == incidentIndex && unit->serviceType == serviceType &&
            unit->arrived && unit->activity == UNIT_ON_SCENE) count++;
    }
    return count;
}

static int HasRequiredOnSceneService(const Simulation *simulation, int incidentIndex,
                                     IncidentType serviceType)
{
    const Incident *incident = &simulation->incidents[incidentIndex];
    int required = incident->requiredUnitsByService[serviceType];
    return required > 0 && CountOnSceneService(simulation, incidentIndex, serviceType) >= required;
}

static void DrawResponderFigure(Vector2 p, Color color, const char *label)
{
    DrawCircle((int)p.x, (int)p.y - 4, 2.0f, (Color){237, 205, 174, 255});
    DrawLineEx((Vector2){p.x, p.y - 2}, (Vector2){p.x, p.y + 3}, 2.3f, color);
    DrawLineEx((Vector2){p.x, p.y}, (Vector2){p.x - 3, p.y + 2}, 1.4f, color);
    DrawLineEx((Vector2){p.x, p.y}, (Vector2){p.x + 3, p.y + 2}, 1.4f, color);
    DrawText(label, (int)p.x - 18, (int)p.y + 5, 6, color);
}

static void DrawCity(const Simulation *simulation)
{
    DrawRectangle(0, 0, CITY_WIDTH, CITY_HEIGHT, (Color){34, 53, 61, 255});

    // Keep the neighborhood background quiet so road markings, routes, and
    // incident symbols remain the strongest map signals.

    // Small green spaces, paths, and planted trees break up the built areas.
    DrawRectangleRounded((Rectangle){45, 56, 100, 72}, 0.08f, 6, (Color){36, 79, 64, 255});
    DrawLineEx((Vector2){54, 112}, (Vector2){129, 69}, 3, (Color){117, 133, 98, 255});
    DrawCityTree((Vector2){68, 76}); DrawCityTree((Vector2){119, 108});
    DrawCityTree((Vector2){92, 116}); DrawCityTree((Vector2){131, 78});
    DrawRectangleRounded((Rectangle){500, 169, 130, 72}, 0.08f, 6, (Color){36, 79, 64, 255});
    DrawLineEx((Vector2){509, 230}, (Vector2){618, 180}, 3, (Color){117, 133, 98, 255});
    DrawCityTree((Vector2){520, 185}); DrawCityTree((Vector2){606, 222});
    DrawCityTree((Vector2){573, 181}); DrawCityTree((Vector2){546, 225});
    DrawRectangleRounded((Rectangle){850, 470, 125, 100}, 0.08f, 6, (Color){36, 79, 64, 255});
    DrawLineEx((Vector2){862, 555}, (Vector2){958, 484}, 3, (Color){117, 133, 98, 255});
    DrawCityTree((Vector2){870, 493}); DrawCityTree((Vector2){943, 550});

    // Parking courts and low-rise blocks in each neighborhood quadrant.
    DrawRectangleRounded((Rectangle){160, 58, 155, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    DrawRectangleRounded((Rectangle){463, 52, 222, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    DrawRectangleRounded((Rectangle){851, 58, 183, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    DrawRectangleRounded((Rectangle){55, 583, 230, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    DrawRectangleRounded((Rectangle){465, 588, 225, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    DrawRectangleRounded((Rectangle){844, 583, 190, 42}, 0.04f, 4, (Color){49, 64, 70, 255});
    for (int i = 0; i < 7; i++) {
        DrawLine(170 + i * 19, 62, 170 + i * 19, 96, (Color){129, 145, 145, 150});
        DrawLine(65 + i * 28, 587, 65 + i * 28, 621, (Color){129, 145, 145, 150});
    }

    const Rectangle *buildings = CITY_LAYOUT_BUILDINGS;
    const int buildingCount = CITY_LAYOUT_BUILDING_COUNT;
    for (int i = 0; i < buildingCount; i++) {
        DrawCityBuilding(buildings[i], i);
        if (simulation->apartmentFireDrill && i == 4) {
            const Incident *fire = simulation->incidentCount ? &simulation->incidents[0] : NULL;
            const EmergencyCall *waitingFireCall = NULL;
            DrawText("APARTMENT BLOCK", (int)buildings[i].x + 7,
                     (int)buildings[i].y + 5, 6, (Color){139, 196, 219, 255});
            if (!fire && simulation->callCount > 0 &&
                simulation->calls[0].trueType == INCIDENT_FIRE &&
                simulation->calls[0].incidentIndex < 0)
                waitingFireCall = &simulation->calls[0];
            if (waitingFireCall) {
                int litWindows = waitingFireCall->trueSeverity * 2;
                for (int room = 0; room < litWindows; room++) {
                    int col = room % 3;
                    int row = room / 3;
                    Rectangle window = {buildings[i].x + 13 + col * 32,
                                        buildings[i].y + 22 + row * 14, 18, 9};
                    float flicker = 0.5f + 0.5f * sinf(simulation->elapsedTime * 8.0f + room);
                    DrawRectangleRounded(window, 0.12f, 3,
                        (Color){(unsigned char)(190 + flicker * 65.0f),
                                (unsigned char)(68 + flicker * 72.0f), 20, 255});
                }
                DrawText("NEW FIRE CALL", (int)buildings[i].x + 6,
                         (int)buildings[i].y + 53, 6, ORANGE);
            }
            if (fire && fire->type == INCIDENT_FIRE) {
                int rooms = fire->fireRoomsBurning;
                if (rooms < 1) rooms = 1;
                if (rooms > APARTMENT_FIRE_ROOM_COUNT) rooms = APARTMENT_FIRE_ROOM_COUNT;
                for (int room = 0; room < APARTMENT_FIRE_ROOM_COUNT; room++) {
                    int col = room % 3;
                    int row = room / 3;
                    Rectangle window = {buildings[i].x + 13 + col * 32,
                                        buildings[i].y + 22 + row * 14, 18, 9};
                    if (fire->status == INCIDENT_RESOLVED && room < rooms) {
                        DrawRectangleRounded(window, 0.12f, 3, (Color){35, 33, 31, 255});
                        continue;
                    }
                    if (room >= rooms || fire->fireContained || IsIncidentFinished(fire))
                        continue;
                    float flicker = 0.5f + 0.5f * sinf(simulation->elapsedTime * 8.0f + room * 1.7f);
                    DrawRectangleRounded(window, 0.12f, 3,
                        (Color){(unsigned char)(190 + flicker * 65.0f),
                                (unsigned char)(68 + flicker * 72.0f), 20, 255});
                    DrawRectangle((int)window.x + 5, (int)window.y + 2, 8, 4,
                                  (Color){255, 221, 127, 230});
                }
                if (!IsIncidentFinished(fire))
                    DrawText(TextFormat("FIRE FRONT  %d / %d ROOMS", rooms,
                                        APARTMENT_FIRE_ROOM_COUNT),
                             (int)buildings[i].x + 6, (int)buildings[i].y + 53,
                             6, fire->fireContained ? GREEN : ORANGE);
            }
            if (fire && fire->status == INCIDENT_RESOLVED) {
                DrawRectangle((int)buildings[i].x + 5, (int)buildings[i].y + 5,
                              (int)buildings[i].width - 10, (int)buildings[i].height - 10,
                              (Color){22, 25, 28, 145});
                DrawLineEx((Vector2){buildings[i].x + 22, buildings[i].y + 17},
                           (Vector2){buildings[i].x + 46, buildings[i].y + 37}, 2,
                           (Color){15, 20, 25, 230});
                DrawText("DAMAGED", (int)buildings[i].x + 36,
                         (int)buildings[i].y + 37, 7, (Color){230, 173, 110, 255});
            }
        }
        if (simulation->apartmentFireDrill && i != 4) {
            const Incident *fire = NULL;
            for (int fireIndex = 0; fireIndex < simulation->incidentCount; fireIndex++) {
                const Incident *candidate = &simulation->incidents[fireIndex];
                if (candidate->type == INCIDENT_FIRE && candidate->fireBuildingIndex == i) {
                    fire = candidate;
                    break;
                }
            }
            const EmergencyCall *waitingFireCall = NULL;
            if (!fire) {
                for (int callIndex = 0; callIndex < simulation->callCount; callIndex++) {
                    const EmergencyCall *candidate = &simulation->calls[callIndex];
                    if (candidate->trueType == INCIDENT_FIRE &&
                        candidate->fireBuildingIndex == i &&
                        (candidate->incidentIndex < 0 ||
                         !IsIncidentFinished(&simulation->incidents[candidate->incidentIndex]))) {
                        waitingFireCall = candidate;
                        break;
                    }
                }
            }
            if (waitingFireCall) {
                DrawRectangleLinesEx(buildings[i], 2.0f, ORANGE);
                float flicker = 4.0f + 2.0f * sinf(simulation->elapsedTime * 7.0f + i);
                for (int flame = 0; flame < 3; flame++) {
                    float x = buildings[i].x + 8.0f + flame * 14.0f;
                    float y = buildings[i].y + 15.0f + (flame % 2) * 14.0f;
                    DrawTriangle((Vector2){x + 4, y - flicker}, (Vector2){x, y + 4},
                                 (Vector2){x + 8, y + 4}, ORANGE);
                }
                DrawText("NEW FIRE CALL", (int)buildings[i].x + 3,
                         (int)buildings[i].y + buildings[i].height - 10, 5, ORANGE);
            }
            if (fire && fire->status == INCIDENT_RESOLVED) {
                DrawRectangle((int)buildings[i].x + 4, (int)buildings[i].y + 4,
                              (int)buildings[i].width - 8, (int)buildings[i].height - 8,
                              (Color){28, 27, 26, 125});
                DrawText("DAMAGE", (int)buildings[i].x + 5,
                         (int)buildings[i].y + buildings[i].height - 10,
                         5, (Color){230, 173, 110, 255});
            } else if (fire && !IsIncidentFinished(fire) && !fire->fireContained) {
                DrawRectangleLinesEx(buildings[i], 2.0f, ORANGE);
                float flicker = 4.0f + 2.5f * sinf(simulation->elapsedTime * 7.0f + i);
                int flames = fire->severity >= 3 ? 3 : fire->severity + 1;
                for (int room = 0; room < flames; room++) {
                    float x = buildings[i].x + 8.0f + room * 14.0f;
                    float y = buildings[i].y + 15.0f + (room % 2) * 14.0f;
                    DrawTriangle((Vector2){x + 4, y - flicker},
                                 (Vector2){x, y + 4}, (Vector2){x + 8, y + 4}, ORANGE);
                }
                DrawText("SPREAD FIRE", (int)buildings[i].x + 4,
                         (int)buildings[i].y + buildings[i].height - 10, 5, ORANGE);
            } else if (fire && !IsIncidentFinished(fire)) {
                DrawRectangleLinesEx(buildings[i], 2.0f, GREEN);
                DrawText("CONTAINED", (int)buildings[i].x + 3,
                         (int)buildings[i].y + buildings[i].height - 10, 5, GREEN);
            }
        }
    }

    // Minor access roads and sidewalks sit beneath the connected main roads.
    for (int i = 0; i < CITY_LAYOUT_SIDEWALK_COUNT; i++) {
        Rectangle sidewalk = CITY_LAYOUT_SIDEWALKS[i];
        DrawRectangleRec(sidewalk, (Color){87, 102, 105, 255});
    }

    // Main arterial geometry matches the road nodes and routing graph exactly.
    for (int edge = 0; edge < CITY_LAYOUT_ROAD_EDGE_COUNT; edge++) {
        Vector2 from = CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_ROAD_EDGES[edge].from];
        Vector2 to = CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_ROAD_EDGES[edge].to];
        float width = CityLayoutRoadWidth(edge);
        DrawLineEx(from, to, width, (Color){68, 82, 91, 255});
    }
    Vector2 firstJunction = CITY_LAYOUT_ROAD_NODES[1];
    Vector2 secondJunction = CITY_LAYOUT_ROAD_NODES[2];
    DrawLine(0, (int)firstJunction.y - 32, CITY_WIDTH, (int)firstJunction.y - 32,
             (Color){139, 151, 153, 210});
    DrawLine(0, (int)firstJunction.y + 32, CITY_WIDTH, (int)firstJunction.y + 32,
             (Color){139, 151, 153, 210});
    DrawLine((int)firstJunction.x - 37, 80, (int)firstJunction.x - 37, 630,
             (Color){139, 151, 153, 210});
    DrawLine((int)firstJunction.x + 37, 80, (int)firstJunction.x + 37, 630,
             (Color){139, 151, 153, 210});
    DrawLine((int)secondJunction.x - 37, 80, (int)secondJunction.x - 37, 630,
             (Color){139, 151, 153, 210});
    DrawLine((int)secondJunction.x + 37, 80, (int)secondJunction.x + 37, 630,
             (Color){139, 151, 153, 210});
    Vector2 eastWardJunction = CITY_LAYOUT_ROAD_NODES[9];
    DrawLine((int)eastWardJunction.x - 23, 80, (int)eastWardJunction.x - 23, 630,
             (Color){139, 151, 153, 210});
    DrawLine((int)eastWardJunction.x + 23, 80, (int)eastWardJunction.x + 23, 630,
             (Color){139, 151, 153, 210});

    // Painted center lines distinguish opposing traffic directions.
    for (int x = 0; x < CITY_WIDTH; x += 40) {
        if (abs(x - (int)firstJunction.x) <= 76 ||
            abs(x - (int)secondJunction.x) <= 76) continue;
        DrawRectangle(x, (int)firstJunction.y - 2, 23, 3, (Color){222, 190, 111, 235});
        DrawRectangle(x, (int)firstJunction.y + 3, 23, 2, (Color){222, 190, 111, 150});
    }
    for (int y = 80; y < 630; y += 40) {
        if (abs(y - (int)firstJunction.y) <= 46) continue;
        DrawRectangle((int)firstJunction.x - 2, y, 3, 23, (Color){222, 190, 111, 235});
        DrawRectangle((int)firstJunction.x + 3, y, 2, 23, (Color){222, 190, 111, 150});
        DrawRectangle((int)secondJunction.x - 2, y, 3, 23, (Color){222, 190, 111, 235});
        DrawRectangle((int)secondJunction.x + 3, y, 2, 23, (Color){222, 190, 111, 150});
        if (abs(y - (int)eastWardJunction.y) > 44) {
            DrawRectangle((int)eastWardJunction.x - 2, y, 3, 23, (Color){222, 190, 111, 235});
            DrawRectangle((int)eastWardJunction.x + 3, y, 2, 23, (Color){222, 190, 111, 150});
        }
    }

    // Zebra crossings and stop bars clarify the two connected junctions.
    // Crosswalk coordinates are also used by the Street View renderer.
    for (int junction = 1; junction <= 2; junction++) {
        Vector2 node = CITY_LAYOUT_ROAD_NODES[junction];
        DrawCrosswalkHorizontal((int)node.x + CITY_LAYOUT_CROSSWALK_LEFT_OFFSET, 293);
        DrawCrosswalkHorizontal((int)node.x + CITY_LAYOUT_CROSSWALK_RIGHT_OFFSET, 293);
        DrawCrosswalkVertical((int)node.x - CITY_LAYOUT_CROSSWALK_HALF_WIDTH,
                              (int)node.y + CITY_LAYOUT_CROSSWALK_UP_OFFSET);
        DrawCrosswalkVertical((int)node.x - CITY_LAYOUT_CROSSWALK_HALF_WIDTH,
                              (int)node.y + CITY_LAYOUT_CROSSWALK_DOWN_OFFSET);
    }
    // East Ward access junction and two marked pedestrian crossing points.
    DrawCrosswalkHorizontal((int)eastWardJunction.x + CITY_LAYOUT_CROSSWALK_LEFT_OFFSET, 293);
    for (int crossingY = 262; crossingY <= 350; crossingY += 88)
        for (int stripe = 0; stripe < 5; stripe++)
            DrawRectangle((int)eastWardJunction.x - 21, crossingY + stripe * 5,
                          42, 3, (Color){203, 212, 209, 175});

    // The short ambulance driveway joins the hospital receiving entrance to
    // the existing East Ward road endpoint used by the route planner.
    DrawLineEx(CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE],
               CITY_LAYOUT_HOSPITAL_ENTRANCE, 16.0f, (Color){68, 82, 91, 255});
    DrawLineEx(CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE],
               CITY_LAYOUT_HOSPITAL_ENTRANCE, 1.5f, (Color){139, 151, 153, 220});

    // District labels and road names provide orientation without hiding units.
    DrawText("RIVERSIDE", 55, 245, 11, (Color){191, 207, 203, 235});
    DrawText("NORTH MARKET", 493, 245, 11, (Color){191, 207, 203, 235});
    DrawText("EAST WARD", 936, 245, 11, (Color){191, 207, 203, 235});
    DrawText("WEST END", 55, 645, 11, (Color){191, 207, 203, 235});
    DrawText("CENTRAL", 493, 645, 11, (Color){191, 207, 203, 235});
    DrawText("PARK DISTRICT", 845, 645, 11, (Color){191, 207, 203, 235});
    DrawServiceLandmark(CITY_LAYOUT_FIRE_STATION, "FIRE HQ", RED);
    DrawServiceLandmark(CITY_LAYOUT_POLICE_STATION, "POLICE", BLUE);
    DrawHospitalMapLandmark();
}

static const char *IncidentTypeName(IncidentType type)

{

    switch (type) {

        case INCIDENT_FIRE: return "Fire";

        case INCIDENT_CRIME: return "Crime";

        case INCIDENT_ACCIDENT: return "Traffic accident";

        case INCIDENT_MEDICAL: return "Medical problem";

        default: return "Unknown";

    }

}



static const char *IncidentStatusName(IncidentStatus status)

{

    switch (status) {

        case INCIDENT_PENDING: return "Pending";

        case INCIDENT_DISPATCHED: return "Travelling";

        case INCIDENT_RESPONDING: return "On scene";

        case INCIDENT_RESOLVED: return "Resolved";

        case INCIDENT_FAILED: return "FAILED";

        case INCIDENT_SECURING: return "Securing scene";

        case INCIDENT_FIREFIGHTING: return "Firefighting";

        case INCIDENT_TREATING: return "Treating patient";

        case INCIDENT_TRANSPORTING: return "Transporting";

        default: return "Unknown";

    }

}


static int FindIncidentAtPosition(const Simulation *simulation, Vector2 position)
{
    // Check in reverse draw order so a topmost marker wins when markers overlap.
    for (int i = simulation->incidentCount - 1; i >= 0; i--) {
        const Incident *incident = &simulation->incidents[i];
        float radius = incident->severity == 1 ? 16.0f :
                       incident->severity == 2 ? 19.0f : 22.0f;
        if (CheckCollisionPointCircle(position, incident->position, radius)) return i;
    }
    return -1;
}

static int FindCallAtPosition(const Simulation *simulation, Vector2 position)
{
    for (int i = simulation->callCount - 1; i >= 0; i--) {
        if (simulation->calls[i].incidentIndex >= 0) continue;
        if (CheckCollisionPointCircle(position, simulation->calls[i].location, 20.0f)) return i;
    }
    return -1;
}

static const char *CallerReportForType(IncidentType type, int callNumber)
{
    static const char *reports[4][2] = {
        {"Caller reports smoke from a building; people may be inside.",
         "Flames spreading; caller reports people trapped inside."},
        {"Possible break-in in progress at a nearby shop.",
         "Caller reports an assault and a person hurt."},
        {"Two vehicles collided; one lane blocked and injuries unclear.",
         "Multiple vehicles involved in a serious collision."},
        {"Caller reports a person collapsed and unresponsive.",
         "An injured person needs urgent medical help."}
    };
    if (type < INCIDENT_FIRE || type > INCIDENT_MEDICAL) type = INCIDENT_FIRE;
    return reports[type][callNumber % 2];
}



static int DrawButtonIcon(Rectangle rect, const char *label)
{
    float cy = rect.y + rect.height * 0.5f;
    if (strstr(label, "FIRE")) {
        DrawIncidentIcon(INCIDENT_FIRE, (Vector2){rect.x + 13, cy}, 6, RAYWHITE);
    } else if (strstr(label, "POLICE")) {
        DrawIncidentIcon(INCIDENT_CRIME, (Vector2){rect.x + 13, cy}, 6, RAYWHITE);
    } else if (strstr(label, "AMBULANCE") || strstr(label, "MEDICAL")) {
        DrawIncidentIcon(INCIDENT_MEDICAL, (Vector2){rect.x + 13, cy}, 6, RAYWHITE);
    } else if (strstr(label, "PAUSE")) {
        DrawRectangleRounded((Rectangle){rect.x + 9, cy - 6, 4, 12}, 0.3f, 3, RAYWHITE);
        DrawRectangleRounded((Rectangle){rect.x + 16, cy - 6, 4, 12}, 0.3f, 3, RAYWHITE);
    } else if (strstr(label, "RESUME")) {
        DrawTriangle((Vector2){rect.x + 9, cy - 7}, (Vector2){rect.x + 9, cy + 7},
                     (Vector2){rect.x + 20, cy}, RAYWHITE);
    } else if (strstr(label, "DISPATCH")) {
        DrawLineEx((Vector2){rect.x + 7, cy}, (Vector2){rect.x + 18, cy}, 2, RAYWHITE);
        DrawLineEx((Vector2){rect.x + 13, cy - 5}, (Vector2){rect.x + 18, cy}, 2, RAYWHITE);
        DrawLineEx((Vector2){rect.x + 13, cy + 5}, (Vector2){rect.x + 18, cy}, 2, RAYWHITE);
    } else if (strstr(label, "REPORT")) {
        Rectangle page = {rect.x + 8, cy - 7, 11, 14};
        DrawRectangleLinesEx(page, 1.2f, RAYWHITE);
        DrawLine((int)page.x + 2, (int)page.y + 4, (int)page.x + 8, (int)page.y + 4, RAYWHITE);
        DrawLine((int)page.x + 2, (int)page.y + 8, (int)page.x + 8, (int)page.y + 8, RAYWHITE);
    } else if (strstr(label, "SETTING")) {
        Vector2 center = {rect.x + 13, cy};
        DrawCircleLines((int)center.x, (int)center.y, 6, RAYWHITE);
        DrawCircleV(center, 2, RAYWHITE);
        for (int i = 0; i < 4; i++) {
            float angle = (float)i * PI * 0.5f;
            DrawLineEx((Vector2){center.x + cosf(angle) * 6.5f, center.y + sinf(angle) * 6.5f},
                       (Vector2){center.x + cosf(angle) * 8.5f, center.y + sinf(angle) * 8.5f},
                       1.5f, RAYWHITE);
        }
    } else if (strstr(label, "RESET")) {
        Vector2 center = {rect.x + 13, cy};
        DrawCircleLines((int)center.x, (int)center.y, 6, RAYWHITE);
        DrawTriangle((Vector2){center.x + 2, center.y - 8},
                     (Vector2){center.x + 8, center.y - 4},
                     (Vector2){center.x + 2, center.y - 2}, RAYWHITE);
    } else if (strstr(label, "COMPARE")) {
        DrawRectangle(rect.x + 8, cy - 6, 4, 12, RAYWHITE);
        DrawRectangle(rect.x + 15, cy - 3, 4, 9, RAYWHITE);
    } else if (strstr(label, "SLOW") || strstr(label, "FAST") ||
               strstr(label, "1x") || strstr(label, "STEP")) {
        Vector2 center = {rect.x + 13, cy + 2};
        DrawCircleLines((int)center.x, (int)center.y, 6, RAYWHITE);
        DrawLineEx(center, (Vector2){center.x + 3, center.y - 4}, 1.5f, RAYWHITE);
    } else {
        return 0;
    }
    return 1;
}

static void DrawButton(Rectangle rect, const char *label, Color color)

{

    Rectangle shadow = {rect.x + 2, rect.y + 3, rect.width, rect.height};
    DrawRectangleRounded(shadow, 0.18f, 8, (Color){0, 0, 0, 38});
    DrawRectangleRounded(rect, 0.18f, 8, color);
    DrawRectangleRoundedLines(rect, 0.18f, 8, (Color){117, 157, 183, 165});

    int hasIcon = rect.width >= 70.0f ? DrawButtonIcon(rect, label) : 0;
    int textWidth = MeasureUiText(label, 11);
    float contentX = rect.x + (hasIcon ? 21.0f : 0.0f);
    float contentWidth = rect.width - (hasIcon ? 21.0f : 0.0f);

    DrawText(label, (int)(contentX + (contentWidth - textWidth) / 2),

             (int)(rect.y + (rect.height - 11) / 2), 11, RAYWHITE);

}



static int IsIncidentFinished(const Incident *incident)
{
    return incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED;
}

static int CallIncidentIndex(const Simulation *simulation, int callIndex)
{
    if (callIndex < 0 || callIndex >= simulation->callCount) return -1;
    return simulation->calls[callIndex].incidentIndex;
}

static int CallIsTerminal(const Simulation *simulation, int callIndex)
{
    int incidentIndex = CallIncidentIndex(simulation, callIndex);
    return incidentIndex >= 0 && incidentIndex < simulation->incidentCount &&
           IsIncidentFinished(&simulation->incidents[incidentIndex]);
}

static int BuildCallQueueOrder(const Simulation *simulation, int order[MAX_CALLS])
{
    int count = 0;
    // Calls awaiting classification or units stay at the top of the queue.
    for (int i = 0; i < simulation->callCount; i++) {
        const EmergencyCall *call = &simulation->calls[i];
        if (call->dispatched || CallIsTerminal(simulation, i)) continue;
        order[count++] = i;
    }
    // Then show in-progress calls, followed by the most recent completed calls.
    for (int i = 0; i < simulation->callCount; i++) {
        if (simulation->calls[i].dispatched && !CallIsTerminal(simulation, i))
            order[count++] = i;
    }
    for (int i = simulation->callCount - 1; i >= 0; i--) {
        if (CallIsTerminal(simulation, i)) order[count++] = i;
    }
    return count;
}

static Color IncidentColor(IncidentType type)
{
    switch (type) {
        case INCIDENT_FIRE: return (Color){255, 91, 81, 255};
        case INCIDENT_CRIME: return (Color){80, 164, 255, 255};
        case INCIDENT_ACCIDENT: return (Color){255, 177, 67, 255};
        case INCIDENT_MEDICAL: return (Color){48, 202, 145, 255};
        default: return RAYWHITE;
    }
}

static const char *ServiceName(IncidentType service)
{
    if (service == INCIDENT_FIRE) return "Fire Engine";
    if (service == INCIDENT_CRIME) return "Police Unit";
    return "Ambulance";
}

static void DrawPanel(Rectangle rect)
{
    DrawRectangleRounded(rect, 0.04f, 12, (Color){11, 27, 43, 255});
    DrawRectangleRoundedLines(rect, 0.04f, 12, (Color){39, 73, 96, 255});
    DrawLineEx((Vector2){rect.x + 14.0f, rect.y + 1.0f},
               (Vector2){rect.x + rect.width - 14.0f, rect.y + 1.0f},
               1.2f, (Color){45, 125, 165, 115});
}

static void DrawMapLegend(void)
{
    const int startX = 770;
    const int y = 111;
    DrawIncidentIcon(INCIDENT_FIRE, (Vector2){startX + 4, y}, 4.5f, IncidentColor(INCIDENT_FIRE));
    DrawText("Fire", startX + 12, y - 5, 8, (Color){206, 218, 227, 255});
    DrawVehicleIcon(INCIDENT_CRIME, (Vector2){startX + 55, y});
    DrawText("Police", startX + 65, y - 5, 8, (Color){206, 218, 227, 255});
    DrawVehicleIcon(INCIDENT_ACCIDENT, (Vector2){startX + 113, y});
    DrawText("EMS", startX + 123, y - 5, 8, (Color){206, 218, 227, 255});
    DrawLineEx((Vector2){startX + 160, y}, (Vector2){startX + 176, y},
               2.0f, (Color){72, 181, 255, 235});
    DrawText("Route", startX + 180, y - 5, 8, (Color){206, 218, 227, 255});
    DrawCircleLines(startX + 224, y, 4, (Color){255, 188, 82, 255});
    DrawText("Call", startX + 232, y - 5, 8, (Color){206, 218, 227, 255});
}

static int CountActiveForService(const Simulation *simulation, IncidentType service)
{
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (SimulationServiceForIncident(incident->type) == service && !IsIncidentFinished(incident)) count++;
    }
    return count;
}

static int CountActiveHazards(const Simulation *simulation)
{
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if ((incident->type == INCIDENT_FIRE || incident->type == INCIDENT_ACCIDENT) &&
            !IsIncidentFinished(incident)) count++;
    }
    return count;
}

static int CountFleetForService(const Simulation *simulation, IncidentType service)
{
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (simulation->vehicles[i].serviceType == service) count++;
    return count;
}

static float AverageResponseForService(const Simulation *simulation, IncidentType service)
{
    int resolved = simulation->resolvedByType[service];
    return resolved > 0 ? simulation->totalResponseTimeByType[service] / resolved : -1.0f;
}

static void OnTimeArrivalCounts(const Simulation *simulation, int *onTime, int *recorded)
{
    *onTime = 0;
    *recorded = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (!incident->responseRecorded) continue;
        (*recorded)++;
        if (incident->responseTime <= incident->deadline) (*onTime)++;
    }
}

static void DrawServiceSummary(const Simulation *simulation, IncidentType service,
                               int x, int y, Color color)
{
    const IncidentType iconType = service == INCIDENT_ACCIDENT ? INCIDENT_MEDICAL : service;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, 350, 28}, 0.08f, 8,
                        (Color){20, 42, 60, 255});
    DrawIncidentIcon(iconType, (Vector2){(float)x + 15, (float)y + 14}, 6, color);
    DrawText(ServiceName(service), x + 28, y + 2, 8, RAYWHITE);
    DrawText(TextFormat("Available %d / %d  |  Active %d",
             SimulationCountAvailableVehicles(simulation, service),
             CountFleetForService(simulation, service),
             CountActiveForService(simulation, service)), x + 28, y + 11, 6,
             (Color){179, 198, 211, 255});
    float average = AverageResponseForService(simulation, service);
    DrawText(average < 0.0f ? TextFormat("Resolved %d  |  Avg response --",
             simulation->resolvedByType[service]) :
             TextFormat("Resolved %d  |  Avg response %.1fs",
             simulation->resolvedByType[service], average), x + 200, y + 11, 6,
             (Color){141, 161, 175, 255});
}

static void DrawOperationalSummary(const Simulation *simulation)
{
    int onTime, recorded;
    int available = 0;
    int waitingForUnits = 0;
    OnTimeArrivalCounts(simulation, &onTime, &recorded);
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (simulation->vehicles[i].available) available++;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (!IsIncidentFinished(incident) &&
            CountAssignedUnits(simulation, i) < CountRequiredUnits(incident)) waitingForUnits++;
    }

    DrawLine(1224, 717, 1574, 717, (Color){43, 67, 83, 255});
    DrawText("CITY OPERATIONS", 1224, 721, 8, (Color){107, 190, 238, 255});
    DrawText(TextFormat("Fleet %d/%d ready  |  Waiting for units %d",
             available, simulation->vehicleCount, waitingForUnits),
             1224, 734, 6, (Color){177, 195, 207, 255});
    DrawText(recorded > 0 ? TextFormat("On-time first arrivals %d / %d", onTime, recorded) :
             "On-time first arrivals -- (no arrivals yet)", 1224, 744, 6,
             (Color){177, 195, 207, 255});
}

static void DrawHeaderService(const Simulation *simulation, IncidentType service,
                              int x, const char *label, Color color)
{
    DrawRectangleRounded((Rectangle){(float)x, 12, 190, 58}, 0.12f, 7,
                         (Color){12, 37, 57, 255});
    DrawRectangleRoundedLines((Rectangle){(float)x, 12, 190, 58}, 0.12f, 7,
                              (Color){34, 74, 101, 255});
    DrawIncidentIcon(service == INCIDENT_ACCIDENT ? INCIDENT_MEDICAL : service,
                     (Vector2){(float)x + 20, 40}, 11, color);
    DrawText(label, x + 39, 19, 10, (Color){179, 198, 211, 255});
    DrawText(TextFormat("%d active  |  %d/%d ready", CountActiveForService(simulation, service),
             SimulationCountAvailableVehicles(simulation, service),
             CountFleetForService(simulation, service)), x + 39, 40, 9, RAYWHITE);
}

static const char *VehicleActivityName(const Vehicle *vehicle)
{
    if (vehicle->activity == UNIT_TRANSPORTING) return "TRANSPORTING";
    if (vehicle->activity == UNIT_RETURNING) return "RETURNING";
    if (vehicle->activity == UNIT_ON_SCENE || vehicle->arrived) return "ON SCENE";
    if (vehicle->activity == UNIT_RESPONDING) return "EN ROUTE";
    return "AVAILABLE";
}

static void DrawUnitRoster(const Simulation *simulation)
{
    DrawPanel((Rectangle){392, 608, 806, 230});
    DrawText("UNIT ROSTER", 408, 621, 15, RAYWHITE);
    DrawText("Live fleet state  |  destination and ETA follow simulation data", 526, 625,
             9, (Color){139, 164, 179, 255});
    const IncidentType services[] = {INCIDENT_FIRE, INCIDENT_CRIME, INCIDENT_ACCIDENT};
    const int columns[] = {408, 674, 940};
    const Color serviceColors[] = {{255, 91, 81, 255}, {80, 164, 255, 255},
                                   {48, 202, 145, 255}};
    for (int serviceIndex = 0; serviceIndex < 3; serviceIndex++) {
        IncidentType service = services[serviceIndex];
        DrawIncidentIcon(service == INCIDENT_ACCIDENT ? INCIDENT_MEDICAL : service,
                         (Vector2){(float)columns[serviceIndex] + 8, 646}, 7,
                         serviceColors[serviceIndex]);
        DrawText(service == INCIDENT_ACCIDENT ? "AMBULANCE" :
                 service == INCIDENT_CRIME ? "POLICE" : "FIRE",
                 columns[serviceIndex] + 20, 640, 9, serviceColors[serviceIndex]);
        int row = 0;
        for (int i = 0; i < simulation->vehicleCount && row < 4; i++) {
            const Vehicle *vehicle = &simulation->vehicles[i];
            if (vehicle->serviceType != service) continue;
            int y = 654 + row * 42;
            Rectangle card = {(float)columns[serviceIndex], (float)y, 250, 38};
            Color cardColor = vehicle->available ? (Color){15, 43, 59, 255} :
                              (Color){18, 38, 54, 255};
            DrawRectangleRounded(card, 0.12f, 6, cardColor);
            DrawVehicleIcon(service, (Vector2){(float)columns[serviceIndex] + 15, (float)y + 17});
            DrawText(TextFormat("%s %d  |  %s", ServiceName(service),
                     ServiceUnitNumber(simulation, i), VehicleActivityName(vehicle)),
                     columns[serviceIndex] + 31, y + 4, 8,
                     vehicle->available ? (Color){194, 214, 225, 255} : RAYWHITE);
            int destination = vehicle->assignedIncident;
            if (!vehicle->available && destination >= 0 && destination < simulation->incidentCount) {
                float eta = SimulationEstimateUnitETA(simulation, i, destination);
                char statusLine[64];
                if (vehicle->activity == UNIT_TRANSPORTING) {
                    if (eta >= 0.0f) snprintf(statusLine, sizeof(statusLine),
                                             "HOSPITAL  |  ETA %.1fs", eta);
                    else snprintf(statusLine, sizeof(statusLine), "DESTINATION: HOSPITAL");
                } else if (vehicle->activity == UNIT_RETURNING) {
                    if (eta >= 0.0f) snprintf(statusLine, sizeof(statusLine),
                                             "STATION  |  ETA %.1fs", eta);
                    else snprintf(statusLine, sizeof(statusLine), "DESTINATION: STATION");
                }
                else if (vehicle->arrived)
                    snprintf(statusLine, sizeof(statusLine), "AT INCIDENT  |  ON SCENE");
                else if (eta < 0.0f)
                    snprintf(statusLine, sizeof(statusLine), "EN ROUTE  |  ETA --");
                else if (simulation->incidents[destination].callIndex >= 0)
                    snprintf(statusLine, sizeof(statusLine), "CALL %02d  |  ETA %.1fs",
                        simulation->incidents[destination].callIndex + 1, eta);
                else
                    snprintf(statusLine, sizeof(statusLine), "INCIDENT %02d  |  ETA %.1fs",
                        destination + 1, eta);
                DrawText(statusLine, columns[serviceIndex] + 31, y + 20, 7,
                         (Color){112, 190, 228, 255});
            } else {
                DrawText("READY FOR DISPATCH", columns[serviceIndex] + 31, y + 20, 7,
                         (Color){137, 164, 179, 255});
            }
            row++;
        }
        if (row == 0) DrawText("No units in fleet", columns[serviceIndex], 662, 8, GRAY);
    }
}

static void DrawCityMark(void)
{
    Color mark = (Color){61, 151, 207, 255};
    DrawRectangleRounded((Rectangle){29, 25, 10, 26}, 0.12f, 3, mark);
    DrawRectangleRounded((Rectangle){42, 16, 13, 35}, 0.12f, 3, mark);
    DrawRectangleRounded((Rectangle){58, 29, 10, 22}, 0.12f, 3, mark);
    for (int row = 0; row < 3; row++) {
        DrawRectangle(32, 30 + row * 6, 2, 2, (Color){194, 229, 248, 255});
        DrawRectangle(47, 22 + row * 7, 2, 2, (Color){194, 229, 248, 255});
        DrawRectangle(62, 34 + row * 6, 2, 2, (Color){194, 229, 248, 255});
    }
}

static void DrawNextCallTypeSelector(IncidentType selectedType)
{
    Rectangle nextCallTypeButton = NextCallTypeSelectorBounds();
    Color accent = IncidentColor(selectedType);
    DrawRectangleRounded(nextCallTypeButton, 0.18f, 7, (Color){12, 37, 57, 255});
    DrawRectangleRoundedLinesEx(nextCallTypeButton, 0.18f, 7, 1.2f, Fade(accent, 0.9f));
    DrawIncidentIcon(selectedType,
        (Vector2){nextCallTypeButton.x + 16, nextCallTypeButton.y + 16}, 7, accent);
    DrawText("NEXT CALL TYPE", (int)nextCallTypeButton.x + 31,
             (int)nextCallTypeButton.y + 3, 6, (Color){151, 178, 194, 255});
    DrawText(IncidentTypeName(selectedType), (int)nextCallTypeButton.x + 31,
             (int)nextCallTypeButton.y + 13, 9, RAYWHITE);
    DrawTriangle((Vector2){nextCallTypeButton.x + nextCallTypeButton.width - 17,
                           nextCallTypeButton.y + 12},
                 (Vector2){nextCallTypeButton.x + nextCallTypeButton.width - 9,
                           nextCallTypeButton.y + 12},
                 (Vector2){nextCallTypeButton.x + nextCallTypeButton.width - 13,
                           nextCallTypeButton.y + 18}, accent);
}

static void DrawIncidentQueue(const Simulation *simulation, const int order[MAX_CALLS],
                              int orderCount, int selectedCall)
{
    DrawPanel(queuePanel);
    int waiting = 0;
    for (int i = 0; i < simulation->callCount; i++)
        if (!simulation->calls[i].dispatched && !CallIsTerminal(simulation, i)) waiting++;
    DrawText("INCIDENT QUEUE", 26, 103, 17, RAYWHITE);
    DrawText(TextFormat("%d waiting  |  %d total", waiting, simulation->callCount), 26, 126, 10,
             (Color){134, 160, 177, 255});
    DrawText(simulation->dispatchMode == DISPATCH_AUTOMATIC ? "AUTO" : "MANUAL",
             318, 126, 9, simulation->dispatchMode == DISPATCH_AUTOMATIC ? GREEN : SKYBLUE);

    int visible = orderCount < QUEUE_VISIBLE ? orderCount : QUEUE_VISIBLE;
    if (visible == 0) {
        DrawText("No calls received yet", 34, 166, 13, (Color){160, 181, 195, 255});
        DrawText("Click the map to log a call.", 34, 188, 11, (Color){116, 142, 159, 255});
    } else {
        for (int row = 0; row < visible; row++) {
            int callIndex = order[row];
            const EmergencyCall *call = &simulation->calls[callIndex];
            int incidentIndex = CallIncidentIndex(simulation, callIndex);
            const Incident *incident = incidentIndex >= 0 &&
                incidentIndex < simulation->incidentCount ? &simulation->incidents[incidentIndex] : NULL;
            float y = queueRowsY + row * queueRowHeight;
            Rectangle card = QueueCardBounds(row);
            Color typeColor = incident ? IncidentColor(incident->type) : (Color){232, 166, 70, 255};
            Color urgencyColor = incident && incident->severity >= 3 ? (Color){255, 112, 91, 255} :
                                 incident && incident->severity == 2 ? (Color){255, 189, 85, 255} :
                                 (Color){130, 181, 209, 255};
            DrawRectangleRounded(card, 0.08f, 8,
                callIndex == selectedCall ? (Color){30, 57, 78, 255} : (Color){18, 38, 54, 255});
            DrawRectangleRounded((Rectangle){card.x, card.y, 4, card.height}, 0.3f, 4, typeColor);
            if (callIndex == selectedCall)
                DrawRectangleRoundedLines(card, 0.08f, 8, (Color){71, 151, 205, 255});

            if (incident) DrawIncidentIcon(incident->type, (Vector2){43, y + 25}, 9, typeColor);
            else {
                DrawCircle(43, (int)(y + 25), 10, (Color){93, 64, 26, 255});
                DrawText("?", 39, (int)y + 17, 16, (Color){255, 205, 110, 255});
            }
            DrawText(TextFormat("CALL %02d%s%s", callIndex + 1,
                     incident ? "  |  " : "",
                     incident ? IncidentTypeName(incident->type) : ""),
                     62, (int)y + 6, 11, RAYWHITE);
            const char *priorityText = incident ? PriorityLabel(incident->severity) :
                                       call->classified ? PriorityLabel(call->severity) : "UNCLASSIFIED";
            Rectangle priorityBadge = {card.x + card.width - 66, y + 5, 58, 16};
            DrawRectangleRounded(priorityBadge, 0.25f, 4,
                incident && incident->severity >= 3 ? (Color){156, 45, 54, 255} :
                incident && incident->severity == 2 ? (Color){151, 102, 35, 255} :
                incident ? (Color){37, 112, 82, 255} : (Color){82, 72, 50, 255});
            DrawText(priorityText, (int)priorityBadge.x + 4, (int)priorityBadge.y + 4,
                     6, RAYWHITE);
            const char *progress = !call->classified ? "NEEDS CLASSIFICATION" :
                CallIsTerminal(simulation, callIndex) && incident->status == INCIDENT_RESOLVED ? "RESOLVED" :
                CallIsTerminal(simulation, callIndex) && incident->status == INCIDENT_FAILED ? "FAILED" :
                !call->dispatched ? "WAITING FOR SUITABLE UNITS" :
                IncidentStatusName(incident->status);
            Color progressColor = CallIsTerminal(simulation, callIndex) && incident &&
                                  incident->status == INCIDENT_FAILED ? RED :
                                  CallIsTerminal(simulation, callIndex) && incident &&
                                  incident->status == INCIDENT_RESOLVED ? GREEN :
                                  !call->classified || !call->dispatched ? ORANGE : SKYBLUE;
            DrawText(progress, 62, (int)y + 22, 8, progressColor);
            DrawWrappedText(call->report, 62, (int)y + 34, 8, 47, 8,
                            (Color){190, 205, 214, 255});
            DrawText(TextFormat("LOCATION  %.0f, %.0f", call->location.x, call->location.y),
                     62, (int)y + 54, 7, (Color){150, 177, 192, 255});
            int assigned = incident ? CountAssignedUnits(simulation, incidentIndex) : 0;
            int required = incident ? CountRequiredUnits(incident) : 0;
            DrawText(TextFormat("RECEIVED T+%.0fs   UNITS %d/%d",
                     call->receivedAt, assigned, required), 62, (int)y + 66, 7, urgencyColor);
        }
    }
}

static int CountSelectedVehicles(unsigned int mask)
{
    int count = 0;
    for (int i = 0; i < MAX_VEHICLES; i++) if (mask & (1u << i)) count++;
    return count;
}

static int ServiceUnitNumber(const Simulation *simulation, int vehicleIndex)
{
    int number = 0;
    for (int i = 0; i <= vehicleIndex; i++)
        if (simulation->vehicles[i].serviceType == simulation->vehicles[vehicleIndex].serviceType)
            number++;
    return number;
}

static int DrawWrappedText(const char *text, int x, int y, int fontSize,
                           int maxCharacters, int lineHeight, Color color)
{
    int lines = 0;
    const char *start = text;
    while (*start) {
        char line[CALL_REPORT_LENGTH];
        int length = 0;
        while (start[length] && start[length] != '\n' && length < maxCharacters) length++;
        if (start[length] && start[length] != '\n') {
            while (length > 0 && start[length] != ' ') length--;
        }
        if (length == 0) {
            while (start[length] && start[length] != '\n' && length < maxCharacters) length++;
        }
        memcpy(line, start, (size_t)length);
        line[length] = '\0';
        DrawText(line, x, y + lines * lineHeight, fontSize, color);
        lines++;
        start += length;
        while (*start == ' ' || *start == '\n') start++;
        if (lines >= 3) break;
    }
    return lines;
}

static const char *PriorityLabel(int severity)
{
    return severity >= 3 ? "HIGH" : severity == 2 ? "MEDIUM" : "LOW";
}

// A small vector operator portrait keeps the handoff visible without relying
// on emoji fonts or an external image asset.
static void DrawOperatorPortrait(Vector2 center, Color color)
{
    DrawCircleV((Vector2){center.x, center.y - 5}, 6, (Color){225, 196, 164, 255});
    DrawRectangleRounded((Rectangle){center.x - 10, center.y + 2, 20, 11},
                         0.45f, 6, color);
    DrawCircleLines((int)center.x, (int)(center.y - 5), 8, color);
    DrawLineEx((Vector2){center.x - 8, center.y - 7},
               (Vector2){center.x - 11, center.y + 1}, 2, color);
    DrawLineEx((Vector2){center.x - 11, center.y + 1},
               (Vector2){center.x - 5, center.y + 1}, 2, color);
    DrawCircleV((Vector2){center.x - 5, center.y + 1}, 2, color);
}

static void DrawOperatorHandoff(const EmergencyCall *call, int callNumber,
                                const Incident *incident, DispatchMode mode)
{
    Rectangle card = {1222, 152, 308, 54};
    DrawRectangleRounded(card, 0.12f, 8, (Color){10, 28, 42, 255});
    DrawRectangleRoundedLinesEx(card, 0.12f, 8, 1, (Color){37, 75, 96, 255});

    Color callerColor = SKYBLUE;
    Color operatorColor = !call->classified ? (Color){255, 195, 91, 255} : SKYBLUE;
    Color dispatchColor = call->dispatched ? GREEN : (Color){104, 123, 137, 255};
    const char *dispatchState = call->dispatched ? "UNITS SENT" : "WAITING";
    const char *operatorState = !call->classified ? "REVIEWING" :
                                call->dispatched ? "HANDOFF DONE" : "SELECTING UNITS";
    if (mode == DISPATCH_AUTOMATIC && !call->classified)
        operatorState = "ASSESSING REPORT";
    if (incident && (incident->status == INCIDENT_RESOLVED ||
                     incident->status == INCIDENT_FAILED)) {
        dispatchState = incident->status == INCIDENT_RESOLVED ? "RESOLVED" : "FAILED";
        dispatchColor = incident->status == INCIDENT_RESOLVED ? GREEN : RED;
    }

    DrawText(TextFormat("CALLER  #%02d", callNumber), 1232, 157, 8, callerColor);
    DrawText(mode == DISPATCH_AUTOMATIC ? "AUTO OPERATOR" : "MANUAL OPERATOR",
             1314, 157, 8, operatorColor);
    DrawText("DISPATCH", 1457, 157, 8, dispatchColor);
    DrawLineEx((Vector2){1254, 176}, (Vector2){1351, 176}, 2,
               call->classified ? SKYBLUE : (Color){87, 107, 120, 255});
    DrawLineEx((Vector2){1377, 176}, (Vector2){1474, 176}, 2,
               call->dispatched ? GREEN : (Color){87, 107, 120, 255});
    DrawCircleV((Vector2){1244, 176}, 9, callerColor);
    DrawText("C", 1241, 170, 12, (Color){8, 27, 40, 255});
    DrawOperatorPortrait((Vector2){1364, 176}, operatorColor);
    DrawCircleV((Vector2){1487, 176}, 9, dispatchColor);
    DrawText("D", 1484, 170, 12, (Color){8, 27, 40, 255});
    DrawText("REPORT RECEIVED", 1224, 190, 7, (Color){169, 188, 200, 255});
    DrawText(operatorState, 1314, 190, 7, operatorColor);
    DrawText(dispatchState, 1457, 190, 7, dispatchColor);
}

static void DrawIncidentDetails(const Simulation *simulation, int selectedCall,
                                int selectedIncident,
                                int comparisonReady, const StrategyComparison *comparison,
                                unsigned int selectedVehicleMask,
                                IncidentType manualType, int manualSeverity)
{
    DrawPanel(detailsPanel);
    int manualUnitsView = 0;
    if (selectedCall >= 0 && selectedCall < simulation->callCount) {
        const EmergencyCall *selected = &simulation->calls[selectedCall];
        int selectedIndex = CallIncidentIndex(simulation, selectedCall);
        const Incident *selectedData = selectedIndex >= 0 &&
            selectedIndex < simulation->incidentCount ? &simulation->incidents[selectedIndex] : NULL;
        const char *typeName = selected->classified ?
            IncidentTypeName(selected->classifiedType) : selectedData ?
            IncidentTypeName(selectedData->type) : "Awaiting classification";
        DrawText(TextFormat("SELECTED CALL #%02d  |  %s", selectedCall + 1, typeName),
                 1224, 120, 10, RAYWHITE);
        DrawRectangleRounded((Rectangle){1468, 117, 106, 25}, 0.22f, 6,
            selected->classified && selected->severity >= 3 ? (Color){185, 45, 55, 255} :
            selected->classified && selected->severity == 2 ? (Color){184, 119, 25, 255} :
            selected->classified ? (Color){38, 130, 89, 255} : (Color){87, 76, 51, 255});
        DrawText(selected->classified ? TextFormat("%s PRIORITY", PriorityLabel(selected->severity)) :
                 "PRIORITY PENDING", 1474, 124, 7, RAYWHITE);
    } else if (selectedIncident >= 0 && selectedIncident < simulation->incidentCount) {
        const Incident *incident = &simulation->incidents[selectedIncident];
        int number = incident->callIndex >= 0 ? incident->callIndex + 1 : selectedIncident + 1;
        DrawText(TextFormat("SELECTED INCIDENT #%02d  |  %s", number,
                 IncidentTypeName(incident->type)), 1224, 120, 10, RAYWHITE);
        DrawRectangleRounded((Rectangle){1468, 117, 106, 25}, 0.22f, 6,
            incident->severity >= 3 ? (Color){185, 45, 55, 255} :
            incident->severity == 2 ? (Color){184, 119, 25, 255} : (Color){38, 130, 89, 255});
        DrawText(TextFormat("%s PRIORITY", PriorityLabel(incident->severity)),
                 1474, 124, 7, RAYWHITE);
    } else {
        DrawText("CALL HANDLING", 1224, 120, 17, RAYWHITE);
    }
    DrawLine(1224, 146, 1529, 146, (Color){43, 67, 83, 255});

    if (selectedCall >= 0 && selectedCall < simulation->callCount) {
        const EmergencyCall *call = &simulation->calls[selectedCall];
        int incidentIndex = CallIncidentIndex(simulation, selectedCall);
        const Incident *incident = incidentIndex >= 0 && incidentIndex < simulation->incidentCount ?
                                   &simulation->incidents[incidentIndex] : NULL;
        int manualSelectingUnits = simulation->dispatchMode == DISPATCH_MANUAL &&
                                   ManualCallNeedsVehicleAssignment(simulation, selectedCall);
        manualUnitsView = manualSelectingUnits;
        int assignedContentEndY = 326;
        DrawOperatorHandoff(call, selectedCall + 1, incident, simulation->dispatchMode);

        DrawWrappedText(call->report, 1224, 210, 9, 55, 12,
                        (Color){207, 219, 225, 255});
        DrawText(TextFormat("LOCATION  %.0f, %.0f   |   RECEIVED T+%.1fs",
                 call->location.x, call->location.y, call->receivedAt),
                 1224, 235, 9, (Color){155, 185, 202, 255});

        if (simulation->dispatchMode == DISPATCH_MANUAL &&
            !call->dispatched && !call->classified) {
            DrawText("CLASSIFY CALL  /  INCIDENT TYPE", 1224, 253, 10,
                     (Color){107, 190, 238, 255});
            const IncidentType types[] = {INCIDENT_FIRE, INCIDENT_CRIME,
                                          INCIDENT_ACCIDENT, INCIDENT_MEDICAL};
            const char *labels[] = {"FIRE", "POLICE", "ACCIDENT", "MEDICAL"};
            for (int i = 0; i < 4; i++) {
                Color buttonColor = manualType == types[i] ? IncidentColor(types[i]) :
                                    (Color){25, 47, 63, 255};
                DrawButton(callTypeButtons[i], labels[i], buttonColor);
            }
            DrawText("PRIORITY", 1224, 346, 9, (Color){155, 185, 202, 255});
            for (int i = 0; i < 3; i++) {
                Color buttonColor = manualSeverity == i + 1 ?
                    (i == 2 ? (Color){185, 45, 55, 255} :
                     i == 1 ? (Color){184, 119, 25, 255} : (Color){38, 130, 89, 255}) :
                    (Color){25, 47, 63, 255};
                DrawButton(callPriorityButtons[i], i == 0 ? "LOW" : i == 1 ? "MED" : "HIGH",
                           buttonColor);
            }
            DrawText(TextFormat("REQUIRED  %d x %s", GetRequiredUnits(manualType, manualSeverity),
                     ServiceName(SimulationServiceForIncident(manualType))),
                     1224, 404, 9, RAYWHITE);
            DrawButton(ManualClassifyButtonBounds(),
                       call->classified ? "UPDATE CLASSIFICATION" : "CLASSIFY CALL",
                       BLUE);
            DrawText(TextFormat("RECEIVED T+%.1fs  |  CLASSIFICATION PENDING",
                     call->receivedAt), 1224, 478, 8, (Color){155, 185, 202, 255});
        } else if (incident) {
            float remaining = incident->deadline - incident->ageTime;
            if (remaining < 0.0f) remaining = 0.0f;
            float eta = SimulationEstimateIncidentETA(simulation, incidentIndex);
            DrawText(TextFormat("CLASSIFIED  %s  |  SEVERITY %d  |  PRIORITY %.0f",
                     IncidentTypeName(incident->type), incident->severity,
                     SimulationIncidentPriority(incident)), 1224, 255, 9, RAYWHITE);
            char requirements[128] = "ON SCENE / REQUIRED";
            size_t used = strlen(requirements);
            const char *serviceShortNames[SERVICE_COUNT] = {"FIRE", "POLICE", "EMS"};
            for (int requiredService = 0; requiredService < SERVICE_COUNT; requiredService++) {
                int required = incident->requiredUnitsByService[requiredService];
                if (required <= 0 || used >= sizeof(requirements) - 1) continue;
                int written = snprintf(requirements + used, sizeof(requirements) - used,
                                       "  %s %d/%d", serviceShortNames[requiredService],
                                       CountOnSceneService(simulation, incidentIndex,
                                           (IncidentType)requiredService), required);
                if (written > 0) used += (size_t)written;
            }
            DrawText(requirements, 1224, 272, 8, (Color){177, 195, 207, 255});
            DrawText(TextFormat("TIME LEFT %.1fs   |   ETA %s",
                     remaining, eta < 0.0f ? "--" : TextFormat("%.1fs", eta)),
                     1224, 289, 9, remaining < 6.0f ? ORANGE :
                     (Color){177, 195, 207, 255});
            DrawText(TextFormat("RESPONSE STAGE  %s", IncidentStatusName(incident->status)),
                     1224, 302, 8, (Color){155, 185, 202, 255});

            if (manualSelectingUnits) {
                DrawText(call->dispatched ? "SELECT ADDITIONAL UNITS  (incident escalated)" :
                         "SELECT SUITABLE UNITS  (all required before dispatch)",
                         1224, 315, 9, (Color){107, 190, 238, 255});
                int vehicleRows[MANUAL_VEHICLE_ROW_COUNT];
                int rowCount = ManualVehicleRows(simulation, incident, vehicleRows);
                for (int row = 0; row < rowCount; row++) {
                    int i = vehicleRows[row];
                    const Vehicle *vehicle = &simulation->vehicles[i];
                    Rectangle vehicleRow = ManualVehicleRowBounds(row);
                    int y = (int)vehicleRow.y;
                    int checked = (selectedVehicleMask & (1u << i)) != 0;
                    int serviceSelected = ManualVehicleSelectedForService(
                        selectedVehicleMask, simulation, vehicle->serviceType);
                    int serviceRemaining = incident->requiredUnitsByService[vehicle->serviceType] -
                        CountAssignedUnitsByService(simulation, incidentIndex, vehicle->serviceType);
                    int selectable = vehicle->available && !IsIncidentFinished(incident) &&
                                     (checked || serviceSelected < serviceRemaining);
                    DrawRectangleRounded(vehicleRow, 0.15f, 4,
                        checked ? (Color){29, 68, 91, 255} :
                        selectable ? (Color){18, 38, 54, 255} : (Color){27, 34, 43, 255});
                    DrawVehicleIcon(vehicle->serviceType, (Vector2){1239, (float)y + 7});
                    DrawText(TextFormat("%s %d  %s", ServiceName(vehicle->serviceType),
                             ServiceUnitNumber(simulation, i),
                             selectable ? "AVAILABLE" : vehicle->available ? "CALL CLOSED" : "BUSY"),
                             1258, y + 2, 8,
                             selectable ? (Color){196, 211, 220, 255} : GRAY);
                    if (selectable) {
                        float unitEta = SimulationEstimateUnitETA(simulation, i, incidentIndex);
                        DrawText(unitEta < 0.0f ? "ETA --" : TextFormat("ETA %.1fs", unitEta),
                                 1446, y + 2, 8, (Color){107, 190, 238, 255});
                        DrawRectangleLinesEx(ManualVehicleCheckboxBounds(row), 1,
                                             checked ? SKYBLUE : (Color){105, 129, 143, 255});
                        if (checked) DrawText("x", 1509, y, 10, SKYBLUE);
                    } else {
                        DrawText(vehicle->available ? "LIMIT" : "IN USE",
                                 1474, y + 2, 8, GRAY);
                    }
                }
                if (rowCount == 0)
                    DrawText("No suitable vehicles are available; leave this call waiting.",
                             1224, 331, 8, ORANGE);
                DrawButton(dispatchSelectedButton,
                    TextFormat("DISPATCH SELECTED  %d / %d",
                        CountSelectedVehicles(selectedVehicleMask),
                        CountRequiredUnits(incident) - CountAssignedUnits(simulation, incidentIndex)),
                    CountSelectedVehicles(selectedVehicleMask) ==
                        CountRequiredUnits(incident) - CountAssignedUnits(simulation, incidentIndex) &&
                        CountSelectedVehicles(selectedVehicleMask) > 0 &&
                        !IsIncidentFinished(incident) ? BLUE : DARKGRAY);
            } else {
                int assignedStartY = 326;
                if (simulation->dispatchMode == DISPATCH_AUTOMATIC) {
                    DrawText("AUTO OPERATOR REASON", 1224, 313, 9,
                             (Color){107, 190, 238, 255});
                    DrawWrappedText(call->decisionReason, 1224, 327, 9, 56, 12,
                                    (Color){207, 219, 225, 255});
                    assignedStartY = 357;
                }
                DrawText("ASSIGNED UNITS", 1224, assignedStartY, 9,
                         (Color){107, 190, 238, 255});
                int shown = 0;
                for (int i = 0; i < simulation->vehicleCount && shown < MAX_VEHICLES; i++) {
                    const Vehicle *vehicle = &simulation->vehicles[i];
                    if (vehicle->assignedIncident != incidentIndex) continue;
                    int y = assignedStartY + 15 + shown * 17;
                    DrawVehicleIcon(vehicle->serviceType, (Vector2){1239, (float)y + 8});
                    const char *unitState = vehicle->activity == UNIT_TRANSPORTING ? "TRANSPORTING" :
                                            vehicle->activity == UNIT_RETURNING ? "RETURNING" :
                                            vehicle->arrived ? "ON SCENE" : "EN ROUTE";
                    DrawText(TextFormat("%s %d  %s", ServiceName(vehicle->serviceType),
                             ServiceUnitNumber(simulation, i), unitState),
                             1258, y + 2, 8, RAYWHITE);
                    float unitEta = SimulationEstimateUnitETA(simulation, i, incidentIndex);
                    DrawText(vehicle->activity == UNIT_TRANSPORTING ?
                             (unitEta < 0.0f ? "HOSP --" : TextFormat("HOSP %.1fs", unitEta)) :
                             vehicle->activity == UNIT_RETURNING ? "RETURNING" :
                             vehicle->arrived ? "ARRIVED" :
                             unitEta < 0.0f ? "ETA --" : TextFormat("ETA %.1fs", unitEta),
                             1446, y + 2, 8, (Color){107, 190, 238, 255});
                    shown++;
                }
                assignedContentEndY = assignedStartY + 15 + shown * 17;
                if (!call->dispatched)
                    DrawText("Waiting for suitable units; dispatcher retries automatically.",
                             1224, assignedContentEndY + 2, 7, ORANGE);
            }

            int manualSelecting = manualSelectingUnits;
            int timelineY = manualSelecting ? 638 :
                (assignedContentEndY + 11 > 480 ? assignedContentEndY + 11 : 480);
            float callToDispatch = call->dispatched ? call->dispatchedAt - call->receivedAt : -1.0f;
            DrawText(callToDispatch >= 0.0f ?
                     TextFormat("RECEIVED T+%.1f  |  CLASSIFIED T+%.1f  |  DISPATCH %.1fs",
                                call->receivedAt, call->classifiedAt, callToDispatch) :
                     TextFormat("RECEIVED T+%.1f  |  CLASSIFIED T+%.1f  |  AWAITING DISPATCH",
                                call->receivedAt, call->classifiedAt),
                     1224, timelineY, 8, (Color){155, 185, 202, 255});
            DrawText(TextFormat("First arrival: %s", incident->responseRecorded ?
                     TextFormat("%.1fs after call", incident->responseTime) : "pending"),
                     1224, timelineY + 11, 8, (Color){155, 185, 202, 255});
            if (call->feedbackReady)
                DrawText(call->callerSatisfied ? "CALLER FEEDBACK: SATISFIED" :
                         "CALLER FEEDBACK: DISSATISFIED", 1224, timelineY + 22, 9,
                         call->callerSatisfied ? GREEN : (Color){255, 105, 110, 255});
        } else if (simulation->dispatchMode == DISPATCH_AUTOMATIC) {
            DrawText("AUTO OPERATOR IS CLASSIFYING THIS REPORT", 1224, 268, 9, SKYBLUE);
            DrawWrappedText(call->decisionReason, 1224, 288, 9, 55, 12,
                            (Color){207, 219, 225, 255});
        }
    } else if (selectedIncident >= 0 && selectedIncident < simulation->incidentCount) {
        const Incident *incident = &simulation->incidents[selectedIncident];
        float remaining = incident->deadline - incident->ageTime;
        if (remaining < 0.0f) remaining = 0.0f;
        float eta = SimulationEstimateIncidentETA(simulation, selectedIncident);
        DrawText(TextFormat("STATUS  %s  |  SEVERITY %d",
                 IncidentStatusName(incident->status), incident->severity),
                 1224, 168, 10, RAYWHITE);
        DrawText(TextFormat("URGENCY %.0f  |  TIME LEFT %.1fs  |  ETA %s",
                 SimulationIncidentPriority(incident), remaining,
                 eta < 0.0f ? "--" : TextFormat("%.1fs", eta)),
                 1224, 190, 8, (Color){177, 195, 207, 255});
        DrawText("REQUIRED / ON SCENE", 1224, 214, 9, (Color){107, 190, 238, 255});
        for (int service = 0; service < SERVICE_COUNT; service++) {
            int required = incident->requiredUnitsByService[service];
            if (required <= 0) continue;
            int y = 234 + service * 20;
            DrawText(TextFormat("%s  %d / %d ON SCENE", ServiceName((IncidentType)service),
                     CountOnSceneService(simulation, selectedIncident,
                                         (IncidentType)service), required),
                     1224, y, 8, RAYWHITE);
        }
        DrawText("ASSIGNED UNITS", 1224, 300, 9, (Color){107, 190, 238, 255});
        int row = 0;
        for (int i = 0; i < simulation->vehicleCount && row < MAX_VEHICLES; i++) {
            const Vehicle *vehicle = &simulation->vehicles[i];
            if (vehicle->assignedIncident != selectedIncident) continue;
            int y = 316 + row * 20;
            DrawVehicleIcon(vehicle->serviceType, (Vector2){1239, (float)y + 7});
            DrawText(TextFormat("%s %d  %s", ServiceName(vehicle->serviceType),
                     ServiceUnitNumber(simulation, i), VehicleActivityName(vehicle)),
                     1258, y, 8, RAYWHITE);
            float unitEta = SimulationEstimateUnitETA(simulation, i, selectedIncident);
            DrawText(vehicle->arrived ? "ARRIVED" : unitEta < 0.0f ? "ETA --" :
                     TextFormat("ETA %.1fs", unitEta), 1480, y, 8,
                     (Color){107, 190, 238, 255});
            row++;
        }
    } else if (comparisonReady && comparison) {
        DrawText("STRATEGY COMPARISON", 1224, 166, 12, (Color){107, 190, 238, 255});
        DrawText("Nearest first", 1224, 194, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->nearestFirst.resolved,
                 comparison->nearestFirst.failed), 1224, 214, 11, (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->nearestFirst.averageResponseTime),
                 1224, 233, 11, (Color){177, 195, 207, 255});
        DrawText("Priority dispatch", 1224, 265, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->priority.resolved,
                 comparison->priority.failed), 1224, 285, 11, (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->priority.averageResponseTime),
                 1224, 304, 11, (Color){177, 195, 207, 255});
    } else {
        DrawText("Select a call from the queue", 1224, 168, 12, RAYWHITE);
        DrawText("or click a marker on the map.", 1224, 188, 11,
                 (Color){156, 177, 191, 255});
        DrawText(TextFormat("Received %d calls  |  Active incidents %d",
                 simulation->callCount, SimulationCountUnresolved(simulation)),
                 1224, 225, 10, (Color){177, 195, 207, 255});
    }

    if (!manualUnitsView) {
        DrawLine(1224, 612, 1574, 612, (Color){43, 67, 83, 255});
        DrawText("SERVICE AVAILABILITY", 1224, 616, 9, (Color){107, 190, 238, 255});
        DrawServiceSummary(simulation, INCIDENT_FIRE, 1218, 628, IncidentColor(INCIDENT_FIRE));
        DrawServiceSummary(simulation, INCIDENT_CRIME, 1218, 657, IncidentColor(INCIDENT_CRIME));
        DrawServiceSummary(simulation, INCIDENT_ACCIDENT, 1218, 686, IncidentColor(INCIDENT_MEDICAL));
        DrawOperationalSummary(simulation);
    }
}

static Rectangle FitCityToPanel(Rectangle panel)
{
    float scaleX = panel.width / CITY_WIDTH;
    float scaleY = panel.height / CITY_HEIGHT;
    float scale = scaleX < scaleY ? scaleX : scaleY;
    float width = CITY_WIDTH * scale;
    float height = CITY_HEIGHT * scale;
    return (Rectangle){panel.x + (panel.width - width) * 0.5f,
                       panel.y + (panel.height - height) * 0.5f, width, height};
}

static Vector2 MapCanvasToCity(Vector2 point, Rectangle cityView)
{
    return (Vector2){(point.x - cityView.x) * CITY_WIDTH / cityView.width,
                     (point.y - cityView.y) * CITY_HEIGHT / cityView.height};
}

static Vector3 CityPointToStreetWorld(Vector2 point)
{
    return CityLayoutMapPointToStreet(point);
}

static Vector3 CityRectCenterToStreetWorld(Rectangle rect)
{
    return CityPointToStreetWorld((Vector2){rect.x + rect.width * 0.5f,
                                            rect.y + rect.height * 0.5f});
}

static float CityPixelsToStreet(float pixels)
{
    return pixels * CITY_LAYOUT_STREET_SCALE;
}

static Vector3 KeepStreetPointInCity(Vector3 point)
{
    const float halfWidth = CITY_WIDTH * CITY_LAYOUT_STREET_SCALE * 0.5f - 0.35f;
    const float halfDepth = CITY_HEIGHT * CITY_LAYOUT_STREET_SCALE * 0.5f - 0.35f;
    if (point.x < -halfWidth) point.x = -halfWidth;
    if (point.x > halfWidth) point.x = halfWidth;
    if (point.z < -halfDepth) point.z = -halfDepth;
    if (point.z > halfDepth) point.z = halfDepth;
    return point;
}

static void DrawStreetRoadNetwork(CityCondition condition, Vector3 cameraPosition)
{
    // Keep these as untinted source colors. StreetDistanceColor applies the
    // condition once, then (for fog) adds the distance haze separately.
    const Color basePavement = (Color){68, 82, 91, 255};
    const Color baseCurb = (Color){139, 151, 153, 220};
    const Color baseLane = (Color){222, 190, 111, 235};
    const float scale = CityPixelsToStreet(1.0f);

    // Sidewalk/access bands use the exact rectangles painted by the 2D map.
    for (int i = 0; i < CITY_LAYOUT_SIDEWALK_COUNT; i++) {
        Rectangle r = CITY_LAYOUT_SIDEWALKS[i];
        Vector3 center = CityRectCenterToStreetWorld(r);
        DrawCubeV((Vector3){center.x,
                           CITY_LAYOUT_SIDEWALK_SURFACE_Y - 0.035f,
                           center.z},
                  (Vector3){r.width * scale, 0.07f, r.height * scale},
                  StreetDistanceColor((Color){87, 102, 105, 255}, center,
                                      cameraPosition, condition));
    }

    // Generate the 3D road surfaces from the route planner's shared graph.
    for (int i = 0; i < CITY_LAYOUT_ROAD_EDGE_COUNT; i++) {
        CityRoadEdge edge = CITY_LAYOUT_ROAD_EDGES[i];
        Vector2 a = CITY_LAYOUT_ROAD_NODES[edge.from];
        Vector2 b = CITY_LAYOUT_ROAD_NODES[edge.to];
        Vector3 wa = CityPointToStreetWorld(a);
        Vector3 wb = CityPointToStreetWorld(b);
        float length = (fabsf(b.x - a.x) + fabsf(b.y - a.y)) * scale;
        int horizontal = fabsf(b.x - a.x) >= fabsf(b.y - a.y);
        float roadWidth = CityPixelsToStreet(CityLayoutRoadWidth(i));
        Vector3 center = {(wa.x + wb.x) * 0.5f, 0.025f, (wa.z + wb.z) * 0.5f};
        Color pavement = StreetDistanceColor(basePavement, center, cameraPosition,
                                             condition);
        Color curb = StreetDistanceColor(baseCurb, center, cameraPosition, condition);
        Color lane = StreetDistanceColor(baseLane, center, cameraPosition, condition);
        DrawCubeV(center, horizontal ? (Vector3){length + 0.04f, 0.09f, roadWidth} :
                                      (Vector3){roadWidth, 0.09f, length + 0.04f}, pavement);

        // Cool, broken specular streaks sit on the road surface during rain.
        // They are static geometry, so rain adds no traffic or simulation work.
        if (condition == CITY_RAIN) {
            Color wetSheen = StreetDistanceColor((Color){127, 173, 190, 120},
                                                center, cameraPosition, condition);
            float sheenLength = CityPixelsToStreet(12.0f);
            float sheenGap = CityPixelsToStreet(28.0f);
            for (float along = CityPixelsToStreet(26.0f); along < length - CityPixelsToStreet(22.0f);
                 along += sheenLength + sheenGap) {
                Vector3 from = center, to = center;
                if (horizontal) {
                    float sign = wb.x >= wa.x ? 1.0f : -1.0f;
                    from.x = wa.x + sign * along;
                    to.x = wa.x + sign * (along + sheenLength);
                    from.z += roadWidth * 0.25f;
                    to.z += roadWidth * 0.25f;
                } else {
                    float sign = wb.z >= wa.z ? 1.0f : -1.0f;
                    from.z = wa.z + sign * along;
                    to.z = wa.z + sign * (along + sheenLength);
                    from.x += roadWidth * 0.25f;
                    to.x += roadWidth * 0.25f;
                }
                from.y = to.y = 0.079f;
                DrawLine3D(from, to, wetSheen);
            }
        }

        // Paired dashed center paint mirrors the opposing lanes in the 2D map.
        float dashLength = CityPixelsToStreet(23.0f);
        float gap = CityPixelsToStreet(17.0f);
        float offset = CityPixelsToStreet(2.5f);
        for (float along = CityPixelsToStreet(20.0f); along < length - CityPixelsToStreet(20.0f);
             along += dashLength + gap) {
            float end = fminf(along + dashLength,
                              length - CityPixelsToStreet(15.0f));
            for (int side = -1; side <= 1; side += 2) {
                Vector3 from = center, to = center;
                if (horizontal) {
                    float sign = wb.x >= wa.x ? 1.0f : -1.0f;
                    from.x = wa.x + sign * along;
                    to.x = wa.x + sign * end;
                    from.z += (float)side * offset;
                    to.z += (float)side * offset;
                } else {
                    float sign = wb.z >= wa.z ? 1.0f : -1.0f;
                    from.z = wa.z + sign * along;
                    to.z = wa.z + sign * end;
                    from.x += (float)side * offset;
                    to.x += (float)side * offset;
                }
                from.y = to.y = 0.083f;
                DrawLine3D(from, to, lane);
            }
        }

        // Raised-looking edge lines make the road boundary legible from street level.
        for (int side = -1; side <= 1; side += 2) {
            Vector3 from = wa, to = wb;
            if (horizontal) {
                from.z += (float)side * roadWidth * 0.5f;
                to.z += (float)side * roadWidth * 0.5f;
            } else {
                from.x += (float)side * roadWidth * 0.5f;
                to.x += (float)side * roadWidth * 0.5f;
            }
            from.y = to.y = 0.10f;
            DrawLine3D(from, to, curb);
        }
    }

    // Zebra stripes are positioned from the same two intersection nodes and
    // offsets as the 2D crosswalks in DrawCity().
    const int junctions[] = {1, 2};
    for (int j = 0; j < 2; j++) {
        Vector2 node = CITY_LAYOUT_ROAD_NODES[junctions[j]];
        for (int side = -1; side <= 1; side += 2) {
            float crossX = node.x + (side < 0 ? CITY_LAYOUT_CROSSWALK_LEFT_OFFSET :
                                                CITY_LAYOUT_CROSSWALK_RIGHT_OFFSET);
            for (int stripe = 0; stripe < 5; stripe++) {
                float x = crossX + (float)stripe * 5.0f;
                Vector3 from = CityPointToStreetWorld((Vector2){x,
                    node.y - CITY_LAYOUT_CROSSWALK_HALF_WIDTH});
                Vector3 to = CityPointToStreetWorld((Vector2){x,
                    node.y + CITY_LAYOUT_CROSSWALK_HALF_WIDTH});
                from.y = to.y = 0.09f;
                DrawLine3D(from, to, (Color){203, 212, 209, 205});
            }
        }
        for (int side = -1; side <= 1; side += 2) {
            float crossY = node.y + (side < 0 ? CITY_LAYOUT_CROSSWALK_UP_OFFSET :
                                                CITY_LAYOUT_CROSSWALK_DOWN_OFFSET);
            for (int stripe = 0; stripe < 5; stripe++) {
                float z = crossY + (float)stripe * 5.0f;
                Vector3 from = CityPointToStreetWorld((Vector2){
                    node.x - CITY_LAYOUT_CROSSWALK_HALF_WIDTH, z});
                Vector3 to = CityPointToStreetWorld((Vector2){
                    node.x + CITY_LAYOUT_CROSSWALK_HALF_WIDTH, z});
                from.y = to.y = 0.09f;
                DrawLine3D(from, to, (Color){203, 212, 209, 205});
            }
        }
    }
    // The East Ward connector uses the same narrowed road and crossings as
    // the 2D map; pedestrian routes cross only at these marked locations.
    Vector2 eastWard = CITY_LAYOUT_ROAD_NODES[9];
    for (int stripe = 0; stripe < 5; stripe++) {
        float z = 262.0f + (float)stripe * 5.0f;
        Vector3 from = CityPointToStreetWorld((Vector2){eastWard.x - 21.0f, z});
        Vector3 to = CityPointToStreetWorld((Vector2){eastWard.x + 21.0f, z});
        from.y = to.y = 0.09f;
        DrawLine3D(from, to, (Color){203, 212, 209, 205});
        z = 350.0f + (float)stripe * 5.0f;
        from = CityPointToStreetWorld((Vector2){eastWard.x - 21.0f, z});
        to = CityPointToStreetWorld((Vector2){eastWard.x + 21.0f, z});
        from.y = to.y = 0.09f;
        DrawLine3D(from, to, (Color){203, 212, 209, 205});
    }
    // One crosswalk at the west side of the three-way East Ward junction.
    for (int stripe = 0; stripe < 5; stripe++) {
        float x = eastWard.x + CITY_LAYOUT_CROSSWALK_LEFT_OFFSET + (float)stripe * 5.0f;
        Vector3 from = CityPointToStreetWorld((Vector2){x,
            eastWard.y - CITY_LAYOUT_CROSSWALK_HALF_WIDTH});
        Vector3 to = CityPointToStreetWorld((Vector2){x,
            eastWard.y + CITY_LAYOUT_CROSSWALK_HALF_WIDTH});
        from.y = to.y = 0.09f;
        DrawLine3D(from, to, (Color){203, 212, 209, 205});
    }

    // Receiving apron and four render-only bay stalls. The simulated route
    // still ends at the shared entrance coordinate in this apron.
    Rectangle bay = CITY_LAYOUT_HOSPITAL_BAY;
    Vector3 bayCenter = CityRectCenterToStreetWorld(bay);
    DrawCubeV((Vector3){bayCenter.x, 0.025f, bayCenter.z},
              (Vector3){bay.width * scale, 0.07f, bay.height * scale},
              StreetDistanceColor((Color){61, 76, 81, 255}, bayCenter,
                                  cameraPosition, condition));
    for (int side = 0; side < 2; side++) {
        float x = bay.x + 2.0f + (float)side * (bay.width - 4.0f);
        Vector3 from = CityPointToStreetWorld((Vector2){x, bay.y + 2.0f});
        Vector3 to = CityPointToStreetWorld((Vector2){x, bay.y + bay.height - 2.0f});
        from.y = to.y = 0.09f;
        DrawLine3D(from, to, (Color){191, 206, 205, 210});
    }
    for (int slot = 1; slot < CITY_LAYOUT_HOSPITAL_BAY_COUNT; slot++) {
        float y = bay.y + bay.height * (float)slot /
                  (float)CITY_LAYOUT_HOSPITAL_BAY_COUNT;
        Vector3 from = CityPointToStreetWorld((Vector2){bay.x + 2.0f, y});
        Vector3 to = CityPointToStreetWorld((Vector2){bay.x + bay.width - 2.0f, y});
        from.y = to.y = 0.09f;
        DrawLine3D(from, to, (Color){191, 206, 205, 180});
    }

    // The driveway matches the 2D access stroke and joins the existing
    // East Ward route node to the marked ambulance receiving position.
    Vector3 hospitalAccessStart = CityPointToStreetWorld(
        CITY_LAYOUT_ROAD_NODES[CITY_LAYOUT_HOSPITAL_ENTRANCE_NODE]);
    Vector3 hospitalEntrance = CityPointToStreetWorld(CITY_LAYOUT_HOSPITAL_ENTRANCE);
    float dx = hospitalEntrance.x - hospitalAccessStart.x;
    float dz = hospitalEntrance.z - hospitalAccessStart.z;
    float accessLength = sqrtf(dx * dx + dz * dz);
    Vector3 accessForward = {dx / accessLength, 0.0f, dz / accessLength};
    Vector3 accessCenter = {(hospitalAccessStart.x + hospitalEntrance.x) * 0.5f,
                            0.035f,
                            (hospitalAccessStart.z + hospitalEntrance.z) * 0.5f};
    DrawStreetOrientedBox(accessCenter, accessForward, accessLength + 0.03f,
                          0.08f, bay.width * scale,
                          StreetDistanceColor((Color){68, 82, 91, 255},
                                              accessCenter, cameraPosition, condition));
}

static void DrawStreetBuilding(Rectangle footprint, int index, int highlighted,
                               CityCondition condition, Vector3 cameraPosition)
{
    static const Color facades[] = {
        {82,108,117,255}, {75,102,112,255}, {93,118,127,255},
        {73,97,108,255}, {78,105,112,255}, {88,111,118,255},
        {73,101,110,255}, {91,112,119,255}
    };
    const float scale = CityPixelsToStreet(1.0f);
    Vector3 center = CityRectCenterToStreetWorld(footprint);
    float width = footprint.width * scale;
    float depth = footprint.height * scale;
    float height = 3.4f + (float)(index % 4) * 0.75f;
    Color facade = facades[index % (int)(sizeof(facades) / sizeof(facades[0]))];
    if (highlighted) facade = (Color){112, 89, 77, 255};
    facade = StreetDistanceColor(facade, center, cameraPosition, condition);

    DrawCubeV((Vector3){center.x, height * 0.5f, center.z},
              (Vector3){width, height, depth}, facade);
    DrawCubeV((Vector3){center.x, height + 0.08f, center.z},
              (Vector3){width + 0.12f, 0.16f, depth + 0.12f},
              highlighted ? (Color){64, 49, 46, 255} : (Color){48, 66, 73, 255});

    // Regular windows and a street-facing entrance. The same footprint is
    // used by the 2D map, while floor count is a restrained visual treatment.
    int columns = width > 4.8f ? 4 : width > 3.2f ? 3 : 2;
    int floors = height > 5.0f ? 3 : 2;
    float windowWidth = fminf(0.34f, (width - 0.55f) / (float)(columns * 1.8f));
    float windowHeight = 0.24f;
    Color glass = condition == CITY_NIGHT ? (Color){255, 204, 112, 255} :
                  StreetDistanceColor((Color){133, 178, 192, 255}, center,
                                      cameraPosition, condition);
    for (int floor = 0; floor < floors; floor++) {
        float y = 0.85f + (float)floor * ((height - 1.1f) / (float)floors);
        for (int column = 0; column < columns; column++) {
            float x = center.x - width * 0.5f + 0.5f +
                      ((float)column + 0.5f) * (width - 1.0f) / (float)columns;
            DrawCubeV((Vector3){x, y, center.z + depth * 0.5f + 0.025f},
                      (Vector3){windowWidth, windowHeight, 0.05f}, glass);
            DrawCubeV((Vector3){x, y, center.z - depth * 0.5f - 0.025f},
                      (Vector3){windowWidth, windowHeight, 0.05f}, glass);
        }
        int sideWindows = depth > 3.6f ? 2 : 1;
        for (int column = 0; column < sideWindows; column++) {
            float z = center.z - depth * 0.25f +
                      (float)column * (depth * 0.5f);
            DrawCubeV((Vector3){center.x + width * 0.5f + 0.025f, y, z},
                      (Vector3){0.05f, windowHeight, 0.30f}, glass);
            DrawCubeV((Vector3){center.x - width * 0.5f - 0.025f, y, z},
                      (Vector3){0.05f, windowHeight, 0.30f}, glass);
        }
    }
    if (index == CITY_LAYOUT_BUILDING_COUNT + 2) {
        Vector3 entrance = CityPointToStreetWorld(CITY_LAYOUT_HOSPITAL_ENTRANCE);
        DrawCubeV((Vector3){center.x + width * 0.5f + 0.04f, 0.52f,
                            entrance.z},
                  (Vector3){0.08f, 0.88f, 0.40f}, (Color){44, 56, 61, 255});
    } else {
        DrawCubeV((Vector3){center.x, 0.52f, center.z + depth * 0.5f + 0.04f},
                  (Vector3){0.40f, 0.88f, 0.08f}, (Color){44, 56, 61, 255});
    }
}

static void DrawStreetCrosswalkForIncident(Vector2 position)
{
    Vector3 p = CityPointToStreetWorld(position);
    p.y = 0.12f;
    DrawSphere(p, 0.28f, (Color){245, 79, 69, 225});
    DrawCylinder((Vector3){p.x, 0.55f, p.z}, 0.055f, 0.055f, 0.75f,
                 8, (Color){244, 96, 76, 230});
    DrawCylinder((Vector3){p.x, 0.95f, p.z}, 0.35f, 0.02f, 0.025f,
                 16, (Color){255, 215, 120, 240});
}

static Color StreetPedestrianColor(PedestrianState state)
{
    switch (state) {
        case PEDESTRIAN_EVACUATING: return (Color){247, 145, 75, 255};
        case PEDESTRIAN_BYSTANDING: return (Color){90, 204, 230, 255};
        case PEDESTRIAN_RESPECTING_CORDON: return (Color){112, 167, 242, 255};
        case PEDESTRIAN_YIELDING: return (Color){255, 216, 105, 255};
        case PEDESTRIAN_CROSSING:
        case PEDESTRIAN_WAITING_TO_CROSS: return (Color){229, 240, 241, 255};
        default: return (Color){177, 202, 211, 255};
    }
}

static void DrawStreetPerson(Vector3 p, Color color, float phase)
{
    float step = sinf(phase) * 0.07f;
    DrawSphere((Vector3){p.x, p.y + 1.03f, p.z}, 0.16f,
               (Color){224, 190, 160, 255});
    DrawCubeV((Vector3){p.x, p.y + 0.64f, p.z},
              (Vector3){0.27f, 0.58f, 0.20f}, color);
    DrawLine3D((Vector3){p.x, p.y + 0.40f, p.z},
               (Vector3){p.x - 0.10f + step, p.y + 0.05f, p.z}, color);
    DrawLine3D((Vector3){p.x, p.y + 0.40f, p.z},
               (Vector3){p.x + 0.10f - step, p.y + 0.05f, p.z}, color);
}

static void DrawStreetOrientedBox(Vector3 center, Vector3 forward,
                                  float length, float height, float width,
                                  Color color)
{
    Vector3 right = {-forward.z, 0.0f, forward.x};
    float halfLength = length * 0.5f;
    float halfWidth = width * 0.5f;
    Vector3 corners[8];
    for (int top = 0; top < 2; top++) {
        float y = center.y + (top ? height * 0.5f : -height * 0.5f);
        for (int front = 0; front < 2; front++) {
            for (int side = 0; side < 2; side++) {
                int index = top * 4 + front * 2 + side;
                float f = front ? halfLength : -halfLength;
                float r = side ? halfWidth : -halfWidth;
                corners[index] = (Vector3){center.x + forward.x * f + right.x * r,
                                           y,
                                           center.z + forward.z * f + right.z * r};
            }
        }
    }
    // Each face is two triangles, orienting the vehicle mesh along the same
    // heading used by the simulation rather than leaving cars sideways.
    const int faces[6][4] = {
        {4,5,7,6}, {0,2,3,1}, {0,1,5,4},
        {2,6,7,3}, {0,4,6,2}, {1,3,7,5}
    };
    for (int face = 0; face < 6; face++) {
        Vector3 a = corners[faces[face][0]], b = corners[faces[face][1]];
        Vector3 c = corners[faces[face][2]], d = corners[faces[face][3]];
        DrawTriangle3D(a, b, c, color);
        DrawTriangle3D(a, c, d, color);
    }
}

static void DrawStreetCivilianCar(const CivilianCar *car, int index)
{
    Vector3 p = CityPointToStreetWorld(car->position);
    // The map simulation already offsets cars into their directional lanes;
    // this model stays at that exact point and uses the same heading.
    float angle = car->headingDegrees * DEG2RAD;
    Vector3 forward = {cosf(angle), 0.0f, sinf(angle)};
    Color body = car->yieldTime > 0.0f ? (Color){221, 161, 78, 255} :
                 (index % 3 == 0 ? (Color){163, 184, 194, 255} :
                  index % 3 == 1 ? (Color){117, 147, 166, 255} :
                                  (Color){194, 198, 184, 255});
    Vector3 bodyCenter = {p.x, 0.28f, p.z};
    DrawStreetOrientedBox(bodyCenter, forward, 0.72f, 0.32f, 0.40f, body);
    DrawStreetOrientedBox((Vector3){p.x, 0.48f, p.z}, forward,
                          0.39f, 0.18f, 0.31f, (Color){73, 105, 120, 255});
    Vector3 nose = {p.x + forward.x * 0.34f, 0.28f,
                    p.z + forward.z * 0.34f};
    DrawCubeV(nose, (Vector3){0.12f, 0.10f, 0.11f},
              (Color){236, 224, 177, 255});
}

static void DrawStreetTree(Vector2 cityPoint, float time, CityCondition condition)
{
    Vector3 p = CityPointToStreetWorld(cityPoint);
    float sway = (condition == CITY_WIND ? 0.19f : 0.025f) *
                 sinf(time * (condition == CITY_WIND ? 2.3f : 0.8f) +
                      cityPoint.x * 0.037f);
    DrawCylinderEx((Vector3){p.x, CITY_LAYOUT_STREET_GROUND_Y, p.z},
                   (Vector3){p.x, 1.03f, p.z}, 0.10f, 0.075f,
                   6, (Color){105, 82, 62, 255});
    DrawSphere((Vector3){p.x + sway * 0.55f, 1.03f, p.z},
               0.49f, (Color){47, 105, 77, 255});
    DrawSphere((Vector3){p.x - 0.18f + sway, 1.20f, p.z - 0.08f}, 0.27f,
               (Color){65, 129, 88, 255});
}

static void DrawStreetFurniture(CityCondition condition, float time)
{
    for (int i = 0; i < CITY_LAYOUT_STREET_LAMP_COUNT; i++) {
        Vector3 base = CityPointToStreetWorld(CITY_LAYOUT_STREET_LAMPS[i]);
        Color metal = condition == CITY_NIGHT ? (Color){77, 91, 103, 255} :
                                                (Color){103, 121, 128, 255};
        float headHeight = CITY_LAYOUT_LAMP_HEAD_TOP_Y -
                           CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y;
        float lensHeight = CITY_LAYOUT_LAMP_LENS_TOP_Y -
                           CITY_LAYOUT_LAMP_LENS_BOTTOM_Y;
        // The foundation sinks through the shared ground plane and rises above
        // the sidewalk surface. This makes the contact visible at a distance,
        // rather than relying on two nearly coplanar surfaces appearing joined.
        DrawCubeV((Vector3){base.x,
                            (CITY_LAYOUT_LAMP_FOOTING_BOTTOM_Y +
                             CITY_LAYOUT_LAMP_FOOTING_TOP_Y) * 0.5f, base.z},
                  (Vector3){CITY_LAYOUT_LAMP_FOOTING_SIZE,
                            CITY_LAYOUT_LAMP_FOOTING_TOP_Y -
                            CITY_LAYOUT_LAMP_FOOTING_BOTTOM_Y,
                            CITY_LAYOUT_LAMP_FOOTING_SIZE},
                  (Color){72, 86, 91, 255});
        // Raylib DrawCylinder() treats position.y as the bottom, not the
        // center. Use explicit endpoints here so each segment lands exactly
        // on the shared sidewalk/footing heights instead of hovering by half
        // its height.
        DrawCylinderEx((Vector3){base.x, CITY_LAYOUT_LAMP_BASE_BOTTOM_Y, base.z},
                       (Vector3){base.x, CITY_LAYOUT_LAMP_BASE_TOP_Y, base.z},
                       CITY_LAYOUT_LAMP_BASE_RADIUS, CITY_LAYOUT_LAMP_BASE_RADIUS,
                       10, metal);
        DrawCylinderEx((Vector3){base.x, CITY_LAYOUT_LAMP_POLE_BOTTOM_Y, base.z},
                       (Vector3){base.x, CITY_LAYOUT_LAMP_POLE_TOP_Y, base.z},
                       CITY_LAYOUT_LAMP_POLE_RADIUS, 0.045f, 10, metal);
        // The pole extends into the fixture and below the sidewalk into the
        // foundation, so neither joint depends on exact edge-to-edge contact.
        DrawCylinderEx((Vector3){base.x,
                                 CITY_LAYOUT_LAMP_POLE_TOP_Y - 0.14f, base.z},
                       (Vector3){base.x,
                                 CITY_LAYOUT_LAMP_POLE_TOP_Y + 0.02f, base.z},
                       CITY_LAYOUT_LAMP_POLE_RADIUS * 1.18f,
                       CITY_LAYOUT_LAMP_POLE_RADIUS * 1.18f, 10, metal);
        DrawCubeV((Vector3){base.x,
                  (CITY_LAYOUT_LAMP_HEAD_BOTTOM_Y +
                   CITY_LAYOUT_LAMP_HEAD_TOP_Y) * 0.5f, base.z},
                  (Vector3){CITY_LAYOUT_LAMP_HEAD_LENGTH, headHeight,
                            CITY_LAYOUT_LAMP_HEAD_WIDTH}, metal);
        Color lens = condition == CITY_NIGHT ? (Color){255, 229, 159, 255} :
                                               (Color){215, 224, 213, 255};
        DrawCubeV((Vector3){base.x,
                  (CITY_LAYOUT_LAMP_LENS_BOTTOM_Y +
                   CITY_LAYOUT_LAMP_LENS_TOP_Y) * 0.5f, base.z},
                  (Vector3){CITY_LAYOUT_LAMP_HEAD_LENGTH * 0.78f, lensHeight,
                            CITY_LAYOUT_LAMP_HEAD_WIDTH * 0.72f}, lens);
    }

    const int junctions[] = {1, 2};
    for (int j = 0; j < 2; j++) {
        Vector2 node = CITY_LAYOUT_ROAD_NODES[junctions[j]];
        Vector3 sign = CityPointToStreetWorld((Vector2){node.x - 51.0f,
                                                        node.y + 49.0f});
        DrawCylinder((Vector3){sign.x, 0.66f, sign.z}, 0.035f, 0.035f,
                     1.32f, 6, (Color){83, 96, 98, 255});
        DrawCubeV((Vector3){sign.x, 1.27f, sign.z},
                  (Vector3){0.48f, 0.30f, 0.08f}, (Color){34, 121, 116, 255});
    }
    // These tree locations are the same planted points used by the 2D map.
    static const Vector2 trees[] = {
        {68,76}, {119,108}, {92,116}, {131,78},
        {520,185}, {606,222}, {573,181}, {546,225},
        {870,493}, {943,550}
    };
    for (int i = 0; i < (int)(sizeof(trees) / sizeof(trees[0])); i++)
        DrawStreetTree(trees[i], time, condition);
}

static unsigned char StreetSignGlyphRow(char ch, int row)
{
    static const unsigned char font[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {31,4,4,4,4,4,31}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    if (ch < 'A' || ch > 'Z' || row < 0 || row > 6) return 0;
    return font[ch - 'A'][row];
}

static void DrawStreetSignText(const char *text, Vector3 center, float cell,
                               Color ink)
{
    const float advance = cell * 6.0f;
    float width = (float)strlen(text) * advance - cell;
    float startX = center.x - width * 0.5f;
    for (int letter = 0; text[letter]; letter++) {
        for (int row = 0; row < 7; row++) {
            unsigned char bits = StreetSignGlyphRow(text[letter], row);
            for (int column = 0; column < 5; column++) {
                if ((bits & (1u << (4 - column))) == 0) continue;
                DrawCubeV((Vector3){startX + (float)letter * advance +
                                    (float)column * cell,
                                    center.y + ((float)3 - (float)row) * cell,
                                    center.z},
                          (Vector3){cell * 0.78f, cell * 0.78f, 0.025f}, ink);
            }
        }
    }
}

static void DrawStreetSignTextEast(const char *text, Vector3 center, float cell,
                                   Color ink)
{
    const float advance = cell * 6.0f;
    float width = (float)strlen(text) * advance - cell;
    float startZ = center.z - width * 0.5f;
    for (int letter = 0; text[letter]; letter++) {
        for (int row = 0; row < 7; row++) {
            unsigned char bits = StreetSignGlyphRow(text[letter], row);
            for (int column = 0; column < 5; column++) {
                if ((bits & (1u << (4 - column))) == 0) continue;
                DrawCubeV((Vector3){center.x,
                                    center.y + ((float)3 - (float)row) * cell,
                                    startZ + (float)letter * advance +
                                    (float)column * cell},
                          (Vector3){0.025f, cell * 0.78f, cell * 0.78f}, ink);
            }
        }
    }
}

static void DrawStreetHospitalEntrance(Vector3 cameraPosition,
                                       CityCondition condition)
{
    Vector3 door = CityPointToStreetWorld(CITY_LAYOUT_HOSPITAL_DOOR);
    Vector3 arrival = CityPointToStreetWorld(CITY_LAYOUT_HOSPITAL_ENTRANCE);
    float edgeX = door.x;
    Color canopy = StreetDistanceColor((Color){49, 137, 133, 255},
                                      door, cameraPosition, condition);
    // Canopy projects from the east-facing entrance into the ambulance bay.
    DrawCubeV((Vector3){edgeX + 0.24f, 1.23f, door.z},
              (Vector3){0.62f, 0.13f, 1.05f}, canopy);
    DrawCubeV((Vector3){edgeX + 0.49f, 0.66f, door.z - 0.40f},
              (Vector3){0.07f, 1.12f, 0.07f}, (Color){81, 99, 102, 255});
    DrawCubeV((Vector3){edgeX + 0.49f, 0.66f, door.z + 0.40f},
              (Vector3){0.07f, 1.12f, 0.07f}, (Color){81, 99, 102, 255});

    // A wall-mounted sign faces the approaching ambulance on the access road.
    Vector3 plate = {edgeX + 0.04f, 2.02f, door.z - 0.50f};
    DrawCubeV(plate, (Vector3){0.12f, 0.70f, 2.18f},
              (Color){12, 43, 51, 255});
    DrawCubeV((Vector3){plate.x + 0.07f, plate.y, plate.z},
              (Vector3){0.035f, 0.62f, 2.08f},
              StreetDistanceColor((Color){35, 131, 119, 255}, plate,
                                  cameraPosition, condition));
    DrawStreetSignTextEast("HOSPITAL", (Vector3){plate.x + 0.10f,
                           plate.y - 0.04f, plate.z}, 0.034f, RAYWHITE);
    Vector3 cross = {plate.x + 0.11f, plate.y + 0.38f, plate.z};
    DrawCubeV(cross, (Vector3){0.035f, 0.25f, 0.08f}, RAYWHITE);
    DrawCubeV(cross, (Vector3){0.035f, 0.08f, 0.25f}, RAYWHITE);

    Vector3 groundMark = {arrival.x, 0.11f, arrival.z};
    DrawCylinder(groundMark, 0.19f, 0.19f, 0.035f, 12,
                 (Color){91, 225, 182, 235});
    DrawCubeV((Vector3){arrival.x - 0.06f, 0.15f, arrival.z},
              (Vector3){0.08f, 0.06f, 0.28f}, RAYWHITE);
}

static void DrawStreetServiceSign(Rectangle footprint, const char *label,
                                  const char *secondary, Color color)
{
    Vector3 building = CityRectCenterToStreetWorld(footprint);
    float depth = footprint.height * CITY_LAYOUT_STREET_SCALE;
    // The plate is mounted to the building's street-facing wall, not suspended
    // above the sidewalk. Its position is derived from the shared 2D footprint.
    Vector3 plate = {building.x, 1.58f, building.z + depth * 0.5f + 0.09f};
    float width = secondary ? 2.55f : 1.75f;
    float height = secondary ? 0.78f : 0.56f;
    DrawCubeV(plate, (Vector3){width, height, 0.12f}, (Color){10, 30, 42, 255});
    DrawCubeV((Vector3){plate.x, plate.y, plate.z + 0.07f},
              (Vector3){width - 0.09f, height - 0.09f, 0.025f}, color);
    float textCell = secondary ? 0.027f : 0.038f;
    if (secondary) {
        DrawStreetSignText(label, (Vector3){plate.x, plate.y + 0.15f, plate.z + 0.10f},
                           textCell, RAYWHITE);
        DrawStreetSignText(secondary,
                           (Vector3){plate.x, plate.y - 0.17f, plate.z + 0.10f},
                           textCell, RAYWHITE);
    } else {
        DrawStreetSignText(label, (Vector3){plate.x, plate.y, plate.z + 0.10f},
                           textCell, RAYWHITE);
    }
}

static void DrawStreetEmergencyVehicle(const Vehicle *vehicle, Vector3 p,
                                       int selected, float time)
{
    Vector3 forward = ResponseUnitForward(vehicle);
    Color body = vehicle->serviceType == INCIDENT_FIRE ? (Color){230, 48, 62, 255} :
                 vehicle->serviceType == INCIDENT_CRIME ? (Color){36, 129, 238, 255} :
                                                          (Color){230, 237, 230, 255};
    Vector3 size;
    if (vehicle->serviceType == INCIDENT_FIRE)
        size = selected ? (Vector3){1.85f, 0.64f, 0.88f} :
                          (Vector3){1.58f, 0.55f, 0.78f};
    else if (vehicle->serviceType == INCIDENT_ACCIDENT)
        size = selected ? (Vector3){1.48f, 0.68f, 0.88f} :
                          (Vector3){1.25f, 0.59f, 0.78f};
    else
        size = selected ? (Vector3){1.36f, 0.48f, 0.78f} :
                          (Vector3){1.14f, 0.43f, 0.68f};
    DrawStreetOrientedBox((Vector3){p.x, 0.34f, p.z}, forward,
                          size.x, size.y, size.z, body);
    float cabY = vehicle->serviceType == INCIDENT_ACCIDENT ? 0.82f : 0.66f;
    float cabLength = vehicle->serviceType == INCIDENT_FIRE ? 0.48f : 0.62f;
    DrawStreetOrientedBox((Vector3){p.x, cabY, p.z}, forward,
                          size.x * cabLength, size.y * 0.40f, size.z * 0.72f,
                          vehicle->serviceType == INCIDENT_ACCIDENT ?
                              (Color){208, 222, 220, 255} :
                              (Color){74, 103, 116, 255});
    Vector3 right = {-forward.z, 0.0f, forward.x};
    for (int axle = -1; axle <= 1; axle += 2) {
        for (int side = -1; side <= 1; side += 2) {
            Vector3 wheel = {p.x + forward.x * (float)axle * size.x * 0.31f +
                                  right.x * (float)side * size.z * 0.47f,
                             0.20f,
                             p.z + forward.z * (float)axle * size.x * 0.31f +
                                  right.z * (float)side * size.z * 0.47f};
            DrawStreetOrientedBox(wheel, forward, 0.30f, 0.22f, 0.13f,
                                  (Color){27, 37, 43, 255});
        }
    }
    DrawStreetOrientedBox((Vector3){p.x + forward.x * size.x * 0.49f, 0.29f,
                                    p.z + forward.z * size.x * 0.49f},
                          forward, 0.12f, 0.17f, size.z * 0.88f,
                          (Color){49, 60, 65, 255});
    if (vehicle->serviceType == INCIDENT_FIRE) {
        // Tank panel and ladder distinguish the engine from the other units.
        DrawStreetOrientedBox((Vector3){p.x - forward.x * 0.12f, 0.69f,
                                        p.z - forward.z * 0.12f}, forward,
                              size.x * 0.54f, 0.06f, size.z * 0.82f,
                              (Color){241, 198, 77, 255});
        Vector3 ladderA = {p.x - forward.x * 0.40f + right.x * 0.24f,
                           0.99f, p.z - forward.z * 0.40f + right.z * 0.24f};
        Vector3 ladderB = {p.x + forward.x * 0.42f + right.x * 0.24f,
                           0.99f, p.z + forward.z * 0.42f + right.z * 0.24f};
        DrawLine3D(ladderA, ladderB, (Color){235, 229, 199, 255});
        DrawLine3D((Vector3){ladderA.x + right.x * 0.09f, ladderA.y, ladderA.z + right.z * 0.09f},
                   (Vector3){ladderB.x + right.x * 0.09f, ladderB.y, ladderB.z + right.z * 0.09f},
                   (Color){235, 229, 199, 255});
    } else if (vehicle->serviceType == INCIDENT_CRIME) {
        // Police markings are simple contrasting door panels.
        DrawStreetOrientedBox((Vector3){p.x, 0.42f, p.z}, forward,
                              size.x * 0.30f, 0.12f, size.z * 1.02f,
                              (Color){224, 234, 239, 255});
        Vector3 badge = {p.x + right.x * size.z * 0.50f, 0.48f,
                         p.z + right.z * size.z * 0.50f};
        DrawCubeV(badge, (Vector3){0.13f, 0.13f, 0.05f}, (Color){30, 69, 119, 255});
    } else {
        // A medical cross and side stripe make the ambulance recognizable.
        DrawStreetOrientedBox((Vector3){p.x, 0.40f, p.z}, forward,
                              size.x * 0.74f, 0.08f, size.z * 1.02f,
                              (Color){52, 164, 204, 255});
        Vector3 cross = {p.x + right.x * size.z * 0.50f, 0.70f,
                         p.z + right.z * size.z * 0.50f};
        DrawCubeV(cross, (Vector3){0.12f, 0.24f, 0.05f}, (Color){213, 63, 70, 255});
        DrawCubeV((Vector3){cross.x, cross.y, cross.z},
                  (Vector3){0.24f, 0.10f, 0.05f}, (Color){213, 63, 70, 255});
    }
    Vector3 beacon = {p.x, vehicle->serviceType == INCIDENT_ACCIDENT ? 1.08f : 0.81f, p.z};
    if (vehicle->activity == UNIT_RESPONDING || vehicle->activity == UNIT_ON_SCENE ||
        vehicle->activity == UNIT_TRANSPORTING) {
        Color leftFlash = (Color){235, 49, 60, 255};
        Color rightFlash = (Color){48, 143, 255, 255};
        if (vehicle->serviceType == INCIDENT_FIRE) {
            leftFlash = (Color){245, 52, 54, 255};
            rightFlash = (Color){255, 191, 74, 255};
        } else if (vehicle->serviceType == INCIDENT_ACCIDENT) {
            leftFlash = (Color){246, 249, 244, 255};
            rightFlash = (Color){61, 177, 245, 255};
        }
        if (((int)(time * 12.0f) & 1) != 0) {
            Color swap = leftFlash;
            leftFlash = rightFlash;
            rightFlash = swap;
        }
        Vector3 leftBeacon = {p.x - right.x * 0.17f, beacon.y,
                              p.z - right.z * 0.17f};
        Vector3 rightBeacon = {p.x + right.x * 0.17f, beacon.y,
                               p.z + right.z * 0.17f};
        DrawCubeV(leftBeacon, (Vector3){0.20f, 0.12f, 0.19f}, leftFlash);
        DrawCubeV(rightBeacon, (Vector3){0.20f, 0.12f, 0.19f}, rightFlash);
    }
    Vector3 marker = {p.x + forward.x * size.x * 0.42f, 0.35f,
                      p.z + forward.z * size.x * 0.42f};
    DrawCubeV(marker, (Vector3){0.15f, 0.12f, 0.14f},
              (Color){245, 231, 175, 255});
    (void)selected; // The followed unit is already distinguished by its larger model and HUD.
}

static Vector3 StreetVehicleDisplayPosition(const Simulation *simulation, int vehicleIndex)
{
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    Vector2 hospitalSlot;
    if (HospitalArrivalSlotForVehicle(simulation, vehicleIndex, &hospitalSlot))
        return CityPointToStreetWorld(hospitalSlot);
    Vector2 axis = {1.0f, 0.0f};
    if (vehicle->routeCount >= 2) {
        Vector3 forward = ResponseUnitForward(vehicle);
        axis = (Vector2){forward.x, forward.z};
    }
    int rank = 0;
    int count = 0;
    int firstClusterVehicle = vehicleIndex;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *other = &simulation->vehicles[i];
        Vector2 delta = {other->position.x - vehicle->position.x,
                         other->position.y - vehicle->position.y};
        float longitudinal = delta.x * axis.x + delta.y * axis.y;
        float lateral = -delta.x * axis.y + delta.y * axis.x;
        // At street scale the longest emergency model is about 41 map pixels.
        // Only nearly same-lane overlaps are spread; parallel traffic stays in
        // its true directional lane.
        if (fabsf(longitudinal) < 42.0f && fabsf(lateral) < 17.0f) {
            if (i < vehicleIndex) rank++;
            if (i < firstClusterVehicle) firstClusterVehicle = i;
            count++;
        }
    }
    Vector2 visual = vehicle->position;
    if (count > 1) {
        if (firstClusterVehicle >= 0 && firstClusterVehicle < simulation->vehicleCount &&
            simulation->vehicles[firstClusterVehicle].routeCount >= 2) {
            Vector3 firstForward = ResponseUnitForward(&simulation->vehicles[firstClusterVehicle]);
            axis = (Vector2){firstForward.x, firstForward.z};
        }
        const float spacing = 46.0f;
        float centeredRank = (float)rank - ((float)count - 1.0f) * 0.5f;
        visual.x += axis.x * centeredRank * spacing;
        visual.y += axis.y * centeredRank * spacing;
    }
    return CityPointToStreetWorld(visual);
}

static int StreetPointVisibleToCamera(Camera3D camera, Vector3 point)
{
    Vector3 delta = {point.x - camera.position.x, 0.0f,
                     point.z - camera.position.z};
    float distance = sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (distance > 24.0f) return 0;
    Vector3 view = {camera.target.x - camera.position.x, 0.0f,
                    camera.target.z - camera.position.z};
    float viewLength = sqrtf(view.x * view.x + view.z * view.z);
    if (viewLength < 0.001f) return 1;
    view.x /= viewLength;
    view.z /= viewLength;
    float dot = (delta.x * view.x + delta.z * view.z) / fmaxf(distance, 0.001f);
    float side = fabsf(view.x * delta.z - view.z * delta.x);
    return dot >= -0.15f && side <= distance * 0.95f + 1.25f;
}

static int StreetCameraPointerAllowed(Vector2 pointer)
{
    Rectangle reserved[MAX_VEHICLES + 1];
    for (int row = 0; row < MAX_VEHICLES; row++)
        reserved[row] = ResponseUnitRowBounds(row);
    reserved[MAX_VEHICLES] = (Rectangle){mapClip.x,
        mapClip.y + mapClip.height - 56.0f, mapClip.width, 56.0f};
    return ResponseCameraPointerAllowed(pointer, mapClip, reserved,
                                        MAX_VEHICLES + 1);
}

static Vector3 StreetIncidentFirePoint(const Incident *incident)
{
    Vector3 fire = CityPointToStreetWorld(incident->position);
    if (incident->fireBuildingIndex >= 0 &&
        incident->fireBuildingIndex < CITY_LAYOUT_BUILDING_COUNT) {
        fire = CityRectCenterToStreetWorld(
            CITY_LAYOUT_BUILDINGS[incident->fireBuildingIndex]);
        fire.y = 4.0f + (float)(incident->fireBuildingIndex % 4) * 0.75f;
    } else {
        fire.y = 1.25f;
    }
    return fire;
}

static void DrawStreetResponder(Vector3 p, IncidentType service, int action,
                                float phase)
{
    Color uniform = service == INCIDENT_FIRE ? (Color){220, 113, 48, 255} :
                    service == INCIDENT_CRIME ? (Color){49, 111, 198, 255} :
                                                (Color){42, 160, 120, 255};
    float bob = sinf(phase) * 0.025f;
    Vector3 head = {p.x, p.y + 1.05f + bob, p.z};
    Vector3 shoulder = {p.x, p.y + 0.76f + bob, p.z};
    Vector3 chest = {p.x, p.y + 0.62f + bob, p.z};
    DrawSphere(head, 0.15f, (Color){224, 190, 162, 255});
    DrawCubeV((Vector3){p.x, p.y + 0.68f + bob, p.z},
              (Vector3){0.34f, 0.52f, 0.25f}, uniform);
    DrawCylinderEx((Vector3){p.x - 0.11f, p.y + 0.40f, p.z},
                   (Vector3){p.x - 0.13f + bob, p.y + 0.05f, p.z},
                   0.055f, 0.045f, 6, (Color){38, 48, 54, 255});
    DrawCylinderEx((Vector3){p.x + 0.11f, p.y + 0.40f, p.z},
                   (Vector3){p.x + 0.13f - bob, p.y + 0.05f, p.z},
                   0.055f, 0.045f, 6, (Color){38, 48, 54, 255});

    if (service == INCIDENT_FIRE) {
        DrawSphere((Vector3){head.x, head.y + 0.13f, head.z}, 0.17f,
                   (Color){246, 190, 61, 255});
        DrawLine3D((Vector3){p.x, p.y + 0.66f, p.z + 0.13f},
                   (Vector3){p.x, p.y + 0.66f, p.z - 0.13f},
                   (Color){247, 212, 92, 255});
        if (action == 1) {
            DrawCylinderEx((Vector3){p.x - 0.10f, shoulder.y, p.z},
                           (Vector3){p.x - 0.06f, p.y + 0.62f, p.z + 0.28f},
                           0.07f, 0.055f, 6, uniform);
            DrawCylinderEx((Vector3){p.x + 0.10f, shoulder.y, p.z},
                           (Vector3){p.x + 0.06f, p.y + 0.62f, p.z + 0.28f},
                           0.07f, 0.055f, 6, uniform);
            DrawSphere((Vector3){p.x, p.y + 0.62f, p.z + 0.30f}, 0.07f,
                       (Color){63, 74, 78, 255});
        } else {
            DrawCylinderEx((Vector3){p.x - 0.16f, shoulder.y, p.z},
                           (Vector3){p.x - 0.20f, p.y + 0.40f, p.z + 0.08f},
                           0.06f, 0.05f, 6, uniform);
            DrawCylinderEx((Vector3){p.x + 0.16f, shoulder.y, p.z},
                           (Vector3){p.x + 0.20f, p.y + 0.40f, p.z + 0.08f},
                           0.06f, 0.05f, 6, uniform);
        }
    } else if (service == INCIDENT_CRIME) {
        DrawCubeV((Vector3){head.x, head.y + 0.13f, head.z},
                  (Vector3){0.32f, 0.08f, 0.28f}, (Color){27, 53, 91, 255});
        if (action == 2) {
            DrawCylinderEx((Vector3){p.x - 0.15f, shoulder.y, p.z},
                           (Vector3){p.x - 0.48f, p.y + 0.79f, p.z},
                           0.06f, 0.045f, 6, uniform);
            DrawCylinderEx((Vector3){p.x + 0.15f, shoulder.y, p.z},
                           (Vector3){p.x + 0.48f, p.y + 0.82f, p.z},
                           0.06f, 0.045f, 6, uniform);
            DrawCubeV((Vector3){p.x + 0.51f, p.y + 0.84f, p.z},
                      (Vector3){0.06f, 0.28f, 0.06f}, (Color){255, 203, 82, 255});
        } else {
            DrawCylinderEx((Vector3){p.x + 0.15f, shoulder.y, p.z},
                           (Vector3){p.x + 0.28f, p.y + 0.50f, p.z + 0.12f},
                           0.06f, 0.05f, 6, uniform);
        }
    } else if (action == 3) {
        // A low, forward-leaning pose reads as patient care rather than a
        // standing bystander. It is drawn only during a staffed treatment stage.
        DrawCubeV((Vector3){p.x, p.y + 1.17f, p.z},
                  (Vector3){0.31f, 0.08f, 0.25f}, RAYWHITE);
        DrawCubeV((Vector3){p.x, p.y + 1.17f, p.z},
                  (Vector3){0.08f, 0.23f, 0.05f}, (Color){215, 58, 69, 255});
        DrawCubeV((Vector3){p.x, p.y + 1.17f, p.z},
                  (Vector3){0.23f, 0.08f, 0.05f}, (Color){215, 58, 69, 255});
        DrawCylinderEx((Vector3){p.x - 0.15f, chest.y, p.z},
                       (Vector3){p.x - 0.22f, p.y + 0.34f, p.z + 0.25f},
                       0.06f, 0.045f, 6, uniform);
        DrawCylinderEx((Vector3){p.x + 0.15f, chest.y, p.z},
                       (Vector3){p.x + 0.22f, p.y + 0.34f, p.z + 0.25f},
                       0.06f, 0.045f, 6, uniform);
    } else {
        DrawCylinderEx((Vector3){p.x - 0.15f, shoulder.y, p.z},
                       (Vector3){p.x - 0.20f, p.y + 0.42f, p.z + 0.05f},
                       0.06f, 0.05f, 6, uniform);
        DrawCylinderEx((Vector3){p.x + 0.15f, shoulder.y, p.z},
                       (Vector3){p.x + 0.20f, p.y + 0.42f, p.z + 0.05f},
                       0.06f, 0.05f, 6, uniform);
    }
}

static void DrawStreetPoliceCordon(Vector2 incidentPosition)
{
    Vector3 a = CityPointToStreetWorld((Vector2){incidentPosition.x - 18.0f,
                                                 incidentPosition.y - 15.0f});
    Vector3 b = CityPointToStreetWorld((Vector2){incidentPosition.x + 18.0f,
                                                 incidentPosition.y - 15.0f});
    a.y = b.y = 0.36f;
    DrawLine3D(a, b, (Color){68, 148, 235, 235});
    Vector3 coneA = a, coneB = b;
    coneA.y = coneB.y = 0.14f;
    DrawCylinder(coneA, 0.11f, 0.035f, 0.28f, 6, (Color){242, 151, 65, 255});
    DrawCylinder(coneB, 0.11f, 0.035f, 0.28f, 6, (Color){242, 151, 65, 255});
}

static void DrawStreetOnSceneWork(const Simulation *simulation, int incidentIndex,
                                  const Incident *incident)
{
    if (!incident || incidentIndex < 0 || IsIncidentFinished(incident)) return;
    Vector3 target = CityPointToStreetWorld(incident->position);
    int fireReady = HasRequiredOnSceneService(simulation, incidentIndex, INCIDENT_FIRE);
    int policeReady = HasRequiredOnSceneService(simulation, incidentIndex, INCIDENT_CRIME);
    int ambulanceReady = HasRequiredOnSceneService(simulation, incidentIndex,
                                                    INCIDENT_ACCIDENT);
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *unit = &simulation->vehicles[i];
        if (unit->assignedIncident != incidentIndex || !unit->arrived ||
            unit->activity != UNIT_ON_SCENE) continue;
        Vector3 engine = StreetVehicleDisplayPosition(simulation, i);
        Vector3 forward = ResponseUnitForward(unit);
        Vector3 right = {-forward.z, 0.0f, forward.x};
        Vector3 worker = {engine.x + right.x * 0.9f, 0.0f,
                          engine.z + right.z * 0.9f};
        if (incident->status == INCIDENT_FIREFIGHTING && fireReady &&
            unit->serviceType == INCIDENT_FIRE) {
            DrawStreetResponder(worker, INCIDENT_FIRE, 1,
                                simulation->elapsedTime * 5.0f + (float)i);
            Vector3 engineHose = {engine.x, 0.23f, engine.z};
            Vector3 hands = {worker.x, 0.64f, worker.z + 0.30f};
            DrawCylinderEx(engineHose, hands, 0.055f, 0.045f, 6,
                           (Color){49, 59, 62, 255});
            Vector3 fire = StreetIncidentFirePoint(incident);
            fire.y += 0.30f;
            // Bright core and droplets make the live water stream distinct from
            // the dark hose. All movement is keyed to simulation time/stage.
            DrawCylinderEx(hands, fire, 0.045f, 0.018f, 7,
                           (Color){107, 199, 245, 235});
            for (int drop = 1; drop <= 5; drop++) {
                float t = (float)drop / 6.0f;
                float arc = 0.35f * sinf(t * PI);
                float spray = sinf(simulation->elapsedTime * 18.0f +
                                   (float)(drop + i)) * 0.04f;
                Vector3 particle = {
                    hands.x + (fire.x - hands.x) * t,
                    hands.y + (fire.y - hands.y) * t + arc + spray,
                    hands.z + (fire.z - hands.z) * t
                };
                DrawSphere(particle, 0.055f - t * 0.018f,
                           (Color){165, 224, 252, 235});
            }
        } else if (incident->status == INCIDENT_SECURING && policeReady &&
                   unit->serviceType == INCIDENT_CRIME) {
            DrawStreetResponder(worker, INCIDENT_CRIME, 0,
                                simulation->elapsedTime * 2.0f + (float)i);
            DrawStreetPoliceCordon(incident->position);
        } else if (incident->status == INCIDENT_TREATING && ambulanceReady &&
                   unit->serviceType == INCIDENT_ACCIDENT) {
            Vector3 patient = {target.x + 0.42f, 0.0f, target.z + 0.36f};
            DrawCubeV((Vector3){patient.x, 0.13f, patient.z},
                      (Vector3){0.54f, 0.13f, 0.25f},
                      (Color){231, 235, 223, 255});
            DrawSphere((Vector3){patient.x + 0.30f, 0.20f, patient.z},
                       0.10f, (Color){229, 192, 163, 255});
            DrawStreetResponder(worker, INCIDENT_ACCIDENT, 3,
                                simulation->elapsedTime * 2.5f + (float)i);
            DrawLine3D((Vector3){worker.x, 0.48f, worker.z},
                       (Vector3){patient.x, 0.30f, patient.z},
                       (Color){74, 218, 164, 255});
            // Medical cross on the treatment bag signals care without adding
            // another patient or changing the simulation's actor count.
            DrawCubeV((Vector3){worker.x + 0.12f, 0.55f, worker.z},
                      (Vector3){0.08f, 0.18f, 0.05f}, RAYWHITE);
            DrawCubeV((Vector3){worker.x + 0.12f, 0.55f, worker.z},
                      (Vector3){0.18f, 0.07f, 0.05f}, RAYWHITE);
        } else if (unit->serviceType == INCIDENT_CRIME && policeReady &&
                   incident->status == INCIDENT_RESPONDING &&
                   incident->type == INCIDENT_CRIME) {
            DrawStreetResponder(worker, INCIDENT_CRIME, 0,
                                simulation->elapsedTime * 2.0f + (float)i);
            // Evidence markers are shown only during an active, staffed police
            // response. They are props, not additional evidence logic.
            Vector3 evidence = {target.x + 0.6f, 0.04f, target.z - 0.55f};
            DrawCylinder(evidence, 0.08f, 0.08f, 0.08f, 6,
                         (Color){255, 204, 92, 255});
        } else if (unit->serviceType == INCIDENT_CRIME && policeReady &&
                   incident->sceneSecured && incident->type == INCIDENT_ACCIDENT &&
                   incident->status == INCIDENT_TREATING) {
            // Scene security is complete; police can now direct traffic while
            // the required ambulance crew treats the patient.
            DrawStreetResponder(worker, INCIDENT_CRIME, 2,
                                simulation->elapsedTime * 3.0f + (float)i);
            Vector3 baton = {worker.x + 0.22f, 0.68f, worker.z};
            DrawLine3D((Vector3){worker.x, 0.63f, worker.z}, baton,
                       (Color){255, 205, 91, 255});
        } else if (unit->serviceType == INCIDENT_CRIME && policeReady &&
                   incident->needsSceneSecurity && incident->sceneSecured) {
            DrawStreetResponder(worker, INCIDENT_CRIME, 0,
                                simulation->elapsedTime * 2.0f + (float)i);
            DrawStreetPoliceCordon(incident->position);
        } else if (unit->serviceType == INCIDENT_FIRE) {
            // Arrived crews wait beside their engine while police secure a scene.
            DrawStreetResponder(worker, INCIDENT_FIRE, 0,
                                simulation->elapsedTime * 2.0f + (float)i);
        } else if (unit->serviceType == INCIDENT_ACCIDENT) {
            // Medics remain staged until the simulation enters patient care.
            DrawStreetResponder(worker, INCIDENT_ACCIDENT, 0,
                                simulation->elapsedTime * 2.0f + (float)i);
        }
    }
}

#define STREET_WEATHER_PARTICLE_COUNT 64

static Vector3 StreetCameraForward(Camera3D camera)
{
    Vector3 forward = {camera.target.x - camera.position.x,
                       camera.target.y - camera.position.y,
                       camera.target.z - camera.position.z};
    float length = sqrtf(forward.x * forward.x + forward.y * forward.y +
                         forward.z * forward.z);
    if (length < 0.001f) return (Vector3){0.0f, 0.0f, 1.0f};
    return (Vector3){forward.x / length, forward.y / length, forward.z / length};
}

static void DrawStreetWeather(CityCondition condition, Camera3D camera, float time)
{
    // Particle positions are regenerated from fixed indices and simulation
    // time: bounded at 64, repeatable, pause/speed aware, and allocation-free.
    if (condition != CITY_RAIN && condition != CITY_SNOW && condition != CITY_WIND)
        return;

    Vector3 forward = StreetCameraForward(camera);
    Vector3 right = {forward.z, 0.0f, -forward.x};
    float rightLength = sqrtf(right.x * right.x + right.z * right.z);
    if (rightLength < 0.001f) right = (Vector3){1.0f, 0.0f, 0.0f};
    else { right.x /= rightLength; right.z /= rightLength; }

    for (unsigned int i = 0; i < STREET_WEATHER_PARTICLE_COUNT; i++) {
        float seedX = WeatherSeed(i, 11u);
        float seedDepth = WeatherSeed(i, 12u);
        float seedPhase = WeatherSeed(i, 13u);
        float side = (seedX - 0.5f) * 10.0f;
        float depth = 2.0f + seedDepth * 8.0f;
        Vector3 center = {camera.position.x + right.x * side + forward.x * depth,
                          camera.position.y,
                          camera.position.z + right.z * side + forward.z * depth};

        if (condition == CITY_RAIN) {
            float fall = fmodf(time * 4.6f + seedPhase * 4.4f, 4.4f);
            center.y += 3.4f - fall;
            float length = 0.30f + WeatherSeed(i, 14u) * 0.20f;
            Vector3 end = {center.x - right.x * 0.07f,
                           center.y - length,
                           center.z - right.z * 0.07f};
            Vector3 shadowStart = {center.x + right.x * 0.025f, center.y,
                                   center.z + right.z * 0.025f};
            Vector3 shadowEnd = {end.x + right.x * 0.025f, end.y,
                                 end.z + right.z * 0.025f};
            DrawLine3D(shadowStart, shadowEnd, (Color){11, 30, 43, 185});
            DrawLine3D(center, end, (Color){221, 241, 250, 225});
        } else if (condition == CITY_SNOW) {
            float fall = fmodf(time * 0.65f + seedPhase * 5.0f, 5.0f);
            float drift = sinf(time * 1.2f + seedX * 13.0f) * 0.28f;
            center.x += drift;
            center.z += drift * 0.55f;
            center.y += 3.2f - fall;
            float radius = 0.035f + WeatherSeed(i, 15u) * 0.025f;
            DrawSphere((Vector3){center.x, center.y, center.z}, radius * 1.65f,
                       (Color){24, 45, 58, 190});
            DrawSphere((Vector3){center.x + 0.008f, center.y + 0.008f,
                                 center.z + 0.008f}, radius,
                       (Color){236, 246, 252, 240});
        } else {
            float y = camera.position.y + 0.9f + seedPhase * 2.3f;
            float gust = 0.20f + WeatherSeed(i, 16u) * 0.34f;
            center.y = y;
            Vector3 end = {center.x + right.x * gust,
                           center.y - 0.018f,
                           center.z + right.z * gust};
            DrawLine3D(center, end, (Color){17, 38, 50, 165});
            DrawLine3D((Vector3){center.x, center.y + 0.008f, center.z},
                       (Vector3){end.x, end.y + 0.008f, end.z},
                       (Color){195, 222, 235, 210});
        }
    }
}

static void DrawResponseStreetScene(const Simulation *simulation, int vehicleIndex,
                                    int firstPerson,
                                    const ResponseCameraState *cameraState)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount) return;
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    Vector3 vehicleWorld = StreetVehicleDisplayPosition(simulation, vehicleIndex);
    Vector3 forward = ResponseUnitForward(vehicle);

    Camera3D camera = ResponseCameraBuild(cameraState, vehicleWorld, forward,
                                          firstPerson);
    if (!firstPerson) {
        Vector2 targetMapPoint = CityLayoutStreetPointToMap(camera.target);
        Vector2 cameraMapPoint = CityLayoutStreetPointToMap(camera.position);
        cameraMapPoint = CityLayoutSafeCameraPoint(targetMapPoint,
                                                   cameraMapPoint, 7.0f);
        camera.position = CityPointToStreetWorld(cameraMapPoint);
        // Keep the chase camera above the roadway even at its closest zoom.
        if (camera.position.y < 1.55f) camera.position.y = 1.55f;
    }
    camera.position = KeepStreetPointInCity(camera.position);
    camera.target = KeepStreetPointInCity(camera.target);

    BeginMode3D(camera);
    Color ground = simulation->cityCondition == CITY_NIGHT ?
                   (Color){25, 42, 49, 255} :
                   simulation->cityCondition == CITY_RAIN ?
                   (Color){42, 65, 72, 255} :
                   simulation->cityCondition == CITY_SNOW ?
                   (Color){54, 73, 81, 255} :
                   (Color){48, 76, 75, 255};
    DrawPlane((Vector3){0.0f, CITY_LAYOUT_STREET_GROUND_Y, 0.0f},
              (Vector2){CITY_WIDTH * CITY_LAYOUT_STREET_SCALE + 1.0f,
                        CITY_HEIGHT * CITY_LAYOUT_STREET_SCALE + 1.0f},
              ground);
    DrawStreetRoadNetwork(simulation->cityCondition, camera.position);

    int incidentIndex = vehicle->assignedIncident;
    const Incident *incident = incidentIndex >= 0 && incidentIndex < simulation->incidentCount ?
                               &simulation->incidents[incidentIndex] : NULL;
    for (int i = 0; i < CITY_LAYOUT_BUILDING_COUNT; i++) {
        DrawStreetBuilding(CITY_LAYOUT_BUILDINGS[i], i,
                           incident && incident->fireBuildingIndex == i,
                           simulation->cityCondition, camera.position);
    }
    DrawStreetBuilding(CITY_LAYOUT_FIRE_STATION, CITY_LAYOUT_BUILDING_COUNT, 0,
                       simulation->cityCondition, camera.position);
    DrawStreetBuilding(CITY_LAYOUT_POLICE_STATION, CITY_LAYOUT_BUILDING_COUNT + 1, 0,
                       simulation->cityCondition, camera.position);
    DrawStreetBuilding(CITY_LAYOUT_HOSPITAL_FOOTPRINT,
                       CITY_LAYOUT_BUILDING_COUNT + 2, 0,
                       simulation->cityCondition, camera.position);
    DrawStreetServiceSign(CITY_LAYOUT_FIRE_STATION, "FIRE", NULL,
                          (Color){170, 47, 56, 255});
    DrawStreetServiceSign(CITY_LAYOUT_POLICE_STATION, "POLICE", NULL,
                          (Color){38, 103, 185, 255});
    DrawStreetHospitalEntrance(camera.position, simulation->cityCondition);
    DrawStreetFurniture(simulation->cityCondition, simulation->elapsedTime);

    // Civilian traffic and pedestrians are rendered from their current
    // simulation positions and behavior states. These are not extra actors.
    for (int i = 0; i < simulation->civilianCount; i++) {
        Vector3 carPosition = CityPointToStreetWorld(simulation->civilianCars[i].position);
        if (StreetPointVisibleToCamera(camera, carPosition))
            DrawStreetCivilianCar(&simulation->civilianCars[i], i);
    }
    for (int i = 0; i < simulation->pedestrianCount; i++) {
        const Pedestrian *person = &simulation->pedestrians[i];
        Vector3 p = CityPointToStreetWorld(person->position);
        if (!StreetPointVisibleToCamera(camera, p)) continue;
        DrawStreetPerson(p, StreetPedestrianColor(person->state),
                         simulation->elapsedTime * 5.0f + (float)i * 1.7f);
        if (person->state == PEDESTRIAN_EVACUATING ||
            person->state == PEDESTRIAN_BYSTANDING ||
            person->state == PEDESTRIAN_RESPECTING_CORDON) {
            DrawSphere((Vector3){p.x, 0.035f, p.z}, 0.16f,
                       Fade(StreetPedestrianColor(person->state), 0.55f));
        }
    }

    // Display real active routes for all viewable responders. The followed
    // unit is brighter; other assigned units remain visible as supporting crews.
    for (int r = 0; r < simulation->vehicleCount; r++) {
        const Vehicle *routed = &simulation->vehicles[r];
        Vector3 actualRouteVehicle = CityPointToStreetWorld(routed->position);
        if (routed->activity != UNIT_RESPONDING || routed->routeCount < 2 ||
            !StreetPointVisibleToCamera(camera, actualRouteVehicle)) continue;
        Vector3 displayedRouteStart = StreetVehicleDisplayPosition(simulation, r);
        Color routeColor = r == vehicleIndex ? (Color){54, 178, 255, 220} :
                           (Color){126, 171, 196, 110};
        for (int point = routed->routeIndex; point < routed->routeCount; point++) {
            Vector3 from = point == routed->routeIndex ? displayedRouteStart :
                           CityPointToStreetWorld(routed->route[point - 1]);
            Vector3 to = CityPointToStreetWorld(routed->route[point]);
            from.y = 0.13f;
            to.y = 0.13f;
            DrawLine3D(from, to, routeColor);
        }
    }

    if (incident) {
        // The destination marker uses the exact incident point used by the 2D
        // map and route planner; a linked fire-building index highlights its
        // actual footprint without moving that marker to an approximate spot.
        DrawStreetCrosswalkForIncident(incident->position);
        if (incident->type == INCIDENT_FIRE && !IsIncidentFinished(incident) &&
            !incident->fireContained) {
            Vector3 fire = CityPointToStreetWorld(incident->position);
            float intensity = incident->status == INCIDENT_FIREFIGHTING ? 0.52f : 1.0f;
            if (incident->fireBuildingIndex >= 0 &&
                incident->fireBuildingIndex < CITY_LAYOUT_BUILDING_COUNT) {
                Rectangle r = CITY_LAYOUT_BUILDINGS[incident->fireBuildingIndex];
                fire = CityRectCenterToStreetWorld(r);
                fire.y = 4.0f + (float)(incident->fireBuildingIndex % 4) * 0.75f;
            } else fire.y = 1.25f;
            float flicker = 0.12f * sinf(simulation->elapsedTime * 10.0f);
            DrawSphere((Vector3){fire.x, fire.y + 0.42f + flicker, fire.z},
                       0.36f * intensity, (Color){255, 114, 37, 230});
            DrawSphere((Vector3){fire.x - 0.27f, fire.y + 0.22f - flicker,
                                 fire.z + 0.13f},
                       0.24f * intensity, (Color){255, 191, 63, 240});
            DrawSphere((Vector3){fire.x + 0.25f, fire.y + 0.30f + flicker,
                                 fire.z - 0.12f},
                       0.26f * intensity, (Color){239, 75, 38, 225});
            for (int puff = 0; puff < 3; puff++) {
                float rise = fmodf(simulation->elapsedTime * 0.55f + puff * 0.42f,
                                   1.35f);
                float breeze = 0.12f * sinf((float)puff + simulation->elapsedTime);
                if (simulation->cityCondition == CITY_WIND) breeze += rise * 0.55f;
                DrawSphere((Vector3){fire.x + 0.35f + breeze,
                                     fire.y + 0.65f + rise, fire.z - 0.18f},
                           0.20f + rise * 0.13f,
                           Fade((Color){87, 92, 91, 255}, 0.52f - rise * 0.12f));
            }
        }
    }

    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *responder = &simulation->vehicles[i];
        Vector3 actualPosition = CityPointToStreetWorld(responder->position);
        if (!StreetPointVisibleToCamera(camera, actualPosition)) continue;
        Vector3 displayPosition = StreetVehicleDisplayPosition(simulation, i);
        DrawStreetEmergencyVehicle(responder, displayPosition, i == vehicleIndex,
                                   simulation->elapsedTime);
    }
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *nearIncident = &simulation->incidents[i];
        Vector3 incidentPosition = CityPointToStreetWorld(nearIncident->position);
        if (StreetPointVisibleToCamera(camera, incidentPosition))
            DrawStreetOnSceneWork(simulation, i, nearIncident);
    }
    DrawStreetWeather(simulation->cityCondition, camera, simulation->elapsedTime);
    EndMode3D();
}

static void RenderResponseStreetView(RenderTexture2D target,
                                     const Simulation *simulation, int vehicleIndex,
                                     int firstPerson,
                                     const ResponseCameraState *cameraState)
{
    BeginTextureMode(target);
    Color sky = simulation->cityCondition == CITY_NIGHT ? (Color){16, 27, 43, 255} :
                simulation->cityCondition == CITY_FOG ? (Color){106, 132, 145, 255} :
                simulation->cityCondition == CITY_RAIN ? (Color){70, 98, 118, 255} :
                simulation->cityCondition == CITY_SNOW ? (Color){117, 144, 160, 255} :
                simulation->cityCondition == CITY_WIND ? (Color){104, 133, 151, 255} :
                                                        (Color){129, 164, 180, 255};
    ClearBackground(sky);
    DrawResponseStreetScene(simulation, vehicleIndex, firstPerson, cameraState);
    EndTextureMode();
}

static void DrawResponseStreetViewTexture(RenderTexture2D source)
{
    Rectangle src = {0.0f, 0.0f, (float)source.texture.width,
                     -(float)source.texture.height};
    Rectangle dst = {mapClip.x * UI_RENDER_SCALE, mapClip.y * UI_RENDER_SCALE,
                     mapClip.width * UI_RENDER_SCALE, mapClip.height * UI_RENDER_SCALE};
    DrawTexturePro(source.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
}

static void DrawResponseStreetHud(const Simulation *simulation, int vehicleIndex,
                                 int ordinal, int unitCount, int firstPerson)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount) return;
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    int incidentIndex = vehicle->assignedIncident;
    if (incidentIndex < 0 || incidentIndex >= simulation->incidentCount) return;
    const Incident *incident = &simulation->incidents[incidentIndex];

    Rectangle hud = {mapClip.x + 9, mapClip.y + mapClip.height - 53,
                     mapClip.width - 18, 44};
    DrawRectangleRounded(hud, 0.12f, 8, (Color){5, 20, 32, 232});
    DrawRectangleRoundedLines(hud, 0.12f, 8, (Color){63, 125, 157, 220});

    DrawText(TextFormat("RESPONSE UNITS  %d/%d", ordinal + 1, unitCount),
             (int)mapClip.x + 15, (int)mapClip.y + 3, 8,
             (Color){176, 204, 218, 255});
    DrawText(firstPerson ? "FIRST PERSON VIEW" : "DRAG TO LOOK  |  WHEEL TO ZOOM",
             (int)mapClip.x + 555, (int)mapClip.y + 3, 8,
             (Color){176, 204, 218, 255});
    for (int row = 0; row < unitCount && row < MAX_VEHICLES; row++) {
        int rowVehicleIndex = ResponseUnitAt(simulation, incidentIndex, row);
        if (rowVehicleIndex < 0) continue;
        const Vehicle *rowVehicle = &simulation->vehicles[rowVehicleIndex];
        Rectangle rowBounds = ResponseUnitRowBounds(row);
        int selected = rowVehicleIndex == vehicleIndex;
        DrawRectangleRounded(rowBounds, 0.12f, 4,
            selected ? (Color){20, 72, 102, 242} : (Color){5, 22, 35, 220});
        DrawRectangleRoundedLines(rowBounds, 0.12f, 4,
            selected ? SKYBLUE : (Color){48, 86, 105, 210});
        DrawVehicleIcon(rowVehicle->serviceType,
            (Vector2){rowBounds.x + 12, rowBounds.y + rowBounds.height * 0.5f});
        const char *rowState = rowVehicle->arrived || rowVehicle->activity == UNIT_ON_SCENE ?
                               "ON SCENE" : "EN ROUTE";
        if (rowVehicle->arrived || rowVehicle->activity == UNIT_ON_SCENE) {
            DrawText(TextFormat("%s %d  |  %s", ServiceName(rowVehicle->serviceType),
                     ServiceUnitNumber(simulation, rowVehicleIndex), rowState),
                     (int)rowBounds.x + 25, (int)rowBounds.y + 8, 7,
                     selected ? RAYWHITE : (Color){190, 205, 213, 255});
        } else {
            float rowEta = SimulationEstimateUnitETA(simulation, rowVehicleIndex,
                                                      incidentIndex);
            DrawText(rowEta >= 0.0f ? TextFormat("%s %d  |  EN ROUTE  %.1fs",
                     ServiceName(rowVehicle->serviceType),
                     ServiceUnitNumber(simulation, rowVehicleIndex), rowEta) :
                     TextFormat("%s %d  |  %s", ServiceName(rowVehicle->serviceType),
                     ServiceUnitNumber(simulation, rowVehicleIndex), rowState),
                     (int)rowBounds.x + 25, (int)rowBounds.y + 8, 7,
                     selected ? RAYWHITE : (Color){190, 205, 213, 255});
        }
    }

    const char *unitState = "EN ROUTE";
    const char *detail = "ETA --";
    if (vehicle->arrived || vehicle->activity == UNIT_ON_SCENE) {
        unitState = "ON SCENE";
        detail = TextFormat("ARRIVED  |  %s", IncidentStatusName(incident->status));
    } else {
        float eta = SimulationEstimateUnitETA(simulation, vehicleIndex, incidentIndex);
        detail = eta >= 0.0f ? TextFormat("ETA %.1fs  |  NEXT: CALL %.0f, %.0f", eta,
                                          incident->position.x, incident->position.y) :
                               TextFormat("ETA --  |  NEXT: CALL %.0f, %.0f",
                                          incident->position.x, incident->position.y);
    }
    DrawText(TextFormat("%s %d  /  %s", ServiceName(vehicle->serviceType),
                        ServiceUnitNumber(simulation, vehicleIndex),
                        IncidentTypeName(incident->type)),
             (int)hud.x + 12, (int)hud.y + 7, 10, RAYWHITE);
    DrawText(TextFormat("%s  |  %s", unitState, detail),
             (int)hud.x + 12, (int)hud.y + 24, 8,
             vehicle->arrived ? (Color){106, 222, 164, 255} :
                                (Color){164, 195, 210, 255});

    DrawButton(responsePreviousButton, "PREV", unitCount > 1 ? BLUE : DARKGRAY);
    DrawButton(responseNextButton, "NEXT", unitCount > 1 ? BLUE : DARKGRAY);
    DrawButton(responseCameraButton, firstPerson ? "CHASE CAM" : "FIRST PERSON",
               (Color){34, 93, 130, 255});
    DrawButton(responseCameraResetButton, "RESET 3P",
               (Color){34, 93, 130, 255});
}

static void DrawMapContents(const Simulation *simulation, int selectedIncident, int selectedCall,
                           Rectangle cityView, bool showRoutes, bool showCivilianTraffic)
{
    Camera2D mapCamera = {0};
    mapCamera.target = (Vector2){CITY_WIDTH * 0.5f, CITY_HEIGHT * 0.5f};
    // BeginMode2D resets to an identity matrix. Since this nested camera draws
    // directly into the 2x render texture, scale its viewport into render pixels.
    mapCamera.offset = (Vector2){(cityView.x + cityView.width * 0.5f) * UI_RENDER_SCALE,
                                 (cityView.y + cityView.height * 0.5f) * UI_RENDER_SCALE};
    mapCamera.zoom = (cityView.width / CITY_WIDTH) * UI_RENDER_SCALE;

    BeginScissorMode((int)(mapClip.x * UI_RENDER_SCALE),
                     (int)(mapClip.y * UI_RENDER_SCALE),
                     (int)(mapClip.width * UI_RENDER_SCALE),
                     (int)(mapClip.height * UI_RENDER_SCALE));
    BeginMode2D(mapCamera);
    DrawCity(simulation);
    DrawCityWeather(simulation);
    if (showCivilianTraffic)
        for (int i = 0; i < simulation->civilianCount; i++)
            DrawCivilianCar(&simulation->civilianCars[i]);
    for (int i = 0; i < simulation->pedestrianCount; i++)
        DrawPedestrian(&simulation->pedestrians[i], simulation->elapsedTime);

    for (int i = 0; showRoutes && i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (vehicle->available || vehicle->activity == UNIT_ON_SCENE || vehicle->routeCount < 2) continue;

        Color routeColor = vehicle->serviceType == INCIDENT_FIRE ? RED :
                           vehicle->serviceType == INCIDENT_CRIME ? BLUE :
                           (Color){45, 199, 177, 255};
        int isSelectedRoute = vehicle->assignedIncident == selectedIncident;
        Vector2 from = vehicle->position;
        for (int point = vehicle->routeIndex; point < vehicle->routeCount; point++) {
            Vector2 to = vehicle->route[point];
            float routeWidth = isSelectedRoute ? 2.6f : 1.6f;
            DrawLineEx(from, to, routeWidth + 1.5f, Fade((Color){7, 20, 29, 255}, 0.8f));
            DrawLineEx(from, to, routeWidth,
                       Fade(routeColor, isSelectedRoute ? 0.92f : 0.46f));
            from = to;
        }
    }

    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        Vector2 markerPosition = MapVehicleDisplayPosition(simulation, i);
        DrawVehicleIcon(vehicle->serviceType, markerPosition);
        if (vehicle->activity == UNIT_TRANSPORTING) {
            // A small passenger marker rides with the ambulance only during transport.
            DrawRectangle((int)markerPosition.x - 3, (int)markerPosition.y - 9, 7, 2,
                          (Color){228, 239, 232, 255});
            DrawCircle((int)markerPosition.x - 1, (int)markerPosition.y - 11, 1.5f,
                       (Color){238, 199, 163, 255});
            DrawLine((int)markerPosition.x + 1, (int)markerPosition.y - 9,
                     (int)markerPosition.x + 5, (int)markerPosition.y - 9,
                     (Color){77, 220, 162, 255});
            DrawText("TO HOSPITAL", (int)markerPosition.x + 10,
                     (int)markerPosition.y - 11, 6, (Color){114, 224, 190, 255});
        } else if (vehicle->activity == UNIT_RETURNING)
            DrawText("RETURNING", (int)markerPosition.x + 10,
                     (int)markerPosition.y - 11, 6, (Color){190, 205, 216, 255});
    }

    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (incident->needsSceneSecurity && incident->sceneSecured && !IsIncidentFinished(incident)) {
            DrawCircleLines((int)incident->position.x, (int)incident->position.y, 37.0f,
                           Fade((Color){78, 164, 245, 255}, 0.8f));
            DrawCircleLines((int)incident->position.x, (int)incident->position.y, 41.0f,
                           Fade((Color){78, 164, 245, 255}, 0.35f));
            DrawText("POLICE CORDON", (int)incident->position.x - 33,
                     (int)incident->position.y + 40, 7, (Color){120, 188, 245, 255});
        }
        if (IsIncidentFinished(incident)) continue;

        if (incident->status == INCIDENT_SECURING &&
            HasRequiredOnSceneService(simulation, i, INCIDENT_CRIME)) {
            DrawLineEx((Vector2){incident->position.x - 21, incident->position.y - 17},
                       (Vector2){incident->position.x + 21, incident->position.y - 17},
                       2.0f, Fade((Color){87, 164, 246, 255}, 0.82f));
            DrawLineEx((Vector2){incident->position.x - 21, incident->position.y + 17},
                       (Vector2){incident->position.x + 21, incident->position.y + 17},
                       2.0f, Fade((Color){87, 164, 246, 255}, 0.82f));
            for (int v = 0; v < simulation->vehicleCount; v++) {
                const Vehicle *unit = &simulation->vehicles[v];
                if (unit->assignedIncident == i && unit->serviceType == INCIDENT_CRIME &&
                    unit->arrived && unit->activity == UNIT_ON_SCENE)
                    DrawResponderFigure((Vector2){unit->position.x + 7, unit->position.y + 7},
                                        (Color){104, 170, 244, 255}, "SECURE");
            }
        }

        if (incident->status == INCIDENT_FIREFIGHTING &&
            HasRequiredOnSceneService(simulation, i, INCIDENT_FIRE)) {
            for (int v = 0; v < simulation->vehicleCount; v++) {
                const Vehicle *unit = &simulation->vehicles[v];
                if (unit->assignedIncident != i || unit->serviceType != INCIDENT_FIRE ||
                    !unit->arrived || unit->activity != UNIT_ON_SCENE)
                    continue;
                Vector2 nozzle = {unit->position.x, unit->position.y - 7};
                DrawLineEx(nozzle, incident->position, 4.0f, (Color){77, 176, 239, 175});
                DrawLineEx(nozzle, incident->position, 1.6f, (Color){197, 233, 255, 240});
                DrawCircleV(incident->position, 3.5f, (Color){160, 220, 255, 220});
                DrawResponderFigure((Vector2){unit->position.x + 8, unit->position.y + 7},
                                    (Color){255, 171, 84, 255}, "HOSE");
            }
        }

        if (incident->status == INCIDENT_TREATING &&
            HasRequiredOnSceneService(simulation, i, INCIDENT_ACCIDENT)) {
            Vector2 patient = {incident->position.x + 16, incident->position.y + 10};
            DrawLineEx((Vector2){patient.x - 5, patient.y + 2},
                       (Vector2){patient.x + 6, patient.y + 2}, 2.5f,
                       (Color){226, 237, 235, 255});
            DrawCircle((int)patient.x - 3, (int)patient.y - 1, 2.0f,
                       (Color){237, 198, 164, 255});
            DrawResponderFigure((Vector2){patient.x + 9, patient.y - 4},
                                (Color){82, 224, 171, 255}, "TREAT");
            for (int v = 0; v < simulation->vehicleCount; v++) {
                const Vehicle *unit = &simulation->vehicles[v];
                if (unit->assignedIncident == i && unit->serviceType == INCIDENT_ACCIDENT &&
                    unit->arrived && unit->activity == UNIT_ON_SCENE)
                    DrawLineEx((Vector2){unit->position.x, unit->position.y + 5}, patient,
                               1.3f, Fade((Color){82, 224, 171, 255}, 0.75f));
            }
        }

        int policeOnScene = HasRequiredOnSceneService(simulation, i, INCIDENT_CRIME);
        const char *policeTask = NULL;
        Color policeColor = (Color){107, 172, 245, 255};
        if (incident->type == INCIDENT_CRIME && incident->status == INCIDENT_RESPONDING &&
            policeOnScene)
            policeTask = incident->severity >= 3 ? "ARREST" : "QUESTION";
        else if (incident->status != INCIDENT_SECURING && policeOnScene &&
                 (incident->type == INCIDENT_FIRE || incident->type == INCIDENT_ACCIDENT) &&
                 (incident->status == INCIDENT_FIREFIGHTING ||
                  incident->status == INCIDENT_TREATING ||
                  incident->status == INCIDENT_TRANSPORTING)) {
            policeTask = "TRAFFIC";
            policeColor = (Color){255, 207, 93, 255};
        }
        if (policeTask) {
            for (int v = 0; v < simulation->vehicleCount; v++) {
                const Vehicle *unit = &simulation->vehicles[v];
                if (unit->assignedIncident != i || unit->serviceType != INCIDENT_CRIME ||
                    !unit->arrived || unit->activity != UNIT_ON_SCENE) continue;
                Vector2 officer = {unit->position.x + 8, unit->position.y + 7};
                DrawResponderFigure(officer, policeColor, policeTask);
                if (strcmp(policeTask, "TRAFFIC") == 0) {
                    DrawLineEx((Vector2){officer.x - 5, officer.y - 1},
                               (Vector2){officer.x + 5, officer.y - 1}, 1.4f, policeColor);
                    DrawTriangle((Vector2){officer.x + 5, officer.y - 4},
                                 (Vector2){officer.x + 9, officer.y - 1},
                                 (Vector2){officer.x + 5, officer.y + 2}, policeColor);
                } else if (strcmp(policeTask, "ARREST") == 0) {
                    DrawCircleLines((int)officer.x + 5, (int)officer.y - 1, 2.2f, policeColor);
                    DrawCircleLines((int)officer.x + 9, (int)officer.y - 1, 2.2f, policeColor);
                } else {
                    DrawCircle((int)officer.x + 7, (int)officer.y - 8, 1.0f, RAYWHITE);
                    DrawCircle((int)officer.x + 10, (int)officer.y - 10, 1.0f, RAYWHITE);
                }
            }
        }
    }


    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        DrawActiveIncidentEffect(incident, i, simulation->elapsedTime, simulation->cityCondition);
        Color color = incident->status == INCIDENT_RESOLVED ? GREEN :
                      incident->status == INCIDENT_FAILED ? GRAY :
                      incident->severity >= 3 ? RED :
                      incident->severity == 2 ? ORANGE : IncidentColor(incident->type);
        float radius = incident->severity == 1 ? 7.0f :
                       incident->severity == 2 ? 10.0f : 13.0f;
        DrawCircleV(incident->position, radius, Fade(color, 0.32f));
        DrawCircleLines((int)incident->position.x, (int)incident->position.y, radius, color);
        DrawIncidentIcon(incident->type, incident->position, radius * 0.58f, color);
        if (incident->status == INCIDENT_RESOLVED) {
            DrawLineEx((Vector2){incident->position.x - 4, incident->position.y + 1},
                       (Vector2){incident->position.x - 1, incident->position.y + 4}, 2, GREEN);
            DrawLineEx((Vector2){incident->position.x - 1, incident->position.y + 4},
                       (Vector2){incident->position.x + 5, incident->position.y - 4}, 2, GREEN);
        } else if (incident->status == INCIDENT_FAILED) {
            DrawLineEx((Vector2){incident->position.x - 4, incident->position.y - 4},
                       (Vector2){incident->position.x + 4, incident->position.y + 4}, 2, GRAY);
            DrawLineEx((Vector2){incident->position.x - 4, incident->position.y + 4},
                       (Vector2){incident->position.x + 4, incident->position.y - 4}, 2, GRAY);
        }
        if (i == selectedIncident)
            DrawCircleLines((int)incident->position.x, (int)incident->position.y,
                            radius + 6.0f, GOLD);
        if (i == selectedIncident) {
            const char *status = IncidentStatusName(incident->status);
            const char *label = IsIncidentFinished(incident) ||
                                incident->status == INCIDENT_SECURING ||
                                incident->status == INCIDENT_FIREFIGHTING ||
                                incident->status == INCIDENT_TREATING ||
                                incident->status == INCIDENT_TRANSPORTING ? status :
                                TextFormat("SEV %d  UNITS %d/%d", incident->severity,
                                           CountAssignedUnits(simulation, i),
                                           CountRequiredUnits(incident));
            const char *incidentLabel = TextFormat("CALL %02d  %s",
                incident->callIndex >= 0 ? incident->callIndex + 1 : i + 1, label);
            int labelWidth = MeasureUiText(incidentLabel, 8) + 10;
            Rectangle labelBox = {incident->position.x + 15,
                                   incident->position.y - 10,
                                   (float)labelWidth, 18};
            if (labelBox.x + labelBox.width > CITY_WIDTH - 4)
                labelBox.x = incident->position.x - labelBox.width - 15;
            DrawRectangleRounded(labelBox, 0.22f, 5, (Color){7, 22, 32, 238});
            DrawRectangleRoundedLinesEx(labelBox, 0.22f, 5, 1.0f,
                                        Fade(color, 0.95f));
            DrawText(incidentLabel, (int)labelBox.x + 5,
                     (int)labelBox.y + 4, 8, RAYWHITE);
        }
    }
    for (int i = 0; i < simulation->callCount; i++) {
        const EmergencyCall *call = &simulation->calls[i];
        if (call->incidentIndex >= 0) continue;
        int selected = i == selectedCall;
        DrawCircleV(call->location, selected ? 15.0f : 11.0f,
                    selected ? (Color){255, 195, 82, 210} : (Color){134, 86, 28, 190});
        DrawCircleLines((int)call->location.x, (int)call->location.y,
                        selected ? 17.0f : 11.0f, selected ? GOLD : ORANGE);
        if (selected)
            DrawCircleLines((int)call->location.x, (int)call->location.y, 21.0f,
                            Fade(GOLD, 0.75f));
        DrawText("?", (int)call->location.x - 4, (int)call->location.y - 7, 14, RAYWHITE);
        if (selected) {
            const char *label = TextFormat("SELECTED CALL #%02d", i + 1);
            int labelWidth = MeasureUiText(label, 8) + 10;
            Rectangle labelBox = {call->location.x + 18, call->location.y - 11,
                                  (float)labelWidth, 18};
            if (labelBox.x + labelBox.width > CITY_WIDTH - 4)
                labelBox.x = call->location.x - labelBox.width - 18;
            DrawRectangleRounded(labelBox, 0.22f, 5, (Color){7, 22, 32, 240});
            DrawRectangleRoundedLinesEx(labelBox, 0.22f, 5, 1.0f, GOLD);
            DrawText(label, (int)labelBox.x + 5, (int)labelBox.y + 4, 8, RAYWHITE);
        }
    }
    EndMode2D();
    EndScissorMode();
}

static void DrawModalBackdrop(void)
{
    DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){0, 0, 0, 155});
    DrawPanel(modalPanel);
}

static void DrawReportsOverlay(const Simulation *simulation)
{
    int resolved = SimulationCountStatus(simulation, INCIDENT_RESOLVED);
    int onTime, recorded;
    float totalResponse = 0.0f;
    int responseCount = 0;
    float totalDispatchTime = 0.0f;
    int dispatchedCalls = 0;
    OnTimeArrivalCounts(simulation, &onTime, &recorded);
    for (int service = 0; service < SERVICE_COUNT; service++) {
        totalResponse += simulation->totalResponseTimeByType[service];
        responseCount += simulation->resolvedByType[service];
    }
    for (int i = 0; i < simulation->callCount; i++) {
        const EmergencyCall *call = &simulation->calls[i];
        if (!call->dispatched) continue;
        totalDispatchTime += call->dispatchedAt - call->receivedAt;
        dispatchedCalls++;
    }

    DrawModalBackdrop();
    DrawText("SIMULATION REPORT", 510, 245, 24, RAYWHITE);
    DrawText("Summary from the current simulation", 512, 280, 13,
             (Color){154, 181, 198, 255});
    DrawLine(510, 310, 1090, 310, (Color){43, 67, 83, 255});
    DrawText(TextFormat("Incidents created             %d", simulation->incidentCount),
             520, 335, 15, RAYWHITE);
    DrawText(TextFormat("Active / resolved / failed    %d / %d / %d",
             SimulationCountUnresolved(simulation), resolved,
             SimulationCountStatus(simulation, INCIDENT_FAILED)), 520, 369, 15, RAYWHITE);
    DrawText(responseCount > 0 ? TextFormat("Overall average response     %.1fs",
             totalResponse / responseCount) : "Overall average response     --",
             520, 403, 15, RAYWHITE);
    DrawText(recorded > 0 ? TextFormat("First arrivals on time       %d / %d",
             onTime, recorded) : "First arrivals on time       -- (no arrivals yet)",
             520, 437, 15, RAYWHITE);
    DrawText(dispatchedCalls > 0 ? TextFormat("Average call-to-dispatch     %.1fs (%d calls)",
             totalDispatchTime / dispatchedCalls, dispatchedCalls) :
             "Average call-to-dispatch     -- (no dispatches yet)",
             520, 463, 13, RAYWHITE);
    DrawLine(510, 490, 1090, 490, (Color){43, 67, 83, 255});
    DrawText("SERVICE RESPONSE AVERAGES", 520, 506, 13,
             (Color){107, 190, 238, 255});
    for (int i = 0; i < SERVICE_COUNT; i++) {
        IncidentType service = i == 2 ? INCIDENT_ACCIDENT : (IncidentType)i;
        float average = AverageResponseForService(simulation, service);
        DrawText(average >= 0.0f ? TextFormat("%-15s  %.1fs  (%d resolved)",
                 ServiceName(service), average, simulation->resolvedByType[service]) :
                 TextFormat("%-15s  --  (no resolved calls)", ServiceName(service)),
                 520, 538 + i * 25, 13, RAYWHITE);
    }
    DrawButton(modalCloseButton, "CLOSE", BLUE);
}

static void DrawSettingsOverlay(bool showRoutes, bool showCivilianTraffic,
                                const Simulation *simulation, int helpScroll)
{
    DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){0, 0, 0, 155});
    DrawPanel(settingsPanel);
    DrawText("CONTROL ROOM SETTINGS", 460, 156, 30, RAYWHITE);
    DrawText("Conditions update the map and Street View together.", 462, 196, 16,
             (Color){154, 181, 198, 255});
    DrawButton(routesToggleButton, showRoutes ? "RESPONSE ROUTES  ON" : "RESPONSE ROUTES  OFF",
               showRoutes ? (Color){31, 92, 132, 255} : DARKGRAY);
    DrawButton(civilianToggleButton,
               showCivilianTraffic ? "CIVILIAN TRAFFIC  ON" : "CIVILIAN TRAFFIC  OFF",
               showCivilianTraffic ? (Color){31, 92, 132, 255} : DARKGRAY);
    DrawButton(cityConditionButton,
               TextFormat("CITY CONDITIONS: %s  (CLICK TO CHANGE)",
                          SimulationCityConditionName(simulation->cityCondition)),
               (Color){31, 92, 132, 255});
    const char *autoLabel = simulation->autoIncidentCapacitySuspended ?
        "AUTO INCIDENTS: ON  |  PAUSED - CAPACITY FULL" :
        simulation->autoIncidentsEnabled ?
        TextFormat("AUTO INCIDENTS: ON  |  NEXT CALL IN %.1fs",
                   SimulationAutoIncidentSecondsUntilNext(simulation)) :
        "AUTO INCIDENTS: OFF";
    DrawButton(autoIncidentsButton, autoLabel,
               simulation->autoIncidentsEnabled ? (Color){24, 112, 87, 255} :
                                                  (Color){31, 92, 132, 255});
    DrawText("CONTROLS & HELP  ·  SCROLL INSIDE THIS PANEL FOR MORE",
             472, 529, 12, (Color){107, 190, 238, 255});
    Rectangle helpPanel = {452, 550, 696, 146};
    DrawRectangleRounded(helpPanel, 0.035f, 8, (Color){8, 24, 37, 255});
    DrawRectangleRoundedLinesEx(helpPanel, 0.035f, 8, 1,
                                (Color){39, 73, 96, 255});
    static const char *helpLines[SETTINGS_HELP_LINE_COUNT] = {
        "KEYS  1 Fire · 2 Police · 3 Traffic accident · 4 Medical; selector click cycles types.",
        "A starts the apartment-fire drill; Shift+A starts its minor version.",
        "Space pause/resume · S slow · F fast · N step 0.1s · T traffic · W weather.",
        "M simulate queue · C compare strategies · R reset · Esc close overlays.",
        "MAP  Click an empty map location to create a call using NEXT CALL TYPE.",
        "Click a call/incident marker to inspect it; click a queue card to select or clear it.",
        "MANUAL  Classify the call and priority, select suitable units, then dispatch.",
        "AUTO  The operator classifies and dispatches; timed human override is Auto-only.",
        "Manual unit list: click an available row or checkbox to toggle that unit.",
        "STREET VIEW  Click a response-unit row or use PREV/NEXT to switch units.",
        "Drag in the 3D scene to orbit; scroll there to zoom; RESET 3P resets the chase view.",
        "FIRST PERSON toggles camera mode; BACK TO MAP returns to the 2D map.",
        "BOTTOM BAR  Compare, Pause/Resume, Slow, 1x, Fast, Step, and Manual Dispatch.",
        "Traffic cycles traffic levels · Auto/Manual switches operator · Sim Queue runs the queue.",
        "Reports opens metrics · Settings opens options/help · Reset restarts the simulation.",
        "The selected call and NEXT CALL TYPE are separate; creating a call keeps the type selected."
    };
    const int helpCount = SETTINGS_HELP_LINE_COUNT;
    const int visibleHelpLines = SETTINGS_HELP_VISIBLE_LINES;
    if (helpScroll < 0) helpScroll = 0;
    if (helpScroll > helpCount - visibleHelpLines) helpScroll = helpCount - visibleHelpLines;
    DrawText(TextFormat("HELP %d–%d OF %d", helpScroll + 1,
             helpScroll + visibleHelpLines, helpCount), 1010, 532, 8,
             (Color){154, 181, 198, 255});
    for (int row = 0; row < visibleHelpLines; row++) {
        int line = helpScroll + row;
        DrawText(helpLines[line], 470, 559 + row * 18, 9,
                 line < 4 ? (Color){186, 211, 224, 255} : RAYWHITE);
    }
    DrawButton(settingsCloseButton, "CLOSE", BLUE);
}



int main(void)

{

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Emergency City Simulation");
    SetWindowMinSize(1120, 630);
    LoadUiFont();

    SetTargetFPS(60);

    RenderTexture2D uiCanvas = LoadRenderTexture(UI_WIDTH * UI_RENDER_SCALE,
                                                  UI_HEIGHT * UI_RENDER_SCALE);
    SetTextureFilter(uiCanvas.texture, TEXTURE_FILTER_BILINEAR);
    RenderTexture2D responseCanvas = LoadRenderTexture(
        (int)(mapClip.width * UI_RENDER_SCALE), (int)(mapClip.height * UI_RENDER_SCALE));
    if (responseCanvas.id != 0)
        SetTextureFilter(responseCanvas.texture, TEXTURE_FILTER_BILINEAR);
    Camera2D renderCamera = {0};
    renderCamera.target = (Vector2){0, 0};
    renderCamera.zoom = UI_RENDER_SCALE;



    Simulation simulation;

    SimulationInit(&simulation);



    int selectedType = INCIDENT_FIRE;

    bool paused = false;

    bool fastForward = false;
    bool slowMotion = false;
    bool comparisonReady = false;
    bool showRoutes = true;
    bool showCivilianTraffic = true;
    bool responseView = false;
    bool responseFirstPerson = false;
    bool responseCameraDragging = false;
    Vector2 responseCameraLastMouse = {0};
    ResponseCameraState responseCameraState = {0};
    ResponseCameraReset(&responseCameraState);
    int responseVehicleIndex = -1;
    int responseIncidentIndex = -1;
    int responseUnitOrdinal = 0;
    UIOverlay overlay = OVERLAY_NONE;
    int selectedCall = -1;
    int selectedIncident = -1;
    int settingsHelpScroll = 0;
    IncidentType manualCallType = INCIDENT_FIRE;
    int manualCallSeverity = 1;
    unsigned int selectedVehicleMask = 0;
    StrategyComparison comparison = {0};
    int overrideModalCall = -1;
    bool overrideResumeWhenDone = false;
    double overrideModalDeadline = 0.0;
    char operatorNotice[CALL_REASON_LENGTH] = "";
    int operatorNoticeMatched = 0;
    double operatorNoticeUntil = 0.0;



    while (!WindowShouldClose()) {

        // Fit the fixed design canvas inside the current client area. The same
        // viewport is used to convert pointer positions, so letterboxing and
        // high-DPI scaling cannot shift clicks away from the drawn controls.
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        Rectangle viewport = FitVirtualViewport(screenWidth, screenHeight,
                                                UI_WIDTH, UI_HEIGHT);

        bool modalWasActive = overrideModalCall >= 0;
        int modalCallAtFrameStart = overrideModalCall;
        if (overrideModalCall >= 0 && GetTime() >= overrideModalDeadline) {
            IncidentType aiType;
            int aiSeverity;
            if (SimulationSuggestCallClassification(&simulation, overrideModalCall,
                                                   &aiType, &aiSeverity))
                AcceptOperatorOverride(&simulation, overrideModalCall, aiType, aiSeverity,
                                       &paused, overrideResumeWhenDone, &overrideModalCall,
                                       &overrideModalDeadline);
            else {
                simulation.calls[overrideModalCall].overridePending = 0;
                overrideModalCall = -1;
                paused = !overrideResumeWhenDone;
            }
        }

        bool stepRequested = false;
        if (!modalWasActive && overrideModalCall < 0) {
        for (int typeIndex = 0; typeIndex < 4; typeIndex++) {
            int callTypeKey = IncidentNumberKeyAt(typeIndex);
            if (IsKeyPressed(callTypeKey)) {
                selectedType = IncidentTypeForNumberKey(callTypeKey);
                break;
            }
        }

        if (IsKeyPressed(KEY_A)) {
            int drillSeverity = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 1 : 3;
            if (SimulationStartApartmentFireDrillAtSeverity(&simulation, drillSeverity)) {
                responseView = false;
                responseVehicleIndex = -1;
                responseIncidentIndex = -1;
                selectedCall = 0;
                selectedIncident = -1;
                selectedVehicleMask = 0;
                selectedType = INCIDENT_FIRE;
                manualCallType = INCIDENT_FIRE;
                manualCallSeverity = drillSeverity;
                comparisonReady = false;
                if (simulation.dispatchMode == DISPATCH_AUTOMATIC) {
                    simulation.calls[0].overridePending = 1;
                    StartPendingOverrideModal(&simulation, &paused,
                        &overrideResumeWhenDone, &overrideModalCall,
                        &overrideModalDeadline);
                } else {
                    SimulationProcessOperator(&simulation);
                }
            }
        }

        if (IsKeyPressed(KEY_SPACE)) paused = !paused;

        if (IsKeyPressed(KEY_S)) {
            slowMotion = !slowMotion;
            if (slowMotion) fastForward = false;
        }

        if (IsKeyPressed(KEY_F)) {
            fastForward = !fastForward;
            if (fastForward) slowMotion = false;
        }

        stepRequested = IsKeyPressed(KEY_N);
        if (IsKeyPressed(KEY_T))
            SimulationSetTrafficLevel(&simulation,
                (TrafficLevel)((simulation.trafficLevel + 1) % TRAFFIC_LEVEL_COUNT));
        if (IsKeyPressed(KEY_W))
            SimulationSetCityCondition(&simulation,
                (CityCondition)((simulation.cityCondition + 1) % CITY_CONDITION_COUNT));

        if (IsKeyPressed(KEY_M)) SimulationSimulateQueue(&simulation);
        if (IsKeyPressed(KEY_C)) {
            SimulationRunStrategyComparison(&comparison);
            comparisonReady = true;
            selectedCall = -1;
            selectedIncident = -1;
            responseView = false;
            responseVehicleIndex = -1;
            responseIncidentIndex = -1;
        }
        if (IsKeyPressed(KEY_ESCAPE)) overlay = OVERLAY_NONE;

        if (IsKeyPressed(KEY_R)) {
            SimulationInit(&simulation);
            paused = false;
            fastForward = false;
            slowMotion = false;
            comparisonReady = false;
            selectedCall = -1;
            selectedIncident = -1;
            responseView = false;
            responseVehicleIndex = -1;
            responseIncidentIndex = -1;
            selectedVehicleMask = 0;
            overlay = OVERLAY_NONE;
            overrideModalCall = -1;
            overrideResumeWhenDone = false;
            overrideModalDeadline = 0.0;
            operatorNotice[0] = '\0';
            operatorNoticeMatched = 0;
            operatorNoticeUntil = 0.0;
        }
        }



        Rectangle cityView = FitCityToPanel(mapClip);
        int queueOrder[MAX_CALLS];
        int queueCount = BuildCallQueueOrder(&simulation, queueOrder);

        Vector2 mousePosition = GetMousePosition();
        Vector2 mouse = {0};
        bool mouseInCanvas = ScreenToVirtualPoint(mousePosition, viewport,
                                                  UI_WIDTH, UI_HEIGHT, &mouse);

        bool cameraPointerAllowed = responseView && overlay == OVERLAY_NONE &&
            overrideModalCall < 0 && !responseFirstPerson && mouseInCanvas &&
            StreetCameraPointerAllowed(mouse);
        if (!responseView || overlay != OVERLAY_NONE || overrideModalCall >= 0 ||
            responseFirstPerson || IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            responseCameraDragging = false;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && cameraPointerAllowed) {
            responseCameraDragging = true;
            responseCameraLastMouse = mouse;
        } else if (responseCameraDragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (cameraPointerAllowed) {
                ResponseCameraDrag(&responseCameraState,
                    mouse.x - responseCameraLastMouse.x,
                    mouse.y - responseCameraLastMouse.y);
                responseCameraLastMouse = mouse;
            } else {
                responseCameraDragging = false;
            }
        }
        float cameraWheel = GetMouseWheelMove();
        if (cameraWheel != 0.0f && cameraPointerAllowed)
            ResponseCameraZoom(&responseCameraState, cameraWheel);
        if (cameraWheel != 0.0f && overlay == OVERLAY_SETTINGS && mouseInCanvas &&
            CheckCollisionPointRec(mouse, SettingsHelpViewportBounds())) {
            settingsHelpScroll += cameraWheel < 0.0f ? 1 : -1;
            if (settingsHelpScroll < 0) settingsHelpScroll = 0;
            if (settingsHelpScroll > SETTINGS_HELP_LINE_COUNT - SETTINGS_HELP_VISIBLE_LINES)
                settingsHelpScroll = SETTINGS_HELP_LINE_COUNT - SETTINGS_HELP_VISIBLE_LINES;
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouseInCanvas) {

            if (modalWasActive || overrideModalCall >= 0) {
                if (overrideModalCall >= 0 && modalCallAtFrameStart == overrideModalCall) {
                    int choice = CheckCollisionPointRec(mouse, operatorFireButton) ? 0 :
                                 CheckCollisionPointRec(mouse, operatorPoliceButton) ? 1 :
                                 CheckCollisionPointRec(mouse, operatorAmbulanceButton) ? 2 : -1;
                    if (choice >= 0) {
                        IncidentType aiType;
                        int aiSeverity;
                        if (SimulationSuggestCallClassification(&simulation,
                                overrideModalCall, &aiType, &aiSeverity)) {
                            IncidentType acceptedType = OperatorServiceChoice(choice, aiType);
                            AcceptOperatorOverride(&simulation, overrideModalCall,
                                acceptedType, aiSeverity, &paused, overrideResumeWhenDone,
                                &overrideModalCall, &overrideModalDeadline);
                        }
                    }
                }
            } else if (overlay == OVERLAY_NONE &&
                       CheckCollisionPointRec(mouse, NextCallTypeSelectorBounds())) {
                selectedType = CycleNextCallType((IncidentType)selectedType);
            } else if (overlay == OVERLAY_NONE &&
                       CheckCollisionPointRec(mouse, responseViewButton)) {
                if (responseView) {
                    responseView = false;
                    responseCameraDragging = false;
                    responseVehicleIndex = -1;
                    responseIncidentIndex = -1;
                } else {
                    int responseIncident = SelectedResponseIncident(&simulation, selectedCall,
                                                                    selectedIncident);
                    int viewableVehicle = ResponseFirstViewableUnit(&simulation,
                                                                    responseIncident);
                    if (viewableVehicle >= 0) {
                        responseView = true;
                        responseVehicleIndex = viewableVehicle;
                        responseIncidentIndex = responseIncident;
                        responseUnitOrdinal = ResponseUnitOrdinal(&simulation,
                            responseIncident, viewableVehicle);
                        responseFirstPerson = false;
                        responseCameraDragging = false;
                        ResponseCameraReset(&responseCameraState);
                    }
                }
            } else if (overlay == OVERLAY_NONE && responseView &&
                       CheckCollisionPointRec(mouse, responsePreviousButton)) {
                int count = ResponseUnitCount(&simulation, responseIncidentIndex);
                if (count > 1) {
                    responseUnitOrdinal = (responseUnitOrdinal + count - 1) % count;
                    responseVehicleIndex = ResponseUnitAt(&simulation,
                        responseIncidentIndex, responseUnitOrdinal);
                    responseCameraDragging = false;
                    ResponseCameraReset(&responseCameraState);
                }
            } else if (overlay == OVERLAY_NONE && responseView &&
                       CheckCollisionPointRec(mouse, responseNextButton)) {
                int count = ResponseUnitCount(&simulation, responseIncidentIndex);
                if (count > 1) {
                    responseUnitOrdinal = (responseUnitOrdinal + 1) % count;
                    responseVehicleIndex = ResponseUnitAt(&simulation,
                        responseIncidentIndex, responseUnitOrdinal);
                    responseCameraDragging = false;
                    ResponseCameraReset(&responseCameraState);
                }
            } else if (overlay == OVERLAY_NONE && responseView &&
                       CheckCollisionPointRec(mouse, responseCameraButton)) {
                responseFirstPerson = !responseFirstPerson;
                responseCameraDragging = false;
            } else if (overlay == OVERLAY_NONE && responseView &&
                       CheckCollisionPointRec(mouse, responseCameraResetButton)) {
                responseFirstPerson = false;
                responseCameraDragging = false;
                ResponseCameraReset(&responseCameraState);
            } else if (overlay == OVERLAY_NONE && responseView &&
                       CheckCollisionPointRec(mouse, mapClip)) {
                int count = ResponseUnitCount(&simulation, responseIncidentIndex);
                for (int row = 0; row < count && row < MAX_VEHICLES; row++) {
                    if (!CheckCollisionPointRec(mouse, ResponseUnitRowBounds(row))) continue;
                    int chosen = ResponseUnitAt(&simulation, responseIncidentIndex, row);
                    if (chosen >= 0) {
                        if (chosen != responseVehicleIndex) {
                            responseCameraDragging = false;
                            ResponseCameraReset(&responseCameraState);
                        }
                        responseVehicleIndex = chosen;
                        responseUnitOrdinal = row;
                    }
                    break;
                }
            } else if (overlay != OVERLAY_NONE) {
                if ((overlay == OVERLAY_SETTINGS &&
                     CheckCollisionPointRec(mouse, settingsCloseButton)) ||
                    (overlay != OVERLAY_SETTINGS &&
                     CheckCollisionPointRec(mouse, modalCloseButton))) {
                    overlay = OVERLAY_NONE;
                } else if (overlay == OVERLAY_SETTINGS &&
                           CheckCollisionPointRec(mouse, routesToggleButton)) {
                    showRoutes = !showRoutes;
                } else if (overlay == OVERLAY_SETTINGS &&
                           CheckCollisionPointRec(mouse, civilianToggleButton)) {
                    showCivilianTraffic = !showCivilianTraffic;
                } else if (overlay == OVERLAY_SETTINGS &&
                           CheckCollisionPointRec(mouse, cityConditionButton)) {
                    SimulationSetCityCondition(&simulation,
                        (CityCondition)((simulation.cityCondition + 1) % CITY_CONDITION_COUNT));
                } else if (overlay == OVERLAY_SETTINGS &&
                           CheckCollisionPointRec(mouse, autoIncidentsButton)) {
                    SimulationSetAutoIncidents(&simulation,
                        !simulation.autoIncidentsEnabled, AUTO_INCIDENT_DEFAULT_SEED);
                }
            } else if (CheckCollisionPointRec(mouse, compareButton)) {

                SimulationRunStrategyComparison(&comparison);
                comparisonReady = true;
                selectedCall = -1;
                selectedIncident = -1;
                selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, dispatchModeButton)) {

                simulation.dispatchMode = simulation.dispatchMode == DISPATCH_AUTOMATIC ?
                                          DISPATCH_MANUAL : DISPATCH_AUTOMATIC;
                selectedVehicleMask = 0;
                if (simulation.dispatchMode == DISPATCH_AUTOMATIC) {
                    for (int i = 0; i < simulation.callCount; i++)
                        if (!simulation.calls[i].classified && !simulation.calls[i].dispatched)
                            simulation.calls[i].overridePending = 1;
                    StartPendingOverrideModal(&simulation, &paused, &overrideResumeWhenDone,
                                              &overrideModalCall, &overrideModalDeadline);
                } else {
                    for (int i = 0; i < simulation.callCount; i++)
                        simulation.calls[i].overridePending = 0;
                }

            } else if (simulation.dispatchMode == DISPATCH_MANUAL &&
                       ManualClassifyButtonHit(&simulation, selectedCall, mouse)) {

                if (SimulationClassifyCall(&simulation, selectedCall, manualCallType,
                                           manualCallSeverity))
                    selectedIncident = simulation.calls[selectedCall].incidentIndex;
                selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, dispatchSelectedButton) &&
                       simulation.dispatchMode == DISPATCH_MANUAL && selectedCall >= 0) {

                if (SimulationDispatchCallVehicles(&simulation, selectedCall, selectedVehicleMask))
                    selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, pauseButton)) {

                paused = !paused;

            } else if (CheckCollisionPointRec(mouse, normalButton)) {

                slowMotion = false;
                fastForward = false;

            } else if (CheckCollisionPointRec(mouse, slowButton)) {

                slowMotion = !slowMotion;
                if (slowMotion) fastForward = false;

            } else if (CheckCollisionPointRec(mouse, fastButton)) {

                fastForward = !fastForward;
                if (fastForward) slowMotion = false;

            } else if (CheckCollisionPointRec(mouse, stepButton)) {

                stepRequested = true;

            } else if (CheckCollisionPointRec(mouse, dispatchButton)) {

                int incidentIndex = selectedCall >= 0 ? CallIncidentIndex(&simulation, selectedCall) : -1;
                int required = incidentIndex >= 0 ?
                    CountRequiredUnits(&simulation.incidents[incidentIndex]) -
                    CountAssignedUnits(&simulation, incidentIndex) : 0;
                if (simulation.dispatchMode == DISPATCH_MANUAL && selectedCall >= 0 &&
                    !simulation.calls[selectedCall].dispatched && required > 0 &&
                    CountSelectedVehicles(selectedVehicleMask) == required &&
                    SimulationDispatchCallVehicles(&simulation, selectedCall, selectedVehicleMask))
                    selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, trafficButton)) {

                SimulationSetTrafficLevel(&simulation,
                    (TrafficLevel)((simulation.trafficLevel + 1) % TRAFFIC_LEVEL_COUNT));

            } else if (CheckCollisionPointRec(mouse, simulateButton)) {

                SimulationSimulateQueue(&simulation);

            } else if (CheckCollisionPointRec(mouse, reportsButton)) {

                overlay = OVERLAY_REPORTS;

            } else if (CheckCollisionPointRec(mouse, settingsButton)) {

                overlay = OVERLAY_SETTINGS;

            } else if (CheckCollisionPointRec(mouse, resetButton)) {

                SimulationInit(&simulation);

                paused = false;

                fastForward = false;
                slowMotion = false;

                comparisonReady = false;
                selectedCall = -1;
                selectedIncident = -1;
                responseView = false;
                responseVehicleIndex = -1;
                responseIncidentIndex = -1;
                selectedVehicleMask = 0;
                overlay = OVERLAY_NONE;
                overrideModalCall = -1;
                overrideResumeWhenDone = false;
                overrideModalDeadline = 0.0;
                operatorNotice[0] = '\0';
                operatorNoticeMatched = 0;
                operatorNoticeUntil = 0.0;

            } else if (CheckCollisionPointRec(mouse, queuePanel) &&
                       mouse.y >= queueRowsY && queueCount > 0) {
                int row = (int)((mouse.y - queueRowsY) / queueRowHeight);
                int visible = queueCount < QUEUE_VISIBLE ? queueCount : QUEUE_VISIBLE;
                if (row >= 0 && row < visible &&
                    CheckCollisionPointRec(mouse, QueueCardBounds(row))) {
                    int clickedCall = queueOrder[row];
                    selectedCall = clickedCall == selectedCall ? -1 : clickedCall;
                    selectedIncident = selectedCall >= 0 ?
                        CallIncidentIndex(&simulation, selectedCall) : -1;
                    if (selectedCall >= 0 && simulation.calls[selectedCall].classified) {
                        manualCallType = simulation.calls[selectedCall].classifiedType;
                        manualCallSeverity = simulation.calls[selectedCall].severity;
                    } else {
                        manualCallType = selectedType;
                        manualCallSeverity = 1;
                    }
                    selectedVehicleMask = 0;
                }
            } else if (simulation.dispatchMode == DISPATCH_MANUAL && selectedCall >= 0 &&
                       CheckCollisionPointRec(mouse, detailsPanel)) {
                EmergencyCall *call = &simulation.calls[selectedCall];
                int callIncidentIndex = CallIncidentIndex(&simulation, selectedCall);
                int callNeedsUnits = ManualCallNeedsVehicleAssignment(&simulation, selectedCall);
                if (!call->classified && !call->dispatched) {
                    const IncidentType types[] = {INCIDENT_FIRE, INCIDENT_CRIME,
                                                  INCIDENT_ACCIDENT, INCIDENT_MEDICAL};
                    for (int i = 0; i < 4; i++)
                        if (CheckCollisionPointRec(mouse, callTypeButtons[i]))
                            manualCallType = types[i];
                    for (int i = 0; i < 3; i++)
                        if (CheckCollisionPointRec(mouse, callPriorityButtons[i]))
                            manualCallSeverity = i + 1;
                } else if (call->classified && callNeedsUnits && selectedIncident >= 0 &&
                           selectedIncident == callIncidentIndex) {
                    Incident *incident = &simulation.incidents[callIncidentIndex];
                    int vehicleRows[MANUAL_VEHICLE_ROW_COUNT];
                    int rowCount = ManualVehicleRows(&simulation, incident, vehicleRows);
                    for (int row = 0; row < rowCount; row++) {
                        unsigned int updatedMask = ManualVehicleToggleSelection(
                            &simulation, incident, selectedIncident, row, mouse,
                            selectedVehicleMask);
                        if (updatedMask != selectedVehicleMask) {
                            selectedVehicleMask = updatedMask;
                            break;
                        }
                        // A click on an already-selected row can leave the mask
                        // unchanged only when the row was unavailable or outside
                        // its hitbox. Keep checking rows so later rows remain usable.
                    }
                }
            } else if (!responseView && CheckCollisionPointRec(mouse, cityView)) {
                Vector2 cityPoint = MapCanvasToCity(mouse, cityView);
                int clickedIncident = FindIncidentAtPosition(&simulation, cityPoint);
                if (clickedIncident >= 0) {
                    selectedIncident = clickedIncident;
                    int linkedCall = simulation.incidents[clickedIncident].callIndex;
                    selectedCall = linkedCall >= 0 && linkedCall < simulation.callCount ? linkedCall : -1;
                    selectedVehicleMask = 0;
                } else if ((selectedCall = FindCallAtPosition(&simulation, cityPoint)) >= 0) {
                    selectedIncident = CallIncidentIndex(&simulation, selectedCall);
                    manualCallType = selectedType;
                    manualCallSeverity = 1;
                    selectedVehicleMask = 0;
                } else if (cityPoint.x >= 0 && cityPoint.x < CITY_WIDTH &&
                           cityPoint.y >= 0 && cityPoint.y < CITY_HEIGHT) {
                    selectedCall = -1;
                    selectedIncident = -1;
                    selectedVehicleMask = 0;
                    if (SimulationCreateCall(&simulation, cityPoint,
                            CallerReportForType((IncidentType)selectedType, simulation.callCount))) {
                        selectedCall = simulation.callCount - 1;
                        IncidentType reportType = (IncidentType)selectedType;
                        int reportSeverity = 1;
                        SimulationSuggestCallClassification(&simulation, selectedCall,
                                                            &reportType, &reportSeverity);
                        if (reportType != (IncidentType)selectedType) reportSeverity = 1;
                        SimulationSetCallTruth(&simulation, selectedCall,
                                               (IncidentType)selectedType, reportSeverity);
                        manualCallType = (IncidentType)selectedType;
                        manualCallSeverity = 1;
                        if (simulation.dispatchMode == DISPATCH_AUTOMATIC) {
                            simulation.calls[selectedCall].overridePending = 1;
                            StartPendingOverrideModal(&simulation, &paused,
                                &overrideResumeWhenDone, &overrideModalCall,
                                &overrideModalDeadline);
                        } else {
                            SimulationProcessOperator(&simulation);
                        }
                    }
                }

            }

        }


        if (responseView && SelectedResponseIncident(&simulation, selectedCall,
                selectedIncident) != responseIncidentIndex) {
            responseView = false;
            responseVehicleIndex = -1;
            responseIncidentIndex = -1;
        } else if (responseView) {
            int ordinal = ResponseUnitOrdinal(&simulation, responseIncidentIndex,
                                                  responseVehicleIndex);
            if (ordinal >= 0) {
                responseUnitOrdinal = ordinal;
            } else {
                int count = ResponseUnitCount(&simulation, responseIncidentIndex);
                if (count > 0) {
                    responseUnitOrdinal = responseUnitOrdinal < count ? responseUnitOrdinal : count - 1;
                    responseVehicleIndex = ResponseUnitAt(&simulation,
                        responseIncidentIndex, responseUnitOrdinal);
                } else {
                    responseView = false;
                    responseVehicleIndex = -1;
                    responseIncidentIndex = -1;
                }
            }
        }

        if (overrideModalCall >= 0 || modalWasActive) {
            // Modal countdowns use real time; simulation state is frozen here.
        } else if (stepRequested) {
            // Manual stepping always advances by the same small time slice,
            // then pauses so the result can be inspected.
            paused = true;
            SimulationUpdate(&simulation, 0.1f);
        } else if (!paused) {
            float speedMultiplier = fastForward ? 8.0f : slowMotion ? 0.25f : 1.0f;
            SimulationUpdate(&simulation, GetFrameTime() * speedMultiplier);
        }
        // A cascade call can be created inside SimulationUpdate. Start its
        // AUTO OP override modal immediately after that update, before drawing.
        StartPendingOverrideModal(&simulation, &paused, &overrideResumeWhenDone,
                                  &overrideModalCall, &overrideModalDeadline);
        UpdateCallerFeedbackToast(&simulation, operatorNotice, sizeof(operatorNotice),
                                  &operatorNoticeMatched, &operatorNoticeUntil);

        // Input can create or reset calls after the hit-test queue was built.
        // Rebuild it before drawing so the same frame never renders stale rows.
        queueCount = BuildCallQueueOrder(&simulation, queueOrder);

        if (responseView && responseCanvas.id != 0 &&
            responseVehicleIndex >= 0 && responseVehicleIndex < simulation.vehicleCount)
            RenderResponseStreetView(responseCanvas, &simulation, responseVehicleIndex,
                                     responseFirstPerson, &responseCameraState);



        BeginTextureMode(uiCanvas);
        ClearBackground((Color){7, 17, 29, 255});
        BeginMode2D(renderCamera);

        DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){7, 17, 29, 255});
        DrawRectangle(0, 0, UI_WIDTH, 80, (Color){9, 25, 42, 255});
        DrawLine(0, 79, UI_WIDTH, 79, (Color){34, 74, 101, 255});
        DrawCityMark();
        DrawText("EMERGENCY CITY SIMULATION", 82, 13, 23, RAYWHITE);
        DrawText("CONTROL ROOM  /  LIVE DISPATCH", 84, 49, 11,
                 (Color){124, 190, 222, 255});
        DrawNextCallTypeSelector((IncidentType)selectedType);
        int waitingCalls = 0;
        for (int call = 0; call < simulation.callCount; call++)
            if (!simulation.calls[call].dispatched && !CallIsTerminal(&simulation, call)) waitingCalls++;
        DrawText(TextFormat("CALLS %d  |  WAIT %d  |  ACTIVE %d",
                 simulation.callCount, waitingCalls, SimulationCountUnresolved(&simulation)),
                 500, 17, 10, (Color){177, 198, 211, 255});
        int onTime, arrivals;
        OnTimeArrivalCounts(&simulation, &onTime, &arrivals);
        DrawText(TextFormat("HAZARDS %d  |  RES %d  |  FAIL %d  |  ON-TIME %s",
                 CountActiveHazards(&simulation),
                 SimulationCountStatus(&simulation, INCIDENT_RESOLVED),
                 SimulationCountStatus(&simulation, INCIDENT_FAILED),
                 arrivals > 0 ? TextFormat("%d/%d", onTime, arrivals) : "--"), 500, 38, 8,
                 (Color){124, 190, 222, 255});
        DrawText(TextFormat("TIME %05.1fs  |  CARS %d  |  %s  |  TRAFFIC %s",
                 simulation.elapsedTime, simulation.civilianCount,
                 SimulationCityConditionName(simulation.cityCondition),
                 SimulationTrafficLevelName(simulation.trafficLevel)),
                 500, 55, 8, (Color){179, 198, 211, 255});
        DrawHeaderService(&simulation, INCIDENT_FIRE, 790, "FIRE", IncidentColor(INCIDENT_FIRE));
        DrawHeaderService(&simulation, INCIDENT_CRIME, 986, "POLICE", IncidentColor(INCIDENT_CRIME));
        DrawHeaderService(&simulation, INCIDENT_ACCIDENT, 1182, "AMBULANCE", IncidentColor(INCIDENT_MEDICAL));
        DrawRectangleRounded((Rectangle){1384, 14, 196, 52}, 0.12f, 7,
                             (Color){12, 37, 57, 255});
        DrawRectangleRoundedLines((Rectangle){1384, 14, 196, 52}, 0.12f, 7,
                                  (Color){34, 74, 101, 255});
        DrawText("DISPATCH MODE", 1398, 21, 8, (Color){151, 178, 194, 255});
        DrawText(simulation.dispatchMode == DISPATCH_AUTOMATIC ? "AUTO OPERATOR" : "MANUAL OPERATOR",
                 1398, 39, 10, simulation.dispatchMode == DISPATCH_AUTOMATIC ? GREEN : SKYBLUE);
        if (simulation.autoIncidentsEnabled) {
            const char *autoStatus = simulation.autoIncidentCapacitySuspended ?
                "AUTO INCIDENTS PAUSED - CAPACITY FULL" :
                TextFormat("AUTO INCIDENTS ON  |  NEXT %.1fs",
                           SimulationAutoIncidentSecondsUntilNext(&simulation));
            DrawText(autoStatus, 500, 68, 7,
                     simulation.autoIncidentCapacitySuspended ? ORANGE : SKYBLUE);
        }

        DrawIncidentQueue(&simulation, queueOrder, queueCount, selectedCall);
        DrawPanel(mapPanel);
        const Vehicle *trackedResponse = responseView && responseVehicleIndex >= 0 &&
            responseVehicleIndex < simulation.vehicleCount ?
            &simulation.vehicles[responseVehicleIndex] : NULL;
        DrawText(responseView ? trackedResponse && trackedResponse->arrived ?
                 "ON-SCENE RESPONSE" : "LIVE 3D RESPONSE" : "CITY MAP",
                 408, 98, 16, RAYWHITE);
        DrawText(responseView ? (responseFirstPerson ? "First-person vehicle view" :
                 "Drag in the scene to look around; scroll to zoom") :
                 "Click map to log or inspect a call  |  1-4 report type", 408, 116, 8,
                 (Color){139, 164, 179, 255});
        if (!responseView) DrawMapLegend();
        if (responseView) {
            DrawButton(responseViewButton, "BACK TO MAP", BLUE);
        } else {
            int responseIncident = SelectedResponseIncident(&simulation, selectedCall,
                                                            selectedIncident);
            int responseVehicle = ResponseFirstViewableUnit(&simulation,
                                                            responseIncident);
            const char *responseLabel = responseCanvas.id == 0 ? "3D VIEW UNAVAILABLE" :
                                        responseIncident < 0 ? "SELECT CALL" :
                                        responseVehicle >= 0 ? "VIEW RESPONSE" :
                                                               "NO VIEWABLE UNIT";
            DrawButton(responseViewButton, responseLabel,
                       responseVehicle >= 0 && responseCanvas.id != 0 ? BLUE : DARKGRAY);
        }
        DrawRectangleRounded(mapClip, 0.02f, 8, (Color){14, 27, 37, 255});

        // The map uses its own camera and draws directly to the 2x render
        // texture. End the UI camera before that pass, then restore it for the
        // details panel and controls so they retain the correct size/position.
        EndMode2D();
        if (responseView && responseCanvas.id != 0)
            DrawResponseStreetViewTexture(responseCanvas);
        else
            DrawMapContents(&simulation, selectedIncident, selectedCall, cityView,
                            showRoutes, showCivilianTraffic);

        BeginMode2D(renderCamera);
        if (responseView && responseCanvas.id != 0)
            DrawResponseStreetHud(&simulation, responseVehicleIndex,
                responseUnitOrdinal, ResponseUnitCount(&simulation, responseIncidentIndex),
                responseFirstPerson);
        DrawUnitRoster(&simulation);
        DrawIncidentDetails(&simulation, selectedCall, selectedIncident,
                            comparisonReady, &comparison,
                            selectedVehicleMask, manualCallType, manualCallSeverity);
        DrawEventFeed(&simulation);

        DrawPanel((Rectangle){18, 842, 1564, 48});
        DrawButton(compareButton, "COMPARE", BLUE);
        DrawButton(pauseButton, paused ? "RESUME" : "PAUSE", paused ? DARKGREEN : DARKGRAY);
        DrawButton(slowButton, slowMotion ? "SLOW ON" : "SLOW",
                   slowMotion ? DARKBLUE : DARKGRAY);
        DrawButton(normalButton, (!slowMotion && !fastForward) ? "1x ON" : "1x",
                   (!slowMotion && !fastForward) ? DARKGREEN : DARKGRAY);
        DrawButton(fastButton, fastForward ? "FAST ON" : "FAST x8",
                   fastForward ? DARKGREEN : DARKGRAY);
        DrawButton(stepButton, "STEP 0.1", (Color){78, 91, 110, 255});
        int dispatchReady = 0;
        if (simulation.dispatchMode == DISPATCH_MANUAL &&
            ManualCallNeedsVehicleAssignment(&simulation, selectedCall)) {
            int incidentIndex = CallIncidentIndex(&simulation, selectedCall);
            if (incidentIndex >= 0 && incidentIndex < simulation.incidentCount) {
                int remaining = CountRequiredUnits(&simulation.incidents[incidentIndex]) -
                                CountAssignedUnits(&simulation, incidentIndex);
                dispatchReady = remaining > 0 && CountSelectedVehicles(selectedVehicleMask) == remaining;
            }
        }
        DrawButton(dispatchButton, "DISPATCH", dispatchReady ? BLUE : DARKGRAY);
        DrawButton(trafficButton,
                   TextFormat("TRAFFIC %s", SimulationTrafficLevelName(simulation.trafficLevel)),
                   simulation.trafficLevel == TRAFFIC_HEAVY ? ORANGE :
                   simulation.trafficLevel == TRAFFIC_MEDIUM ? (Color){176, 122, 48, 255} :
                   DARKGRAY);
        DrawButton(dispatchModeButton,
                   simulation.dispatchMode == DISPATCH_AUTOMATIC ? "AUTO OP" : "MANUAL OP",
                   simulation.dispatchMode == DISPATCH_AUTOMATIC ? (Color){34, 111, 82, 255} : BLUE);
        DrawButton(simulateButton, "SIM QUEUE", BLUE);
        DrawButton(reportsButton, "REPORTS", (Color){38, 75, 103, 255});
        DrawButton(settingsButton, "SETTINGS", (Color){38, 75, 103, 255});
        DrawButton(resetButton, "RESET", MAROON);

        if (overlay == OVERLAY_REPORTS) DrawReportsOverlay(&simulation);
        else if (overlay == OVERLAY_SETTINGS)
            DrawSettingsOverlay(showRoutes, showCivilianTraffic, &simulation,
                                settingsHelpScroll);

        if (overrideModalCall >= 0)
            DrawOperatorOverrideModal(&simulation, overrideModalCall, overrideModalDeadline);
        if (operatorNotice[0] && GetTime() < operatorNoticeUntil) {
            Color noticeColor = operatorNoticeMatched ? (Color){24, 119, 83, 248} :
                                (Color){145, 49, 57, 248};
            DrawRectangleRounded((Rectangle){425, 96, 750, 38}, 0.18f, 8, noticeColor);
            int noticeWidth = MeasureUiText(operatorNotice, 12);
            DrawText(operatorNotice, 800 - noticeWidth / 2, 109, 12, RAYWHITE);
        }

        EndMode2D();
        EndTextureMode();

        BeginDrawing();
        ClearBackground((Color){16, 25, 37, 255});
        DrawTexturePro(uiCanvas.texture,
                       (Rectangle){0, 0, (float)uiCanvas.texture.width,
                                   -(float)uiCanvas.texture.height},
                       viewport, (Vector2){0, 0}, 0.0f, WHITE);
        EndDrawing();

    }



    UnloadRenderTexture(uiCanvas);
    if (responseCanvas.id != 0) UnloadRenderTexture(responseCanvas);
    UnloadUiFont();
    CloseWindow();

    return 0;

}
