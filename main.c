#include "raylib.h"

#include "incidents.h"

#include "simulation.h"
#include "ui_layout.h"

#include <stdio.h>
#include <math.h>



#define SCREEN_WIDTH 1440
#define SCREEN_HEIGHT 810
#define UI_WIDTH 1600
#define UI_HEIGHT 900
#define UI_RENDER_SCALE 2
#define CITY_WIDTH 1100
#define CITY_HEIGHT 700
#define QUEUE_VISIBLE 8


static const Rectangle queuePanel = {18, 104, 270, 700};
static const Rectangle mapPanel = {300, 104, 930, 700};
static const Rectangle detailsPanel = {1242, 104, 340, 700};
static const Rectangle mapClip = {310, 150, 910, 642};
static const float queueRowsY = 158.0f;
static const float queueRowHeight = 77.0f;

static Rectangle compareButton = {460, 830, 90, 44};
static Rectangle pauseButton = {558, 830, 90, 44};
static Rectangle slowButton = {656, 830, 90, 44};
static Rectangle normalButton = {754, 830, 90, 44};
static Rectangle fastButton = {852, 830, 90, 44};
static Rectangle stepButton = {950, 830, 90, 44};
static Rectangle trafficButton = {1048, 830, 95, 44};
static Rectangle simulateButton = {1151, 830, 115, 44};
static Rectangle resetButton = {1470, 830, 100, 44};
static const Rectangle dispatchModeButton = {1434, 112, 132, 30};
static const Rectangle dispatchSelectedButton = {1260, 440, 306, 30};

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



static void DrawCity(void)

