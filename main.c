#include "raylib.h"
#include "incidents.h"
#include "simulation.h"
#include <stdio.h>

#define SCREEN_WIDTH 1100
#define SCREEN_HEIGHT 700

static Rectangle fastButton = {710, 640, 105, 42};
static Rectangle simulateButton = {825, 640, 145, 42};
static Rectangle resetButton = {980, 640, 100, 42};

static void DrawCity(void)
{
    ClearBackground((Color){238, 242, 237, 255});

    // Simplified roads
    DrawRectangle(0, 290, SCREEN_WIDTH, 72, (Color){190, 196, 198, 255});
    DrawRectangle(355, 80, 82, 550, (Color){190, 196, 198, 255});
    DrawRectangle(730, 80, 82, 550, (Color){190, 196, 198, 255});

    for (int x = 0; x < SCREEN_WIDTH; x += 55)
        DrawRectangle(x, 324, 30, 4, RAYWHITE);
    for (int y = 90; y < 630; y += 55) {
        DrawRectangle(394, y, 4, 30, RAYWHITE);
        DrawRectangle(769, y, 4, 30, RAYWHITE);
    }

    DrawRectangle(70, 105, 210, 130, (Color){210, 218, 223, 255});
    DrawRectangle(490, 105, 175, 130, (Color){210, 218, 223, 255});
    DrawRectangle(865, 105, 175, 130, (Color){210, 218, 223, 255});
    DrawRectangle(70, 410, 210, 155, (Color){210, 218, 223, 255});
    DrawRectangle(490, 410, 175, 155, (Color){210, 218, 223, 255});
    DrawRectangle(865, 410, 175, 155, (Color){210, 218, 223, 255});

    DrawText("BUILDING A", 112, 160, 18, DARKGRAY);
    DrawText("BUILDING B", 520, 160, 18, DARKGRAY);
    DrawText("BUILDING C", 900, 160, 18, DARKGRAY);
    DrawText("BUILDING D", 112, 465, 18, DARKGRAY);
    DrawText("BUILDING E", 520, 465, 18, DARKGRAY);
    DrawText("BUILDING F", 900, 465, 18, DARKGRAY);

    // Three fire engines are grouped at the fire station.
    DrawCircle(300, 270, 10, BLUE);
    DrawCircle(330, 270, 10, BLUE);
    DrawCircle(315, 245, 10, BLUE);
    DrawText("FIRE x3", 287, 218, 14, DARKBLUE);
    DrawCircle(660, 380, 10, DARKGREEN);
    DrawText("POLICE", 625, 354, 14, DARKGREEN);
    DrawCircle(790, 270, 10, MAROON);
    DrawText("MEDICAL", 758, 245, 14, MAROON);
}

static const char *IncidentTypeName(IncidentType type)
{
    switch (type) {
        case INCIDENT_FIRE: return "Fire";
        case INCIDENT_CRIME: return "Crime";
        case INCIDENT_ACCIDENT: return "Traffic accident";
        default: return "Unknown";
    }
}

static const char *IncidentStatusName(IncidentStatus status)
{
    switch (status) {
        case INCIDENT_PENDING: return "Pending";
        case INCIDENT_DISPATCHED: return "Travelling";
        case INCIDENT_RESPONDING: return "Responding";
        case INCIDENT_RESOLVED: return "Resolved";
        default: return "Unknown";
    }
}

static void DrawButton(Rectangle rect, const char *label, Color color)
{
    DrawRectangleRec(rect, color);
    DrawRectangleLinesEx(rect, 1.0f, RAYWHITE);
    int textWidth = MeasureText(label, 14);
    DrawText(label, (int)(rect.x + (rect.width - textWidth) / 2),
             (int)(rect.y + 13), 14, RAYWHITE);
}

static void DrawServiceStats(const Simulation *simulation, int y,
                             IncidentType type, const char *label, Color color)
{
    int active = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (incident->type == type && incident->status != INCIDENT_RESOLVED) active++;
    }

    int resolved = simulation->resolvedByType[type];
    float average = resolved > 0
        ? simulation->totalResponseTimeByType[type] / (float)resolved
        : 0.0f;

    DrawText(label, 850, y, 14, color);
    DrawText(TextFormat("Sent %d | Done %d", simulation->dispatchedByType[type], resolved),
             850, y + 17, 12, DARKGRAY);
    DrawText(TextFormat("Active %d | Units free %d",
             active, SimulationCountAvailableVehicles(simulation, type)),
             850, y + 32, 12, DARKGRAY);
    DrawText(TextFormat("Avg arrival %.1fs", average), 850, y + 47, 12, DARKGRAY);
}

