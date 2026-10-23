#include "raylib.h"

#include "incidents.h"

#include "simulation.h"
#include "ui_layout.h"
#include "manual_selection.h"

#include <stdio.h>
#include <math.h>
#include <string.h>



#define SCREEN_WIDTH 1440
#define SCREEN_HEIGHT 810
#define UI_WIDTH 1600
#define UI_HEIGHT 900
#define UI_RENDER_SCALE 2
#define CITY_WIDTH 1100
#define CITY_HEIGHT 700
#define QUEUE_VISIBLE 8

typedef enum {
    OVERLAY_NONE = 0,
    OVERLAY_REPORTS,
    OVERLAY_SETTINGS
} UIOverlay;


static const Rectangle queuePanel = {18, 104, 270, 700};
static const Rectangle mapPanel = {300, 104, 930, 700};
static const Rectangle detailsPanel = {1242, 104, 340, 700};
static const Rectangle mapClip = {310, 150, 910, 642};
static const float queueRowsY = 158.0f;
static const float queueRowHeight = 77.0f;

static const Rectangle compareButton = {332, 830, 76, 44};
static const Rectangle pauseButton = {416, 830, 72, 44};
static const Rectangle slowButton = {496, 830, 72, 44};
static const Rectangle normalButton = {576, 830, 64, 44};
static const Rectangle fastButton = {648, 830, 72, 44};
static const Rectangle stepButton = {728, 830, 70, 44};
static const Rectangle dispatchButton = {806, 830, 84, 44};
static const Rectangle trafficButton = {898, 830, 96, 44};
static const Rectangle dispatchModeButton = {1002, 830, 92, 44};
static const Rectangle simulateButton = {1102, 830, 104, 44};
static const Rectangle reportsButton = {1214, 830, 86, 44};
static const Rectangle settingsButton = {1308, 830, 86, 44};
static const Rectangle resetButton = {1482, 830, 88, 44};
static const Rectangle dispatchSelectedButton = {1260, 474, 306, 28};
static const Rectangle callTypeButtons[4] = {
    {1260, 244, 148, 23}, {1418, 244, 148, 23},
    {1260, 271, 148, 23}, {1418, 271, 148, 23}
};
static const Rectangle callPriorityButtons[3] = {
    {1260, 300, 64, 22}, {1330, 300, 64, 22}, {1400, 300, 64, 22}
};
static const Rectangle modalPanel = {470, 210, 660, 490};
static const Rectangle modalCloseButton = {1000, 638, 100, 38};
static const Rectangle routesToggleButton = {550, 340, 500, 48};
static const Rectangle civilianToggleButton = {550, 405, 500, 48};
static const Rectangle cityConditionButton = {550, 470, 500, 48};
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
static int DrawWrappedText(const char *text, int x, int y, int fontSize,
                           int maxCharacters, int lineHeight, Color color);
static const char *PriorityLabel(int severity);
static void DrawButton(Rectangle rect, const char *label, Color color);
static void DrawPanel(Rectangle rect);

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
        DrawRectangle(0, 0, CITY_WIDTH, CITY_HEIGHT, (Color){196, 211, 218, 34});
        for (int band = 0; band < 4; band++) {
            float y = 90.0f + band * 157.0f + sinf(simulation->elapsedTime * 0.35f + band) * 12.0f;
            DrawRectangle(0, (int)y, CITY_WIDTH, 48, (Color){201, 216, 221, 16});
        }
    } else if (simulation->cityCondition == CITY_RAIN) {
        for (int drop = 0; drop < 82; drop++) {
            float x = (float)((drop * 97 + 29) % CITY_WIDTH);
            float y = fmodf((float)((drop * 53) % CITY_HEIGHT) + simulation->elapsedTime * 92.0f,
                            (float)CITY_HEIGHT);
            DrawLineEx((Vector2){x, y}, (Vector2){x - 5.0f, y + 10.0f}, 1.1f,
                       (Color){127, 190, 225, 105});
        }
    } else if (simulation->cityCondition == CITY_WIND) {
        for (int streak = 0; streak < 8; streak++) {
            int y = 78 + streak * 67;
            int x = (int)fmodf((float)(streak * 127) + simulation->elapsedTime * 30.0f,
                               (float)CITY_WIDTH);
            DrawLineEx((Vector2){(float)x, (float)y}, (Vector2){(float)x + 22.0f, (float)y - 2.0f},
                       1.0f, (Color){141, 194, 220, 75});
        }
    }
}