{

    DrawRectangle(0, 0, CITY_WIDTH, CITY_HEIGHT, (Color){27, 48, 60, 255});



    // Simplified roads

    DrawRectangle(0, 290, CITY_WIDTH, 72, (Color){75, 91, 103, 255});

    DrawRectangle(355, 80, 82, 550, (Color){75, 91, 103, 255});

    DrawRectangle(730, 80, 82, 550, (Color){75, 91, 103, 255});



    for (int x = 0; x < CITY_WIDTH; x += 55)

        DrawRectangle(x, 324, 30, 4, (Color){188, 204, 211, 255});

    for (int y = 90; y < 630; y += 55) {

        DrawRectangle(394, y, 4, 30, (Color){188, 204, 211, 255});

        DrawRectangle(769, y, 4, 30, (Color){188, 204, 211, 255});

    }



    DrawRectangle(70, 105, 210, 130, (Color){40, 63, 77, 255});

    DrawRectangle(490, 105, 175, 130, (Color){40, 63, 77, 255});

    DrawRectangle(865, 105, 175, 130, (Color){40, 63, 77, 255});

    DrawRectangle(70, 410, 210, 155, (Color){40, 63, 77, 255});

    DrawRectangle(490, 410, 175, 155, (Color){40, 63, 77, 255});

    DrawRectangle(865, 410, 175, 155, (Color){40, 63, 77, 255});



    DrawText("BUILDING A", 112, 160, 18, (Color){186, 204, 213, 255});

    DrawText("BUILDING B", 520, 160, 18, (Color){186, 204, 213, 255});

    DrawText("BUILDING C", 900, 160, 18, (Color){186, 204, 213, 255});

    DrawText("BUILDING D", 112, 465, 18, (Color){186, 204, 213, 255});

    DrawText("BUILDING E", 520, 465, 18, (Color){186, 204, 213, 255});

    DrawText("BUILDING F", 900, 465, 18, (Color){186, 204, 213, 255});



    // Initial station positions for all nine response units.

    DrawCircle(300, 270, 8, RED);

    DrawCircle(330, 270, 8, RED);

    DrawCircle(315, 245, 8, RED);

    DrawText("FIRE x3", 287, 218, 14, MAROON);

    DrawCircle(650, 380, 8, BLUE);

    DrawCircle(680, 380, 8, BLUE);

    DrawText("POLICE x2", 630, 350, 14, DARKBLUE);

    DrawCircle(770, 270, 8, DARKGREEN);

    DrawCircle(800, 270, 8, DARKGREEN);

    DrawCircle(770, 300, 8, DARKGREEN);

    DrawCircle(800, 300, 8, DARKGREEN);

    DrawText("AMBULANCE x4", 748, 238, 14, DARKGREEN);

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



static void DrawButton(Rectangle rect, const char *label, Color color)

{

    Rectangle shadow = {rect.x + 2, rect.y + 3, rect.width, rect.height};
    DrawRectangleRounded(shadow, 0.18f, 8, (Color){0, 0, 0, 38});
    DrawRectangleRounded(rect, 0.18f, 8, color);
    DrawRectangleRoundedLines(rect, 0.18f, 8, (Color){255, 255, 255, 150});

    int textWidth = MeasureText(label, 11);

    DrawText(label, (int)(rect.x + (rect.width - textWidth) / 2),

             (int)(rect.y + (rect.height - 11) / 2), 11, RAYWHITE);

}



static int IsIncidentFinished(const Incident *incident)
{
    return incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED;
}

static int BuildQueueOrder(const Simulation *simulation, int order[MAX_INCIDENTS])
{
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (IsIncidentFinished(incident)) continue;

        int insertAt = count;
        float priority = SimulationIncidentPriority(incident);
        while (insertAt > 0) {
            const Incident *previous = &simulation->incidents[order[insertAt - 1]];
            float previousPriority = SimulationIncidentPriority(previous);
            if (previousPriority > priority ||
                (previousPriority == priority && previous->waitTime >= incident->waitTime)) break;
            order[insertAt] = order[insertAt - 1];
            insertAt--;
        }
        order[insertAt] = i;
        count++;
    }

    // Keep recent completed calls visible when the active queue is short.
    for (int i = simulation->incidentCount - 1; i >= 0 && count < MAX_INCIDENTS; i--) {
        if (IsIncidentFinished(&simulation->incidents[i])) order[count++] = i;
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
    DrawRectangleRounded(rect, 0.04f, 12, (Color){13, 29, 45, 255});
    DrawRectangleRoundedLines(rect, 0.04f, 12, (Color){39, 62, 79, 255});
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

static void DrawServiceSummary(const Simulation *simulation, IncidentType service,
                               int x, int y, Color color)
{
    const IncidentType iconType = service == INCIDENT_ACCIDENT ? INCIDENT_MEDICAL : service;
    DrawRectangleRounded((Rectangle){(float)x, (float)y, 316, 64}, 0.08f, 8,
                        (Color){20, 42, 60, 255});
    DrawIncidentIcon(iconType, (Vector2){(float)x + 21, (float)y + 20}, 9, color);
    DrawText(ServiceName(service), x + 42, y + 9, 14, RAYWHITE);
    DrawText(TextFormat("Available %d  |  Active %d", 
             SimulationCountAvailableVehicles(simulation, service),
             CountActiveForService(simulation, service)), x + 42, y + 31, 11,
             (Color){179, 198, 211, 255});
    DrawText(TextFormat("Resolved %d  |  Failed %d", simulation->resolvedByType[service],
             simulation->failedByType[service]), x + 42, y + 46, 10,
             (Color){141, 161, 175, 255});
}

static void DrawIncidentQueue(const Simulation *simulation, const int order[MAX_INCIDENTS],
                              int orderCount, int selectedIncident)
{
    DrawPanel(queuePanel);
    DrawText("INCIDENT QUEUE", 34, 120, 17, RAYWHITE);
    DrawText(TextFormat("%d active", SimulationCountUnresolved(simulation)), 34, 140, 11,
             (Color){134, 160, 177, 255});
    DrawText("PRIORITY", 211, 140, 10, (Color){134, 160, 177, 255});

    int visible = orderCount < QUEUE_VISIBLE ? orderCount : QUEUE_VISIBLE;
    if (visible == 0) {
        DrawText("No incidents yet", 42, 180, 13, (Color){160, 181, 195, 255});
        DrawText("Click the city map to add one.", 42, 202, 11, (Color){116, 142, 159, 255});
        return;
    }

    for (int row = 0; row < visible; row++) {
        int index = order[row];
        const Incident *incident = &simulation->incidents[index];
        float y = queueRowsY + row * queueRowHeight;
        Rectangle card = {26, y, 254, 70};
        Color typeColor = IncidentColor(incident->type);
        DrawRectangleRounded(card, 0.08f, 8,
            index == selectedIncident ? (Color){30, 57, 78, 255} : (Color){18, 38, 54, 255});
        DrawRectangleRounded((Rectangle){card.x, card.y, 4, card.height}, 0.3f, 4, typeColor);
        if (index == selectedIncident)
            DrawRectangleRoundedLines(card, 0.08f, 8, (Color){71, 151, 205, 255});

        DrawIncidentIcon(incident->type, (Vector2){48, y + 27}, 10, typeColor);
        DrawText(IncidentTypeName(incident->type), 67, (int)y + 10, 12, RAYWHITE);
        DrawText(IncidentStatusName(incident->status), 67, (int)y + 29, 10,
                 IsIncidentFinished(incident) ? (incident->status == INCIDENT_FAILED ? RED : GREEN) :
                 (Color){166, 188, 202, 255});
        DrawText(TextFormat("SEV %d   P%.0f", incident->severity,
                 SimulationIncidentPriority(incident)), 67, (int)y + 48, 10,
                 incident->severity >= 3 ? (Color){255, 112, 91, 255} :
                 incident->severity == 2 ? (Color){255, 189, 85, 255} :
                 (Color){130, 181, 209, 255});
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

static void DrawIncidentDetails(const Simulation *simulation, int selectedIncident,
                                int comparisonReady, const StrategyComparison *comparison,
                                unsigned int selectedVehicleMask)
{
    DrawPanel(detailsPanel);
    DrawText("INCIDENT DETAILS", 1260, 120, 17, RAYWHITE);
    DrawButton(dispatchModeButton,
               simulation->dispatchMode == DISPATCH_AUTOMATIC ? "AUTO DISPATCH" : "MANUAL DISPATCH",
               simulation->dispatchMode == DISPATCH_AUTOMATIC ? (Color){34, 111, 82, 255} : BLUE);
    DrawLine(1260, 146, 1565, 146, (Color){43, 67, 83, 255});

    if (selectedIncident >= 0 && selectedIncident < simulation->incidentCount) {
        const Incident *incident = &simulation->incidents[selectedIncident];
        Color typeColor = IncidentColor(incident->type);
        DrawIncidentIcon(incident->type, (Vector2){1283, 176}, 16, typeColor);
        DrawText(IncidentTypeName(incident->type), 1310, 158, 15, RAYWHITE);
        DrawText(IncidentStatusName(incident->status), 1310, 180, 11,
                 IsIncidentFinished(incident) ? (incident->status == INCIDENT_FAILED ? RED : GREEN) :
                 (Color){122, 194, 232, 255});
        DrawLine(1260, 210, 1565, 210, (Color){43, 67, 83, 255});

        float remaining = incident->deadline - incident->ageTime;
        if (remaining < 0.0f) remaining = 0.0f;
        DrawText(TextFormat("Severity %d     Priority %.0f", incident->severity,
                 SimulationIncidentPriority(incident)), 1260, 225, 12, RAYWHITE);
        DrawText(TextFormat("Time remaining %.1fs", remaining), 1260, 247, 11,
                 remaining < 6.0f && !IsIncidentFinished(incident) ? ORANGE :
                 (Color){177, 195, 207, 255});
        DrawText(TextFormat("Assigned %d / %d   |   Arrived %d", 
                 CountAssignedUnits(simulation, selectedIncident), incident->requiredUnits,
                 CountArrivedUnits(simulation, selectedIncident)), 1260, 267, 11,
                 (Color){177, 195, 207, 255});
        float eta = SimulationEstimateIncidentETA(simulation, selectedIncident);
        DrawText(eta < 0.0f ? "ETA -- (units unavailable)" :
                 TextFormat("ETA %.1fs  |  %s traffic", eta,
                            SimulationTrafficLevelName(simulation->trafficLevel)),
                 1260, 287, 11,
                 (Color){177, 195, 207, 255});
        if (incident->responseRecorded)
            DrawText(TextFormat("First arrival %.1fs", incident->responseTime),
                     1260, 307, 11, (Color){177, 195, 207, 255});

        if (simulation->dispatchMode == DISPATCH_MANUAL) {
            int remainingUnits = incident->requiredUnits - CountAssignedUnits(simulation, selectedIncident);
            if (remainingUnits < 0) remainingUnits = 0;
            DrawText(TextFormat("CHOOSE UNITS  (%d more needed)", remainingUnits),
                     1260, 330, 11, (Color){107, 190, 238, 255});
            int shown = 0;
            IncidentType service = SimulationServiceForIncident(incident->type);
            for (int i = 0; i < simulation->vehicleCount && shown < 4; i++) {
                const Vehicle *vehicle = &simulation->vehicles[i];
                if (!vehicle->available || vehicle->serviceType != service) continue;
                int y = 350 + shown * 22;
                Rectangle row = {1258, (float)y - 2, 308, 21};
                int checked = (selectedVehicleMask & (1u << i)) != 0;
                DrawRectangleRounded(row, 0.18f, 4,
                    checked ? (Color){29, 68, 91, 255} : (Color){18, 38, 54, 255});
                DrawVehicleIcon(vehicle->serviceType, (Vector2){1275, (float)y + 7});
                DrawText(TextFormat("%s %d   AVAILABLE", ServiceName(service),
                         ServiceUnitNumber(simulation, i)), 1295, y + 1, 10,
                         (Color){196, 211, 220, 255});
                DrawRectangleLinesEx((Rectangle){1539, (float)y + 1, 15, 15}, 1,
                                     checked ? SKYBLUE : (Color){105, 129, 143, 255});
                if (checked) DrawText("x", 1542, y + 1, 12, SKYBLUE);
                shown++;
            }
            if (shown == 0)
                DrawText("No suitable units available", 1260, 354, 10,
                         (Color){156, 177, 191, 255});
            DrawButton(dispatchSelectedButton,
                       TextFormat("DISPATCH SELECTED  %d", CountSelectedVehicles(selectedVehicleMask)),
                       CountSelectedVehicles(selectedVehicleMask) > 0 ? BLUE : DARKGRAY);
        } else {
            DrawText("RESPONSE UNITS", 1260, 330, 12, (Color){107, 190, 238, 255});
            int shown = 0;
            for (int i = 0; i < simulation->vehicleCount; i++) {
                const Vehicle *vehicle = &simulation->vehicles[i];
                if (vehicle->assignedIncident != selectedIncident) continue;
                int y = 354 + shown * 24;
                DrawVehicleIcon(vehicle->serviceType, (Vector2){1278, (float)y + 9});
                DrawText(TextFormat("%s %d", ServiceName(vehicle->serviceType),
                         ServiceUnitNumber(simulation, i)), 1300, y + 1, 10, RAYWHITE);
                DrawText(vehicle->arrived ? "On scene" : "En route", 1300, y + 13, 9,
                         vehicle->arrived ? GREEN : (Color){156, 188, 207, 255});
                shown++;
            }
            if (shown == 0)
                DrawText("Waiting for automatic dispatch", 1260, 354, 10,
                         (Color){156, 177, 191, 255});
        }
    } else if (comparisonReady && comparison) {
        DrawText("STRATEGY COMPARISON", 1260, 166, 12, (Color){107, 190, 238, 255});
        DrawText("Nearest first", 1260, 194, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->nearestFirst.resolved,
                 comparison->nearestFirst.failed), 1260, 214, 11,
                 (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->nearestFirst.averageResponseTime),
                 1260, 233, 11, (Color){177, 195, 207, 255});
        DrawText("Priority dispatch", 1260, 265, 12, RAYWHITE);
        DrawText(TextFormat("Resolved %d  |  Failed %d", comparison->priority.resolved,
                 comparison->priority.failed), 1260, 285, 11,
                 (Color){177, 195, 207, 255});
        DrawText(TextFormat("Avg arrival %.1fs", comparison->priority.averageResponseTime),
                 1260, 304, 11, (Color){177, 195, 207, 255});
    } else {
        DrawText("Select a call from the queue", 1260, 168, 12, RAYWHITE);
        DrawText("or click a marker on the map.", 1260, 188, 11,
                 (Color){156, 177, 191, 255});
        DrawText(TextFormat("Created %d   Resolved %d   Failed %d",
                 simulation->incidentCount,
                 SimulationCountStatus(simulation, INCIDENT_RESOLVED),
                 SimulationCountStatus(simulation, INCIDENT_FAILED)),
                 1260, 225, 11, (Color){177, 195, 207, 255});
    }

    DrawLine(1260, 478, 1565, 478, (Color){43, 67, 83, 255});
    DrawText("SERVICE AVAILABILITY", 1260, 492, 12, (Color){107, 190, 238, 255});
    DrawServiceSummary(simulation, INCIDENT_FIRE, 1254, 514, IncidentColor(INCIDENT_FIRE));
    DrawServiceSummary(simulation, INCIDENT_CRIME, 1254, 584, IncidentColor(INCIDENT_CRIME));
    DrawServiceSummary(simulation, INCIDENT_ACCIDENT, 1254, 654, IncidentColor(INCIDENT_MEDICAL));
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
                           Rectangle cityView)
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
    DrawCity();
    for (int i = 0; i < simulation->civilianCount; i++)
        DrawCivilianCar(&simulation->civilianCars[i]);

    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (vehicle->available || vehicle->arrived || vehicle->routeCount < 2) continue;

        Color routeColor = vehicle->serviceType == INCIDENT_FIRE ? RED :
                           vehicle->serviceType == INCIDENT_CRIME ? BLUE : DARKGREEN;
        Vector2 from = vehicle->position;
        for (int point = vehicle->routeIndex; point < vehicle->routeCount; point++) {
            Vector2 to = vehicle->route[point];
            DrawLineEx(from, to, 3.0f, Fade(routeColor, 0.78f));
            from = to;
        }
    }

    for (int i = 0; i < simulation->vehicleCount; i++)
        DrawVehicleIcon(simulation->vehicles[i].serviceType, simulation->vehicles[i].position);

    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        Color color = incident->status == INCIDENT_RESOLVED ? GREEN :
                      incident->status == INCIDENT_FAILED ? GRAY :
                      incident->severity >= 3 ? RED :
                      incident->severity == 2 ? ORANGE : IncidentColor(incident->type);
        float radius = incident->severity == 1 ? 7.0f :
                       incident->severity == 2 ? 10.0f : 13.0f;
        DrawCircleV(incident->position, radius, Fade(color, 0.32f));
        DrawCircleLines((int)incident->position.x, (int)incident->position.y, radius, color);
        DrawIncidentIcon(incident->type, incident->position, radius * 0.58f, color);
        if (i == selectedIncident)
            DrawCircleLines((int)incident->position.x, (int)incident->position.y,
                            radius + 6.0f, GOLD);
        DrawText(IsIncidentFinished(incident) ? IncidentStatusName(incident->status) :
                 TextFormat("S%d  %d/%d", incident->severity,
                            CountArrivedUnits(simulation, i), incident->requiredUnits),
                 (int)incident->position.x + 12, (int)incident->position.y - 8, 10, color);
    }
    EndMode2D();
    EndScissorMode();
}