int main(void)
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "adv_emergency | Emergency City Simulation");
    SetTargetFPS(60);

    Simulation simulation;
    SimulationInit(&simulation);

    int selectedType = INCIDENT_FIRE;
    bool paused = false;
    bool fastForward = false;

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ONE)) selectedType = INCIDENT_FIRE;
        if (IsKeyPressed(KEY_TWO)) selectedType = INCIDENT_CRIME;
        if (IsKeyPressed(KEY_THREE)) selectedType = INCIDENT_ACCIDENT;
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_F)) fastForward = !fastForward;
        if (IsKeyPressed(KEY_M)) SimulationSimulateQueue(&simulation);
        if (IsKeyPressed(KEY_R)) SimulationInit(&simulation);

        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (CheckCollisionPointRec(mouse, fastButton)) {
                fastForward = !fastForward;
            } else if (CheckCollisionPointRec(mouse, simulateButton)) {
                SimulationSimulateQueue(&simulation);
            } else if (CheckCollisionPointRec(mouse, resetButton)) {
                SimulationInit(&simulation);
                paused = false;
                fastForward = false;
            } else if (mouse.y >= 70 && mouse.y <= 630 && mouse.x < 825) {
                SimulationCreateIncident(&simulation, (IncidentType)selectedType, mouse);
            }
        }

        if (!paused) {
            float speedMultiplier = fastForward ? 8.0f : 1.0f;
            SimulationUpdate(&simulation, GetFrameTime() * speedMultiplier);
        }

        BeginDrawing();
        DrawCity();

        DrawRectangle(0, 0, SCREEN_WIDTH, 70, (Color){30, 42, 55, 255});
        DrawText("EMERGENCY CITY SIMULATION", 20, 12, 25, RAYWHITE);
        DrawText("Operational monitoring prototype | C + Raylib", 22, 42, 14, (Color){205, 215, 222, 255});

        DrawRectangle(0, 630, SCREEN_WIDTH, 70, (Color){30, 42, 55, 255});
        DrawText("1 Fire | 2 Crime | 3 Accident | Click map to create", 18, 642, 15, RAYWHITE);
        DrawText("SPACE pause | F fast-forward | M simulate queue | R reset", 18, 665, 12, (Color){205, 215, 222, 255});
        DrawButton(fastButton, fastForward ? "FAST x8: ON" : "FAST x8", fastForward ? DARKGREEN : DARKGRAY);
        DrawButton(simulateButton, "SIMULATE QUEUE", BLUE);
        DrawButton(resetButton, "RESET", MAROON);

        DrawRectangle(835, 80, 245, 540, (Color){250, 251, 252, 245});
        DrawRectangleLines(835, 80, 245, 540, GRAY);
        DrawText("SIMULATION STATUS", 850, 94, 17, DARKBLUE);
        DrawText(TextFormat("Total created: %d", simulation.incidentCount), 850, 122, 15, DARKGRAY);
        DrawText(TextFormat("Resolved: %d", SimulationCountStatus(&simulation, INCIDENT_RESOLVED)), 850, 144, 15, DARKGREEN);
        DrawText(TextFormat("Waiting: %d", SimulationCountStatus(&simulation, INCIDENT_PENDING)), 850, 166, 15, ORANGE);
        DrawText(TextFormat("In progress: %d", SimulationCountInProgress(&simulation)), 850, 188, 15, MAROON);
        DrawText(paused ? "PAUSED" : (fastForward ? "RUNNING x8" : "RUNNING x1"),
                 850, 211, 15, paused ? RED : DARKGREEN);

        DrawLine(850, 238, 1065, 238, LIGHTGRAY);
        DrawText("SERVICE RESPONSES", 850, 247, 16, DARKBLUE);
        DrawServiceStats(&simulation, 272, INCIDENT_FIRE, "FIRE SERVICE (3 units)", BLUE);
        DrawServiceStats(&simulation, 338, INCIDENT_CRIME, "POLICE SERVICE", DARKGREEN);
        DrawServiceStats(&simulation, 404, INCIDENT_ACCIDENT, "AMBULANCE SERVICE", MAROON);

        DrawLine(850, 470, 1065, 470, LIGHTGRAY);
        DrawText("RECENT INCIDENTS", 850, 480, 15, DARKBLUE);
        int visible = simulation.incidentCount < 6 ? simulation.incidentCount : 6;
        for (int i = 0; i < visible; i++) {
            Incident *incident = &simulation.incidents[simulation.incidentCount - visible + i];
            int y = 505 + i * 18;
            Color c = incident->status == INCIDENT_RESOLVED ? DARKGREEN :
                      incident->status == INCIDENT_PENDING ? ORANGE : MAROON;
            DrawText(TextFormat("%s: %s", IncidentTypeName(incident->type),
                     IncidentStatusName(incident->status)), 850, y, 11, c);
        }

        // Draw responding units. Their positions update as the simulation advances.
        for (int i = 0; i < simulation.vehicleCount; i++) {
            Vehicle *vehicle = &simulation.vehicles[i];
            Color c = vehicle->serviceType == INCIDENT_FIRE ? BLUE :
                      vehicle->serviceType == INCIDENT_CRIME ? DARKGREEN : MAROON;
            DrawCircleV(vehicle->position, 7, c);
            DrawCircleLines((int)vehicle->position.x, (int)vehicle->position.y, 9, RAYWHITE);
        }

        // Incident markers are drawn on top of the city map.
        for (int i = 0; i < simulation.incidentCount; i++) {
            Incident *incident = &simulation.incidents[i];
            Color c = incident->status == INCIDENT_RESOLVED ? DARKGREEN :
                      incident->status == INCIDENT_PENDING ? ORANGE : RED;
            DrawCircleV(incident->position, 5, c);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