static void DrawEventFeed(const Simulation *simulation)
{
    // Reserve a compact strip at the bottom of the right panel. The feed used
    // to be drawn over service and city-operation summaries.
    Rectangle feedPanel = {1254, 714, 316, 90};
    DrawRectangleRounded(feedPanel, 0.035f, 8, (Color){11, 29, 43, 245});
    DrawRectangleRoundedLinesEx(feedPanel, 0.035f, 8, 1, (Color){35, 69, 89, 255});
    DrawText("LIVE EVENT FEED", 1266, 719, 9, (Color){107, 190, 238, 255});
    DrawText(TextFormat("%d / %d", simulation->eventCount, MAX_SIMULATION_EVENTS),
             1518, 721, 7, (Color){135, 161, 178, 255});
    DrawLine(1266, 734, 1558, 734, (Color){43, 67, 83, 255});
    if (simulation->eventCount == 0) {
        DrawText("Waiting for simulation events", 1266, 746, 8,
                 (Color){145, 166, 180, 255});
        return;
    }
    int shown = simulation->eventCount < 3 ? simulation->eventCount : 3;
    for (int row = 0; row < shown; row++) {
        int eventIndex = simulation->eventCount - 1 - row;
        const SimulationEvent *event = &simulation->events[eventIndex];
        int y = 739 + row * 20;
        DrawCircle(1270, y + 5, 2, row == 0 ? SKYBLUE : (Color){86, 126, 148, 255});
        DrawText(TextFormat("%.0fs", event->time), 1277, y, 7,
                 (Color){128, 163, 183, 255});
        DrawText(TextFormat("%.47s", event->message), 1304, y, 7,
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

    // Subtle urban blocks and service alleys; the driveable emergency route
    // network remains the existing three connected arterial roads below.
    for (int x = 20; x < CITY_WIDTH; x += 64)
        DrawLine(x, 0, x, CITY_HEIGHT, (Color){55, 73, 78, 35});
    for (int y = 20; y < CITY_HEIGHT; y += 64)
        DrawLine(0, y, CITY_WIDTH, y, (Color){55, 73, 78, 35});

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

    const Rectangle buildings[] = {
        {48,145,92,82}, {158,112,145,66}, {169,190,110,49},
        {462,108,92,54}, {565,105,119,52}, {464,174,35,72}, {640,176,47,64},
        {839,113,88,100}, {943,116,99,83}, {843,207,88,37},
        {48,410,100,110}, {163,389,122,82}, {164,484,120,81},
        {463,390,111,76}, {586,389,101,83}, {467,482,92,86}, {570,489,116,73},
        {839,394,91,63}, {944,393,99,101}, {840,483,87,83}, {948,516,95,69}
    };
    const int buildingCount = (int)(sizeof(buildings) / sizeof(buildings[0]));
    for (int i = 0; i < buildingCount; i++) {
        DrawCityBuilding(buildings[i], i);
        if (simulation->apartmentFireDrill && i == 4) {
            const Incident *fire = simulation->incidentCount ? &simulation->incidents[0] : NULL;
            DrawText("APARTMENT BLOCK", (int)buildings[i].x + 7,
                     (int)buildings[i].y + 5, 6, (Color){139, 196, 219, 255});
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
        if (simulation->apartmentFireDrill && i == 7 && simulation->incidentCount > 0 &&
            simulation->incidents[0].fireSpreadOccurred) {
            DrawRectangleLinesEx(buildings[i], 2.0f,
                simulation->incidents[0].fireContained ? GREEN : ORANGE);
            DrawText(simulation->incidents[0].fireContained ? "FIRE CONTAINED" : "THREATENED",
                     (int)buildings[i].x + 8, (int)buildings[i].y + buildings[i].height - 14,
                     7, simulation->incidents[0].fireContained ? GREEN : ORANGE);
        }
    }

    // Minor access roads and sidewalks sit beneath the connected main roads.
    DrawRectangle(0, 274, CITY_WIDTH, 16, (Color){87, 102, 105, 255});
    DrawRectangle(0, 362, CITY_WIDTH, 16, (Color){87, 102, 105, 255});
    DrawRectangle(339, 80, 16, 550, (Color){87, 102, 105, 255});
    DrawRectangle(437, 80, 16, 550, (Color){87, 102, 105, 255});
    DrawRectangle(714, 80, 16, 550, (Color){87, 102, 105, 255});
    DrawRectangle(812, 80, 16, 550, (Color){87, 102, 105, 255});

    // Main arterial geometry matches the road nodes and routing graph exactly.
    DrawRectangle(0, 290, CITY_WIDTH, 72, (Color){68, 82, 91, 255});
    DrawRectangle(355, 80, 82, 550, (Color){68, 82, 91, 255});
    DrawRectangle(730, 80, 82, 550, (Color){68, 82, 91, 255});
    DrawLine(0, 294, CITY_WIDTH, 294, (Color){139, 151, 153, 210});
    DrawLine(0, 358, CITY_WIDTH, 358, (Color){139, 151, 153, 210});
    DrawLine(359, 80, 359, 630, (Color){139, 151, 153, 210});
    DrawLine(433, 80, 433, 630, (Color){139, 151, 153, 210});
    DrawLine(734, 80, 734, 630, (Color){139, 151, 153, 210});
    DrawLine(808, 80, 808, 630, (Color){139, 151, 153, 210});

    // Painted center lines distinguish opposing traffic directions.
    for (int x = 0; x < CITY_WIDTH; x += 40) {
        if ((x >= 320 && x <= 440) || (x >= 695 && x <= 815)) continue;
        DrawRectangle(x, 324, 23, 3, (Color){222, 190, 111, 235});
        DrawRectangle(x, 329, 23, 2, (Color){222, 190, 111, 150});
    }
    for (int y = 80; y < 630; y += 40) {
        if (y >= 280 && y <= 360) continue;
        DrawRectangle(394, y, 3, 23, (Color){222, 190, 111, 235});
        DrawRectangle(399, y, 2, 23, (Color){222, 190, 111, 150});
        DrawRectangle(769, y, 3, 23, (Color){222, 190, 111, 235});
        DrawRectangle(774, y, 2, 23, (Color){222, 190, 111, 150});
    }

    // Zebra crossings and stop bars clarify the two connected junctions.
    DrawCrosswalkHorizontal(335, 293); DrawCrosswalkHorizontal(437, 293);
    DrawCrosswalkHorizontal(710, 293); DrawCrosswalkHorizontal(812, 293);
    DrawCrosswalkVertical(363, 262); DrawCrosswalkVertical(363, 363);
    DrawCrosswalkVertical(738, 262); DrawCrosswalkVertical(738, 363);

    // District labels and road names provide orientation without hiding units.
    DrawText("RIVERSIDE", 55, 245, 11, (Color){191, 207, 203, 235});
    DrawText("NORTH MARKET", 493, 245, 11, (Color){191, 207, 203, 235});
    DrawText("EAST WARD", 936, 245, 11, (Color){191, 207, 203, 235});
    DrawText("WEST END", 55, 645, 11, (Color){191, 207, 203, 235});
    DrawText("CENTRAL", 493, 645, 11, (Color){191, 207, 203, 235});
    DrawText("PARK DISTRICT", 845, 645, 11, (Color){191, 207, 203, 235});
    DrawServiceLandmark((Rectangle){240, 242, 54, 32}, "FIRE HQ", RED);
    DrawServiceLandmark((Rectangle){590, 590, 62, 32}, "POLICE", BLUE);
    DrawServiceLandmark((Rectangle){860, 490, 80, 58}, "HOSPITAL", (Color){91, 225, 182, 255});
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
    DrawRectangleRounded((Rectangle){(float)x, (float)y, 316, 32}, 0.08f, 8,
                        (Color){20, 42, 60, 255});
    DrawIncidentIcon(iconType, (Vector2){(float)x + 20, (float)y + 16}, 7, color);
    DrawText(ServiceName(service), x + 36, y + 2, 10, RAYWHITE);
    DrawText(TextFormat("Available %d / %d  |  Active %d",
             SimulationCountAvailableVehicles(simulation, service),
             CountFleetForService(simulation, service),
             CountActiveForService(simulation, service)), x + 36, y + 14, 7,
             (Color){179, 198, 211, 255});
    float average = AverageResponseForService(simulation, service);
    DrawText(average < 0.0f ? TextFormat("Resolved %d  |  Avg response --",
             simulation->resolvedByType[service]) :
             TextFormat("Resolved %d  |  Avg response %.1fs",
             simulation->resolvedByType[service], average), x + 36, y + 23, 7,
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

    DrawLine(1260, 663, 1565, 663, (Color){43, 67, 83, 255});
    DrawText("CITY OPERATIONS", 1260, 667, 9, (Color){107, 190, 238, 255});
    DrawText(TextFormat("Fleet %d/%d ready  |  Waiting for units %d",
             available, simulation->vehicleCount, waitingForUnits),
             1260, 682, 7, (Color){177, 195, 207, 255});
    DrawText(recorded > 0 ? TextFormat("On-time first arrivals %d / %d", onTime, recorded) :
             "On-time first arrivals -- (no arrivals yet)", 1260, 695, 7,
             (Color){177, 195, 207, 255});
}

static void DrawHeaderService(const Simulation *simulation, IncidentType service,
                              int x, const char *label, Color color)
{
    DrawRectangleRounded((Rectangle){(float)x, 12, 156, 58}, 0.12f, 7,
                         (Color){12, 37, 57, 255});
    DrawRectangleRoundedLines((Rectangle){(float)x, 12, 156, 58}, 0.12f, 7,
                              (Color){34, 74, 101, 255});
    DrawIncidentIcon(service == INCIDENT_ACCIDENT ? INCIDENT_MEDICAL : service,
                     (Vector2){(float)x + 20, 40}, 11, color);
    DrawText(label, x + 39, 19, 10, (Color){179, 198, 211, 255});
    DrawText(TextFormat("%d active  |  %d/%d ready", CountActiveForService(simulation, service),
             SimulationCountAvailableVehicles(simulation, service),
             CountFleetForService(simulation, service)), x + 39, 40, 9, RAYWHITE);
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

static void DrawIncidentQueue(const Simulation *simulation, const int order[MAX_CALLS],
                              int orderCount, int selectedCall)
{
    DrawPanel(queuePanel);
    int waiting = 0;
    for (int i = 0; i < simulation->callCount; i++)
        if (!simulation->calls[i].dispatched && !CallIsTerminal(simulation, i)) waiting++;
    DrawText("CALL QUEUE", 34, 120, 17, RAYWHITE);
    DrawText(TextFormat("%d waiting  |  %d total", waiting, simulation->callCount), 34, 140, 10,
             (Color){134, 160, 177, 255});
    DrawText(simulation->dispatchMode == DISPATCH_AUTOMATIC ? "AUTO" : "MANUAL",
             225, 140, 9, simulation->dispatchMode == DISPATCH_AUTOMATIC ? GREEN : SKYBLUE);

    int visible = orderCount < QUEUE_VISIBLE ? orderCount : QUEUE_VISIBLE;
    if (visible == 0) {
        DrawText("No calls received yet", 42, 180, 13, (Color){160, 181, 195, 255});
        DrawText("Click the map to log a call.", 42, 202, 11, (Color){116, 142, 159, 255});
    } else {
        for (int row = 0; row < visible; row++) {
            int callIndex = order[row];
            const EmergencyCall *call = &simulation->calls[callIndex];
            int incidentIndex = CallIncidentIndex(simulation, callIndex);
            const Incident *incident = incidentIndex >= 0 &&
                incidentIndex < simulation->incidentCount ? &simulation->incidents[incidentIndex] : NULL;
            float y = queueRowsY + row * queueRowHeight;
            Rectangle card = {26, y, 254, 74};
            Color typeColor = incident ? IncidentColor(incident->type) : (Color){232, 166, 70, 255};
            Color urgencyColor = incident && incident->severity >= 3 ? (Color){255, 112, 91, 255} :
                                 incident && incident->severity == 2 ? (Color){255, 189, 85, 255} :
                                 (Color){130, 181, 209, 255};
            DrawRectangleRounded(card, 0.08f, 8,
                callIndex == selectedCall ? (Color){30, 57, 78, 255} : (Color){18, 38, 54, 255});
            DrawRectangleRounded((Rectangle){card.x, card.y, 4, card.height}, 0.3f, 4, typeColor);
            if (callIndex == selectedCall)
                DrawRectangleRoundedLines(card, 0.08f, 8, (Color){71, 151, 205, 255});

            if (incident) DrawIncidentIcon(incident->type, (Vector2){48, y + 25}, 9, typeColor);
            else {
                DrawCircle(48, (int)(y + 25), 10, (Color){93, 64, 26, 255});
                DrawText("?", 44, (int)y + 17, 16, (Color){255, 205, 110, 255});
            }
            DrawText(TextFormat("CALL %02d%s%s", callIndex + 1,
                     incident ? "  |  " : "",
                     incident ? IncidentTypeName(incident->type) : ""),
                     67, (int)y + 6, 11, RAYWHITE);
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
            DrawText(progress, 67, (int)y + 22, 8, progressColor);
            DrawWrappedText(call->report, 67, (int)y + 34, 8, 38, 8,
                            (Color){190, 205, 214, 255});
            int assigned = incident ? CountAssignedUnits(simulation, incidentIndex) : 0;
            int required = incident ? CountRequiredUnits(incident) : 0;
            DrawText(TextFormat("RECEIVED T+%.0fs   UNITS %d/%d",
                     call->receivedAt, assigned, required), 67, (int)y + 62, 7, urgencyColor);
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
    Rectangle card = {1258, 152, 308, 54};
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

    DrawText(TextFormat("CALLER  #%02d", callNumber), 1268, 157, 8, callerColor);
    DrawText(mode == DISPATCH_AUTOMATIC ? "AUTO OPERATOR" : "MANUAL OPERATOR",
             1350, 157, 8, operatorColor);
    DrawText("DISPATCH", 1493, 157, 8, dispatchColor);
    DrawLineEx((Vector2){1290, 176}, (Vector2){1387, 176}, 2,
               call->classified ? SKYBLUE : (Color){87, 107, 120, 255});
    DrawLineEx((Vector2){1413, 176}, (Vector2){1510, 176}, 2,
               call->dispatched ? GREEN : (Color){87, 107, 120, 255});
    DrawCircleV((Vector2){1280, 176}, 9, callerColor);
    DrawText("C", 1277, 170, 12, (Color){8, 27, 40, 255});
    DrawOperatorPortrait((Vector2){1400, 176}, operatorColor);
    DrawCircleV((Vector2){1523, 176}, 9, dispatchColor);
    DrawText("D", 1520, 170, 12, (Color){8, 27, 40, 255});
    DrawText("REPORT RECEIVED", 1260, 190, 7, (Color){169, 188, 200, 255});
    DrawText(operatorState, 1350, 190, 7, operatorColor);
    DrawText(dispatchState, 1493, 190, 7, dispatchColor);
}

static void DrawIncidentDetails(const Simulation *simulation, int selectedCall,
                                int comparisonReady, const StrategyComparison *comparison,
                                unsigned int selectedVehicleMask,
                                IncidentType manualType, int manualSeverity)
{
    DrawPanel(detailsPanel);
    DrawText("CALL HANDLING", 1260, 120, 17, RAYWHITE);
    DrawLine(1260, 146, 1565, 146, (Color){43, 67, 83, 255});

    if (selectedCall >= 0 && selectedCall < simulation->callCount) {
        const EmergencyCall *call = &simulation->calls[selectedCall];
        int incidentIndex = CallIncidentIndex(simulation, selectedCall);
        const Incident *incident = incidentIndex >= 0 && incidentIndex < simulation->incidentCount ?
                                   &simulation->incidents[incidentIndex] : NULL;
        int displaySeverity = call->classified ? call->severity : manualSeverity;
        DrawOperatorHandoff(call, selectedCall + 1, incident, simulation->dispatchMode);
        DrawRectangleRounded((Rectangle){1452, 117, 112, 25}, 0.22f, 6,
            displaySeverity >= 3 ? (Color){185, 45, 55, 255} :
            displaySeverity == 2 ? (Color){184, 119, 25, 255} :
                                   (Color){38, 130, 89, 255});
        DrawText(TextFormat("%s PRIORITY", PriorityLabel(displaySeverity)), 1462, 124, 9, RAYWHITE);

        DrawWrappedText(call->report, 1260, 210, 9, 55, 12,
                        (Color){207, 219, 225, 255});
        DrawText(TextFormat("LOCATION  %.0f, %.0f   |   RECEIVED T+%.1fs",
                 call->location.x, call->location.y, call->receivedAt),
                 1260, 235, 9, (Color){155, 185, 202, 255});

        if (simulation->dispatchMode == DISPATCH_MANUAL &&
            !call->dispatched && !call->classified) {
            DrawText("CLASSIFY CALL  /  INCIDENT TYPE", 1260, 253, 10,
                     (Color){107, 190, 238, 255});
            const IncidentType types[] = {INCIDENT_FIRE, INCIDENT_CRIME,
                                          INCIDENT_ACCIDENT, INCIDENT_MEDICAL};
            const char *labels[] = {"FIRE", "POLICE", "ACCIDENT", "MEDICAL"};
            for (int i = 0; i < 4; i++) {
                Color buttonColor = manualType == types[i] ? IncidentColor(types[i]) :
                                    (Color){25, 47, 63, 255};
                DrawButton(callTypeButtons[i], labels[i], buttonColor);
            }
            DrawText("PRIORITY", 1260, 302, 9, (Color){155, 185, 202, 255});
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
                     1460, 304, 9, RAYWHITE);
            DrawButton(ManualClassifyButtonBounds(),
                       call->classified ? "UPDATE CLASSIFICATION" : "CLASSIFY CALL",
                       BLUE);
            DrawText(TextFormat("RECEIVED T+%.1fs  |  CLASSIFICATION PENDING",
                     call->receivedAt), 1260, 362, 8, (Color){155, 185, 202, 255});
        } else if (incident) {
            float remaining = incident->deadline - incident->ageTime;
            if (remaining < 0.0f) remaining = 0.0f;
            float eta = SimulationEstimateIncidentETA(simulation, incidentIndex);
            DrawText(TextFormat("CLASSIFIED  %s  |  SEVERITY %d  |  PRIORITY %.0f",
                     IncidentTypeName(incident->type), incident->severity,
                     SimulationIncidentPriority(incident)), 1260, 255, 9, RAYWHITE);
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
            DrawText(requirements, 1260, 272, 8, (Color){177, 195, 207, 255});
            DrawText(TextFormat("TIME LEFT %.1fs   |   ETA %s",
                     remaining, eta < 0.0f ? "--" : TextFormat("%.1fs", eta)),
                     1260, 289, 9, remaining < 6.0f ? ORANGE :
                     (Color){177, 195, 207, 255});
            DrawText(TextFormat("RESPONSE STAGE  %s", IncidentStatusName(incident->status)),
                     1260, 302, 8, (Color){155, 185, 202, 255});

            if (simulation->dispatchMode == DISPATCH_MANUAL && !call->dispatched) {
                DrawText("SELECT SUITABLE UNITS  (all required before dispatch)",
                         1260, 315, 9, (Color){107, 190, 238, 255});
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
                    DrawVehicleIcon(vehicle->serviceType, (Vector2){1275, (float)y + 7});
                    DrawText(TextFormat("%s %d  %s", ServiceName(vehicle->serviceType),
                             ServiceUnitNumber(simulation, i),
                             selectable ? "AVAILABLE" : vehicle->available ? "CALL CLOSED" : "BUSY"),
                             1294, y + 2, 8,
                             selectable ? (Color){196, 211, 220, 255} : GRAY);
                    if (selectable) {
                        float unitEta = SimulationEstimateUnitETA(simulation, i, incidentIndex);
                        DrawText(unitEta < 0.0f ? "ETA --" : TextFormat("ETA %.1fs", unitEta),
                                 1482, y + 2, 8, (Color){107, 190, 238, 255});
                        DrawRectangleLinesEx(ManualVehicleCheckboxBounds(row), 1,
                                             checked ? SKYBLUE : (Color){105, 129, 143, 255});
                        if (checked) DrawText("x", 1545, y, 10, SKYBLUE);
                    } else {
                        DrawText(vehicle->available ? "LIMIT" : "IN USE",
                                 1510, y + 2, 8, GRAY);
                    }
                }
                if (rowCount == 0)
                    DrawText("No suitable vehicles are available; leave this call waiting.",
                             1260, 331, 8, ORANGE);
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
                    DrawText("AUTO OPERATOR REASON", 1260, 313, 9,
                             (Color){107, 190, 238, 255});
                    DrawWrappedText(call->decisionReason, 1260, 327, 9, 56, 12,
                                    (Color){207, 219, 225, 255});
                    assignedStartY = 357;
                }
                DrawText("ASSIGNED UNITS", 1260, assignedStartY, 9,
                         (Color){107, 190, 238, 255});
                int shown = 0;
                for (int i = 0; i < simulation->vehicleCount && shown < MAX_VEHICLES; i++) {
                    const Vehicle *vehicle = &simulation->vehicles[i];
                    if (vehicle->assignedIncident != incidentIndex) continue;
                    int y = assignedStartY + 15 + shown * 21;
                    DrawVehicleIcon(vehicle->serviceType, (Vector2){1275, (float)y + 8});
                    const char *unitState = vehicle->activity == UNIT_TRANSPORTING ? "TRANSPORTING" :
                                            vehicle->activity == UNIT_RETURNING ? "RETURNING" :
                                            vehicle->arrived ? "ON SCENE" : "EN ROUTE";
                    DrawText(TextFormat("%s %d  %s", ServiceName(vehicle->serviceType),
                             ServiceUnitNumber(simulation, i), unitState),
                             1294, y + 2, 8, RAYWHITE);
                    float unitEta = SimulationEstimateUnitETA(simulation, i, incidentIndex);
                    DrawText(vehicle->activity == UNIT_TRANSPORTING ?
                             (unitEta < 0.0f ? "HOSP --" : TextFormat("HOSP %.1fs", unitEta)) :
                             vehicle->activity == UNIT_RETURNING ? "RETURNING" :
                             vehicle->arrived ? "ARRIVED" :
                             unitEta < 0.0f ? "ETA --" : TextFormat("ETA %.1fs", unitEta),
                             1482, y + 2, 8, (Color){107, 190, 238, 255});
                    shown++;
                }
                if (!call->dispatched)
                    DrawText("Waiting for enough suitable units; dispatcher retries automatically.",
                             1260, 433, 8, ORANGE);
            }

            int manualSelecting = simulation->dispatchMode == DISPATCH_MANUAL && !call->dispatched;
            int timelineY = manualSelecting ? 504 : 480;
            float callToDispatch = call->dispatched ? call->dispatchedAt - call->receivedAt : -1.0f;
            DrawText(callToDispatch >= 0.0f ?
                     TextFormat("RECEIVED T+%.1f  |  CLASSIFIED T+%.1f  |  DISPATCH %.1fs",
                                call->receivedAt, call->classifiedAt, callToDispatch) :
                     TextFormat("RECEIVED T+%.1f  |  CLASSIFIED T+%.1f  |  AWAITING DISPATCH",
                                call->receivedAt, call->classifiedAt),
                     1260, timelineY, 8, (Color){155, 185, 202, 255});
            DrawText(TextFormat("First arrival: %s", incident->responseRecorded ?
                     TextFormat("%.1fs after call", incident->responseTime) : "pending"),
                     1260, timelineY + 11, 8, (Color){155, 185, 202, 255});
            if (call->feedbackReady)
                DrawText(call->callerSatisfied ? "CALLER FEEDBACK: SATISFIED" :
                         "CALLER FEEDBACK: DISSATISFIED", 1260, timelineY + 22, 9,
                         call->callerSatisfied ? GREEN : (Color){255, 105, 110, 255});
        } else if (simulation->dispatchMode == DISPATCH_AUTOMATIC) {
            DrawText("AUTO OPERATOR IS CLASSIFYING THIS REPORT", 1260, 268, 9, SKYBLUE);
            DrawWrappedText(call->decisionReason, 1260, 288, 9, 55, 12,
                            (Color){207, 219, 225, 255});
        }
    } else if (comparisonReady && comparison) {
        DrawText("STRATEGY COMPARISON", 1260, 166, 12, (Color){107, 190, 238, 255});
        DrawText("Nearest first", 1260, 194, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->nearestFirst.resolved,
                 comparison->nearestFirst.failed), 1260, 214, 11, (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->nearestFirst.averageResponseTime),
                 1260, 233, 11, (Color){177, 195, 207, 255});
        DrawText("Priority dispatch", 1260, 265, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->priority.resolved,
                 comparison->priority.failed), 1260, 285, 11, (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->priority.averageResponseTime),
                 1260, 304, 11, (Color){177, 195, 207, 255});
    } else {
        DrawText("Select a call from the queue", 1260, 168, 12, RAYWHITE);
        DrawText("or click a marker on the map.", 1260, 188, 11,
                 (Color){156, 177, 191, 255});
        DrawText(TextFormat("Received %d calls  |  Active incidents %d",
                 simulation->callCount, SimulationCountUnresolved(simulation)),
                 1260, 225, 10, (Color){177, 195, 207, 255});
    }

    DrawLine(1260, 539, 1565, 539, (Color){43, 67, 83, 255});
    DrawText("SERVICE AVAILABILITY", 1260, 543, 10, (Color){107, 190, 238, 255});
    DrawServiceSummary(simulation, INCIDENT_FIRE, 1254, 558, IncidentColor(INCIDENT_FIRE));
    DrawServiceSummary(simulation, INCIDENT_CRIME, 1254, 592, IncidentColor(INCIDENT_CRIME));
    DrawServiceSummary(simulation, INCIDENT_ACCIDENT, 1254, 626, IncidentColor(INCIDENT_MEDICAL));
    DrawOperationalSummary(simulation);
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

static void DrawMapContents(const Simulation *simulation, int selectedIncident,
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
                           vehicle->serviceType == INCIDENT_CRIME ? BLUE : DARKGREEN;
        Vector2 from = vehicle->position;
        for (int point = vehicle->routeIndex; point < vehicle->routeCount; point++) {
            Vector2 to = vehicle->route[point];
            DrawLineEx(from, to, 3.0f, Fade(routeColor, 0.78f));
            from = to;
        }
    }

    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        DrawVehicleIcon(vehicle->serviceType, vehicle->position);
        if (vehicle->activity == UNIT_TRANSPORTING) {
            // A small passenger marker rides with the ambulance only during transport.
            DrawRectangle((int)vehicle->position.x - 3, (int)vehicle->position.y - 9, 7, 2,
                          (Color){228, 239, 232, 255});
            DrawCircle((int)vehicle->position.x - 1, (int)vehicle->position.y - 11, 1.5f,
                       (Color){238, 199, 163, 255});
            DrawLine((int)vehicle->position.x + 1, (int)vehicle->position.y - 9,
                     (int)vehicle->position.x + 5, (int)vehicle->position.y - 9,
                     (Color){77, 220, 162, 255});
            DrawText("TO HOSPITAL", (int)vehicle->position.x + 10,
                     (int)vehicle->position.y - 11, 6, (Color){114, 224, 190, 255});
        } else if (vehicle->activity == UNIT_RETURNING)
            DrawText("RETURNING", (int)vehicle->position.x + 10,
                     (int)vehicle->position.y - 11, 6, (Color){190, 205, 216, 255});
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
        DrawText(IsIncidentFinished(incident) ||
                 incident->status == INCIDENT_SECURING ||
                 incident->status == INCIDENT_FIREFIGHTING ||
                 incident->status == INCIDENT_TREATING ||
                 incident->status == INCIDENT_TRANSPORTING ? IncidentStatusName(incident->status) :
                 TextFormat("S%d  %d/%d", incident->severity,
                            CountArrivedUnits(simulation, i), CountRequiredUnits(incident)),
                 (int)incident->position.x + 12, (int)incident->position.y - 8, 10, color);
    }
    if (simulation->apartmentFireDrill && simulation->incidentCount > 0 &&
        simulation->incidents[0].fireSpreadOccurred &&
        !IsIncidentFinished(&simulation->incidents[0])) {
        Vector2 nearby = {883, 163};
        float flicker = 6.0f + 2.5f * sinf(simulation->elapsedTime * 7.0f);
        DrawTriangle((Vector2){nearby.x, nearby.y - flicker},
                     (Vector2){nearby.x - 5, nearby.y + 5},
                     (Vector2){nearby.x + 5, nearby.y + 5}, ORANGE);
        DrawCircle((int)nearby.x - 3, (int)nearby.y - 14, 4, (Color){150, 161, 166, 100});
    }
    for (int i = 0; i < simulation->callCount; i++) {
        const EmergencyCall *call = &simulation->calls[i];
        if (call->incidentIndex >= 0) continue;
        DrawCircleV(call->location, 11.0f, (Color){134, 86, 28, 190});
        DrawCircleLines((int)call->location.x, (int)call->location.y, 11.0f, ORANGE);
        DrawText("?", (int)call->location.x - 4, (int)call->location.y - 7, 14, RAYWHITE);
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
                                const Simulation *simulation)
{
    DrawModalBackdrop();
    DrawText("CONTROL ROOM SETTINGS", 510, 245, 24, RAYWHITE);
    DrawText("Display options apply to the city map only.", 512, 280, 13,
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
    DrawText("Wind can spread severe fires; other conditions change map visibility.",
             550, 523, 10, (Color){154, 181, 198, 255});
    DrawText(TextFormat("Dispatch: %s  |  Traffic: %s",
             simulation->dispatchMode == DISPATCH_AUTOMATIC ? "Automatic" : "Manual",
             SimulationTrafficLevelName(simulation->trafficLevel)), 550, 544, 12, RAYWHITE);
    DrawText("Keys: 1-4 reports | Space pause | S slow | F fast | N step",
             550, 572, 10, (Color){154, 181, 198, 255});
    DrawText("T traffic | W weather | M queue | C compare | R reset | Esc close",
             550, 590, 10, (Color){154, 181, 198, 255});
    DrawButton(modalCloseButton, "CLOSE", BLUE);
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
    UIOverlay overlay = OVERLAY_NONE;
    int selectedCall = -1;
    int selectedIncident = -1;
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
        if (IsKeyPressed(KEY_ONE)) selectedType = INCIDENT_FIRE;

        if (IsKeyPressed(KEY_TWO)) selectedType = INCIDENT_CRIME;

        if (IsKeyPressed(KEY_THREE)) selectedType = INCIDENT_ACCIDENT;

        if (IsKeyPressed(KEY_FOUR)) selectedType = INCIDENT_MEDICAL;

        if (IsKeyPressed(KEY_A)) {
            int drillSeverity = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 1 : 3;
            if (SimulationStartApartmentFireDrillAtSeverity(&simulation, drillSeverity)) {
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
            } else if (overlay != OVERLAY_NONE) {
                if (CheckCollisionPointRec(mouse, modalCloseButton)) {
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
                if (row >= 0 && row < visible) {
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
                if (!call->classified && !call->dispatched) {
                    const IncidentType types[] = {INCIDENT_FIRE, INCIDENT_CRIME,
                                                  INCIDENT_ACCIDENT, INCIDENT_MEDICAL};
                    for (int i = 0; i < 4; i++)
                        if (CheckCollisionPointRec(mouse, callTypeButtons[i]))
                            manualCallType = types[i];
                    for (int i = 0; i < 3; i++)
                        if (CheckCollisionPointRec(mouse, callPriorityButtons[i]))
                            manualCallSeverity = i + 1;
                } else if (call->classified && !call->dispatched && selectedIncident >= 0 &&
                           !IsIncidentFinished(&simulation.incidents[selectedIncident])) {
                    Incident *incident = &simulation.incidents[selectedIncident];
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
            } else if (CheckCollisionPointRec(mouse, cityView)) {
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
        UpdateCallerFeedbackToast(&simulation, operatorNotice, sizeof(operatorNotice),
                                  &operatorNoticeMatched, &operatorNoticeUntil);

        // Input can create or reset calls after the hit-test queue was built.
        // Rebuild it before drawing so the same frame never renders stale rows.
        queueCount = BuildCallQueueOrder(&simulation, queueOrder);



        BeginTextureMode(uiCanvas);
        ClearBackground((Color){7, 17, 29, 255});
        BeginMode2D(renderCamera);

        DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){7, 17, 29, 255});
        DrawRectangle(0, 0, UI_WIDTH, 88, (Color){9, 25, 42, 255});
        DrawLine(0, 87, UI_WIDTH, 87, (Color){34, 74, 101, 255});
        DrawCityMark();
        DrawText("EMERGENCY CITY SIMULATION", 82, 13, 23, RAYWHITE);
        DrawText("CONTROL ROOM  /  LIVE DISPATCH", 84, 49, 11,
                 (Color){124, 190, 222, 255});
        int waitingCalls = 0;
        for (int call = 0; call < simulation.callCount; call++)
            if (!simulation.calls[call].dispatched && !CallIsTerminal(&simulation, call)) waitingCalls++;
        DrawText(TextFormat("CALLS %d  |  WAIT %d  |  ACTIVE %d",
                 simulation.callCount, waitingCalls, SimulationCountUnresolved(&simulation)),
                 500, 21, 11, (Color){177, 198, 211, 255});
        int onTime, arrivals;
        OnTimeArrivalCounts(&simulation, &onTime, &arrivals);
        DrawText(TextFormat("HAZARDS %d  |  RES %d  |  FAIL %d  |  ON-TIME %s",
                 CountActiveHazards(&simulation),
                 SimulationCountStatus(&simulation, INCIDENT_RESOLVED),
                 SimulationCountStatus(&simulation, INCIDENT_FAILED),
                 arrivals > 0 ? TextFormat("%d/%d", onTime, arrivals) : "--"), 500, 47, 9,
                 (Color){124, 190, 222, 255});
        DrawHeaderService(&simulation, INCIDENT_FIRE, 805, "FIRE", IncidentColor(INCIDENT_FIRE));
        DrawHeaderService(&simulation, INCIDENT_CRIME, 970, "POLICE", IncidentColor(INCIDENT_CRIME));
        DrawHeaderService(&simulation, INCIDENT_ACCIDENT, 1135, "AMBULANCE", IncidentColor(INCIDENT_MEDICAL));
        DrawRectangleRounded((Rectangle){1300, 18, 272, 50}, 0.12f, 7,
                             (Color){17, 42, 60, 255});
        DrawText(TextFormat("TIME %05.1fs  |  CARS %d", simulation.elapsedTime,
                 simulation.civilianCount), 1315, 24, 10, (Color){179, 198, 211, 255});
        Color conditionColor = simulation.cityCondition == CITY_FOG ?
                               (Color){184, 202, 214, 255} :
                               simulation.cityCondition == CITY_RAIN ?
                               (Color){97, 184, 226, 255} :
                               (Color){113, 204, 156, 255};
        DrawText(TextFormat("%s  |  TRAFFIC %s",
                 SimulationCityConditionName(simulation.cityCondition),
                 SimulationTrafficLevelName(simulation.trafficLevel)),
                 1315, 43, 10, conditionColor);

        DrawIncidentQueue(&simulation, queueOrder, queueCount, selectedCall);
        DrawPanel(mapPanel);
        DrawText("CITY MAP", 320, 119, 16, RAYWHITE);
        DrawText("Click map to log or inspect a call  |  1-4 report type", 420, 122, 11,
                 (Color){139, 164, 179, 255});
        DrawIncidentIcon(INCIDENT_FIRE, (Vector2){940, 125}, 7, RED);
        DrawText("Fire", 952, 120, 10, RAYWHITE);
        DrawVehicleIcon(INCIDENT_CRIME, (Vector2){1000, 125});
        DrawText("Police", 1014, 120, 10, RAYWHITE);
        DrawVehicleIcon(INCIDENT_ACCIDENT, (Vector2){1070, 125});
        DrawText("EMS", 1084, 120, 10, RAYWHITE);
        DrawLine(1112, 125, 1132, 125, (Color){72, 181, 255, 255});
        DrawText("Route", 1138, 120, 10, RAYWHITE);
        DrawCircleLines(1180, 125, 6, ORANGE);
        DrawText("Call", 1190, 120, 10, RAYWHITE);
        DrawRectangleRounded(mapClip, 0.02f, 8, (Color){14, 27, 37, 255});

        // The map uses its own camera and draws directly to the 2x render
        // texture. End the UI camera before that pass, then restore it for the
        // details panel and controls so they retain the correct size/position.
        EndMode2D();
        DrawMapContents(&simulation, selectedIncident, cityView,
                        showRoutes, showCivilianTraffic);

        BeginMode2D(renderCamera);
        DrawIncidentDetails(&simulation, selectedCall, comparisonReady, &comparison,
                            selectedVehicleMask, manualCallType, manualCallSeverity);
        DrawEventFeed(&simulation);

        DrawPanel((Rectangle){18, 818, 1564, 68});
        DrawText("SIMULATION CONTROLS", 34, 833, 13,
                 (Color){132, 194, 225, 255});
        DrawText("A serious fire drill | SHIFT+A minor | 1-4 report | SPACE pause | S slow | F fast | N step",
                 34, 853, 9, (Color){158, 180, 193, 255});
        DrawText("T traffic | W weather | C compare | M queue | R reset",
                 34, 868, 9, (Color){158, 180, 193, 255});
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
        if (simulation.dispatchMode == DISPATCH_MANUAL && selectedCall >= 0 &&
            selectedCall < simulation.callCount && simulation.calls[selectedCall].classified &&
            !simulation.calls[selectedCall].dispatched) {
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
            DrawSettingsOverlay(showRoutes, showCivilianTraffic, &simulation);

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
    UnloadUiFont();
    CloseWindow();

    return 0;

}