int main(void)

{

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Emergency City Simulation");
    SetWindowMinSize(1120, 630);

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
    int selectedIncident = -1;
    unsigned int selectedVehicleMask = 0;
    StrategyComparison comparison = {0};



    while (!WindowShouldClose()) {

        // Fit the fixed design canvas inside the current client area. The same
        // viewport is used to convert pointer positions, so letterboxing and
        // high-DPI scaling cannot shift clicks away from the drawn controls.
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        Rectangle viewport = FitVirtualViewport(screenWidth, screenHeight,
                                                UI_WIDTH, UI_HEIGHT);

        if (IsKeyPressed(KEY_ONE)) selectedType = INCIDENT_FIRE;

        if (IsKeyPressed(KEY_TWO)) selectedType = INCIDENT_CRIME;

        if (IsKeyPressed(KEY_THREE)) selectedType = INCIDENT_ACCIDENT;

        if (IsKeyPressed(KEY_FOUR)) selectedType = INCIDENT_MEDICAL;

        if (IsKeyPressed(KEY_SPACE)) paused = !paused;

        if (IsKeyPressed(KEY_S)) {
            slowMotion = !slowMotion;
            if (slowMotion) fastForward = false;
        }

        if (IsKeyPressed(KEY_F)) {
            fastForward = !fastForward;
            if (fastForward) slowMotion = false;
        }

        bool stepRequested = IsKeyPressed(KEY_N);
        if (IsKeyPressed(KEY_T))
            SimulationSetTrafficLevel(&simulation,
                (TrafficLevel)((simulation.trafficLevel + 1) % TRAFFIC_LEVEL_COUNT));

        if (IsKeyPressed(KEY_M)) SimulationSimulateQueue(&simulation);
        if (IsKeyPressed(KEY_C)) {
            SimulationRunStrategyComparison(&comparison);
            comparisonReady = true;
            selectedIncident = -1;
        }

        if (IsKeyPressed(KEY_R)) {
            SimulationInit(&simulation);
            paused = false;
            fastForward = false;
            slowMotion = false;
            comparisonReady = false;
            selectedIncident = -1;
            selectedVehicleMask = 0;
        }



        Rectangle cityView = FitCityToPanel(mapClip);
        int queueOrder[MAX_INCIDENTS];
        int queueCount = BuildQueueOrder(&simulation, queueOrder);

        Vector2 mousePosition = GetMousePosition();
        Vector2 mouse = {0};
        bool mouseInCanvas = ScreenToVirtualPoint(mousePosition, viewport,
                                                  UI_WIDTH, UI_HEIGHT, &mouse);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouseInCanvas) {

            if (CheckCollisionPointRec(mouse, compareButton)) {

                SimulationRunStrategyComparison(&comparison);
                comparisonReady = true;
                selectedIncident = -1;
                selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, dispatchModeButton)) {

                simulation.dispatchMode = simulation.dispatchMode == DISPATCH_AUTOMATIC ?
                                          DISPATCH_MANUAL : DISPATCH_AUTOMATIC;
                selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, dispatchSelectedButton) &&
                       simulation.dispatchMode == DISPATCH_MANUAL && selectedIncident >= 0) {

                SimulationDispatchVehicles(&simulation, selectedIncident, selectedVehicleMask);
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

            } else if (CheckCollisionPointRec(mouse, trafficButton)) {

                SimulationSetTrafficLevel(&simulation,
                    (TrafficLevel)((simulation.trafficLevel + 1) % TRAFFIC_LEVEL_COUNT));

            } else if (CheckCollisionPointRec(mouse, simulateButton)) {

                SimulationSimulateQueue(&simulation);

            } else if (CheckCollisionPointRec(mouse, resetButton)) {

                SimulationInit(&simulation);

                paused = false;

                fastForward = false;
                slowMotion = false;

                comparisonReady = false;
                selectedIncident = -1;
                selectedVehicleMask = 0;

            } else if (CheckCollisionPointRec(mouse, queuePanel) &&
                       mouse.y >= queueRowsY && queueCount > 0) {
                int row = (int)((mouse.y - queueRowsY) / queueRowHeight);
                int visible = queueCount < QUEUE_VISIBLE ? queueCount : QUEUE_VISIBLE;
                if (row >= 0 && row < visible) {
                    int clickedIncident = queueOrder[row];
                    selectedIncident = clickedIncident == selectedIncident ? -1 : clickedIncident;
                    selectedVehicleMask = 0;
                }
            } else if (simulation.dispatchMode == DISPATCH_MANUAL && selectedIncident >= 0 &&
                       CheckCollisionPointRec(mouse, detailsPanel)) {
                Incident *incident = &simulation.incidents[selectedIncident];
                int remaining = incident->requiredUnits - CountAssignedUnits(&simulation, selectedIncident);
                IncidentType service = SimulationServiceForIncident(incident->type);
                if (remaining > 0 && !IsIncidentFinished(incident)) {
                    int row = 0;
                    for (int i = 0; i < simulation.vehicleCount && row < 4; i++) {
                        Vehicle *vehicle = &simulation.vehicles[i];
                        if (!vehicle->available || vehicle->serviceType != service) continue;
                        Rectangle vehicleRow = {1258, 348.0f + row * 22.0f, 308, 21};
                        if (CheckCollisionPointRec(mouse, vehicleRow)) {
                            unsigned int bit = 1u << i;
                            if (selectedVehicleMask & bit) selectedVehicleMask &= ~bit;
                            else if (CountSelectedVehicles(selectedVehicleMask) < remaining)
                                selectedVehicleMask |= bit;
                            break;
                        }
                        row++;
                    }
                }
            } else if (CheckCollisionPointRec(mouse, cityView)) {
                Vector2 cityPoint = MapCanvasToCity(mouse, cityView);
                int clickedIncident = FindIncidentAtPosition(&simulation, cityPoint);
                if (clickedIncident >= 0) {
                    selectedIncident = clickedIncident == selectedIncident ? -1 : clickedIncident;
                    selectedVehicleMask = 0;
                } else if (cityPoint.x >= 0 && cityPoint.x < CITY_WIDTH &&
                           cityPoint.y >= 0 && cityPoint.y < CITY_HEIGHT) {
                    selectedIncident = -1;
                    selectedVehicleMask = 0;
                    if (SimulationCreateIncident(&simulation, (IncidentType)selectedType, cityPoint))
                        selectedIncident = simulation.incidentCount - 1;
                }

            }

        }



        if (stepRequested) {
            // Manual stepping always advances by the same small time slice,
            // then pauses so the result can be inspected.
            paused = true;
            SimulationUpdate(&simulation, 0.1f);
        } else if (!paused) {
            float speedMultiplier = fastForward ? 8.0f : slowMotion ? 0.25f : 1.0f;
            SimulationUpdate(&simulation, GetFrameTime() * speedMultiplier);
        }



        BeginTextureMode(uiCanvas);
        ClearBackground((Color){7, 17, 29, 255});
        BeginMode2D(renderCamera);

        DrawRectangle(0, 0, UI_WIDTH, UI_HEIGHT, (Color){7, 17, 29, 255});
        DrawRectangle(0, 0, UI_WIDTH, 88, (Color){11, 29, 46, 255});
        DrawText("EMERGENCY CITY SIMULATION", 28, 17, 25, RAYWHITE);
        DrawText("CONTROL ROOM  /  LIVE DISPATCH", 30, 52, 12,
                 (Color){124, 190, 222, 255});
        DrawText(TextFormat("CALLS %d   ACTIVE %d   RESOLVED %d   FAILED %d",
                 simulation.incidentCount, SimulationCountUnresolved(&simulation),
                 SimulationCountStatus(&simulation, INCIDENT_RESOLVED),
                 SimulationCountStatus(&simulation, INCIDENT_FAILED)),
                 920, 32, 13, (Color){177, 198, 211, 255});

        DrawIncidentQueue(&simulation, queueOrder, queueCount, selectedIncident);
        DrawPanel(mapPanel);
        DrawText("CITY MAP", 320, 119, 16, RAYWHITE);
        DrawText("Click map to create or inspect a call", 420, 122, 11,
                 (Color){139, 164, 179, 255});
        DrawText("FIRE", 940, 121, 10, RED);
        DrawText("POLICE", 986, 121, 10, BLUE);
        DrawText("AMBULANCE", 1052, 121, 10, DARKGREEN);
        DrawRectangleRounded(mapClip, 0.02f, 8, (Color){14, 27, 37, 255});

        // The map uses its own camera and draws directly to the 2x render
        // texture. End the UI camera before that pass, then restore it for the
        // details panel and controls so they retain the correct size/position.
        EndMode2D();
        DrawMapContents(&simulation, selectedIncident, cityView);

        BeginMode2D(renderCamera);
        DrawIncidentDetails(&simulation, selectedIncident, comparisonReady, &comparison,
                            selectedVehicleMask);

        DrawPanel((Rectangle){18, 818, 1564, 68});
        DrawText("SIMULATION CONTROLS", 34, 833, 13,
                 (Color){132, 194, 225, 255});
        DrawText("1-4 type | SPACE pause | S slow | F fast | N step | T traffic | R reset",
                 34, 858, 10, (Color){158, 180, 193, 255});
        DrawButton(compareButton, "COMPARE", BLUE);
        DrawButton(pauseButton, paused ? "RESUME" : "PAUSE", paused ? DARKGREEN : DARKGRAY);
        DrawButton(slowButton, slowMotion ? "SLOW x0.25 ON" : "SLOW x0.25",
                   slowMotion ? DARKBLUE : DARKGRAY);
        DrawButton(normalButton, (!slowMotion && !fastForward) ? "NORMAL x1 ON" : "NORMAL x1",
                   (!slowMotion && !fastForward) ? DARKGREEN : DARKGRAY);
        DrawButton(fastButton, fastForward ? "FAST x8 ON" : "FAST x8",
                   fastForward ? DARKGREEN : DARKGRAY);
        DrawButton(stepButton, "STEP 0.1", (Color){78, 91, 110, 255});
        DrawButton(trafficButton,
                   TextFormat("TRAF %s", SimulationTrafficLevelName(simulation.trafficLevel)),
                   simulation.trafficLevel == TRAFFIC_HEAVY ? ORANGE :
                   simulation.trafficLevel == TRAFFIC_MEDIUM ? (Color){176, 122, 48, 255} :
                   DARKGRAY);
        DrawButton(simulateButton, "SIM QUEUE", BLUE);
        DrawButton(resetButton, "RESET", MAROON);

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
    CloseWindow();

    return 0;

}
