#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// --- Configuration ---
#define GRID_WIDTH      20
#define GRID_HEIGHT     15
#define CELL_SIZE       40
#define SCREEN_WIDTH    (GRID_WIDTH * CELL_SIZE)
#define SCREEN_HEIGHT   ((GRID_HEIGHT * CELL_SIZE) + 110)

#define MAX_PATH_LENGTH 120
#define MAX_UNITS       10
#define MAX_INCIDENTS   12

// --- Enums & Structs ---
typedef struct { int x, y; } Vector2Int;

typedef enum { TYPE_FIRE, TYPE_MEDICAL, TYPE_CRIME, TYPE_COUNT } IncidentType;
typedef enum { STATE_IDLE, STATE_MOVING, STATE_RESOLVING } UnitState;
typedef enum { UNIT_FIRE, UNIT_AMBULANCE, UNIT_POLICE, UNIT_COUNT } UnitType;

typedef struct {
    Vector2Int pos;
    Vector2Int target;
    UnitState  state;
    UnitType   type;
    float      moveTimer;
    float      resolveTimer;
    Vector2Int path[MAX_PATH_LENGTH];
    int        pathLength;
    int        pathIndex;
} EmergencyUnit;

typedef struct {
    Vector2Int    pos;
    IncidentType  type;
    bool          active;
    float         timeActive;
    float         maxTime;
} Incident;

typedef struct {
    Vector2Int pos;
    int g, h, f;
    bool isClosed;
    Vector2Int parent;
} Node;

// --- Globals ---
bool gridObstacles[GRID_WIDTH][GRID_HEIGHT] = { 0 };
EmergencyUnit units[MAX_UNITS];
int unitCount = 0;
Incident incidents[MAX_INCIDENTS];
int incidentCount = 0;

float gameTime = 0.0f;

// Simulation control
bool  paused = false;
float simulationSpeed = 1.0f;

// Statistics
int   incidentsResolved = 0;
float totalResponseTime = 0.0f;

Color unitColors[UNIT_COUNT];
Color incidentColors[TYPE_COUNT];

// --- Utility ---
Vector2Int GetRandomEmptyCell(void) {
    Vector2Int cell;
    do {
        cell.x = GetRandomValue(0, GRID_WIDTH - 1);
        cell.y = GetRandomValue(0, GRID_HEIGHT - 1);
    } while (gridObstacles[cell.x][cell.y]);
    return cell;
}

Vector2 GetCellCenter(Vector2Int cell) {
    return (Vector2) {
        cell.x* CELL_SIZE + CELL_SIZE / 2.0f,
            cell.y* CELL_SIZE + CELL_SIZE / 2.0f
    };
}

void InitColors(void) {
    unitColors[UNIT_FIRE] = RED;
    unitColors[UNIT_AMBULANCE] = SKYBLUE;
    unitColors[UNIT_POLICE] = DARKBLUE;

    incidentColors[TYPE_FIRE] = ORANGE;
    incidentColors[TYPE_MEDICAL] = LIME;
    incidentColors[TYPE_CRIME] = PURPLE;
}

// --- A* Pathfinding ---
bool FindPath(Vector2Int start, Vector2Int end, Vector2Int* outPath, int* outLength) {
    if (start.x == end.x && start.y == end.y) return false;
    if (gridObstacles[end.x][end.y]) return false;

    Node nodes[GRID_WIDTH][GRID_HEIGHT];
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            nodes[x][y].pos = (Vector2Int){ x, y };
            nodes[x][y].g = 999999;
            nodes[x][y].h = abs(x - end.x) + abs(y - end.y);
            nodes[x][y].f = 999999;
            nodes[x][y].isClosed = false;
            nodes[x][y].parent = (Vector2Int){ -1, -1 };
        }
    }

    nodes[start.x][start.y].g = 0;
    nodes[start.x][start.y].f = nodes[start.x][start.y].h;

    Vector2Int openList[GRID_WIDTH * GRID_HEIGHT];
    int openCount = 0;
    openList[openCount++] = start;

    const int dx[] = { 0, 0, 1, -1 };
    const int dy[] = { 1, -1, 0, 0 };

    while (openCount > 0) {
        int lowest = 0;
        for (int i = 1; i < openCount; i++) {
            if (nodes[openList[i].x][openList[i].y].f < nodes[openList[lowest].x][openList[lowest].y].f)
                lowest = i;
        }

        Vector2Int current = openList[lowest];
        openList[lowest] = openList[--openCount];
        nodes[current.x][current.y].isClosed = true;

        if (current.x == end.x && current.y == end.y) {
            int length = 0;
            Vector2Int temp[MAX_PATH_LENGTH];
            Vector2Int curr = current;
            while (curr.x != -1 && length < MAX_PATH_LENGTH) {
                temp[length++] = curr;
                curr = nodes[curr.x][curr.y].parent;
            }
            for (int i = 0; i < length; i++)
                outPath[i] = temp[length - 1 - i];
            *outLength = length;
            return true;
        }

        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            if (nx < 0 || nx >= GRID_WIDTH || ny < 0 || ny >= GRID_HEIGHT) continue;
            if (nodes[nx][ny].isClosed || gridObstacles[nx][ny]) continue;

            int tentativeG = nodes[current.x][current.y].g + 1;
            if (tentativeG < nodes[nx][ny].g) {
                nodes[nx][ny].parent = current;
                nodes[nx][ny].g = tentativeG;
                nodes[nx][ny].f = tentativeG + nodes[nx][ny].h;

                bool alreadyOpen = false;
                for (int j = 0; j < openCount; j++) {
                    if (openList[j].x == nx && openList[j].y == ny) {
                        alreadyOpen = true;
                        break;
                    }
                }
                if (!alreadyOpen) openList[openCount++] = (Vector2Int){ nx, ny };
            }
        }
    }
    return false;
}

// --- Simulation ---
void InitSimulation(void) {
    for (int x = 0; x < GRID_WIDTH; x++)
        for (int y = 0; y < GRID_HEIGHT; y++)
            gridObstacles[x][y] = false;

    unitCount = 0;
    incidentCount = 0;
    gameTime = 0.0f;
    incidentsResolved = 0;
    totalResponseTime = 0.0f;
    paused = false;
    simulationSpeed = 1.0f;

    // Place random buildings
    for (int i = 0; i < 45; i++) {
        int x = GetRandomValue(1, GRID_WIDTH - 2);
        int y = GetRandomValue(1, GRID_HEIGHT - 2);
        gridObstacles[x][y] = true;
    }
}

void SpawnUnit(UnitType type) {
    if (unitCount >= MAX_UNITS) return;
    units[unitCount++] = (EmergencyUnit){
        .pos = GetRandomEmptyCell(),
        .state = STATE_IDLE,
        .type = type,
        .pathLength = 0,
        .pathIndex = 0,
        .moveTimer = 0,
        .resolveTimer = 0
    };
}

void SpawnIncident(IncidentType type) {
    if (incidentCount >= MAX_INCIDENTS) return;

    float maxTime = (type == TYPE_FIRE) ? 18.0f :
        (type == TYPE_MEDICAL) ? 12.0f : 22.0f;

    incidents[incidentCount++] = (Incident){
        .pos = GetRandomEmptyCell(),
        .type = type,
        .active = true,
        .timeActive = 0.0f,
        .maxTime = maxTime
    };
}

void UpdateSimulation(void) {
    // Controls
    if (IsKeyPressed(KEY_SPACE)) paused = !paused;

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        simulationSpeed += 0.5f;
        if (simulationSpeed > 5.0f) simulationSpeed = 5.0f;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        simulationSpeed -= 0.5f;
        if (simulationSpeed < 0.5f) simulationSpeed = 0.5f;
    }

    if (paused) return;

    float dt = GetFrameTime() * simulationSpeed;
    gameTime += dt;

    // Auto spawn
    static float spawnTimer = 0.0f;
    spawnTimer += dt;
    if (spawnTimer >= 4.0f) {
        SpawnIncident((IncidentType)GetRandomValue(0, TYPE_COUNT - 1));
        spawnTimer = 0.0f;
    }

    // Keyboard spawn units
    if (IsKeyPressed(KEY_ONE))   SpawnUnit(UNIT_FIRE);
    if (IsKeyPressed(KEY_TWO))   SpawnUnit(UNIT_AMBULANCE);
    if (IsKeyPressed(KEY_THREE)) SpawnUnit(UNIT_POLICE);

    // Mouse spawn incidents
    Vector2 mouse = GetMousePosition();
    int gx = (int)(mouse.x / CELL_SIZE);
    int gy = (int)(mouse.y / CELL_SIZE);

    if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT && !gridObstacles[gx][gy]) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && incidentCount < MAX_INCIDENTS)
            incidents[incidentCount++] = (Incident){ {gx, gy}, TYPE_FIRE, true, 0, 18.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && incidentCount < MAX_INCIDENTS)
            incidents[incidentCount++] = (Incident){ {gx, gy}, TYPE_MEDICAL, true, 0, 12.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && incidentCount < MAX_INCIDENTS)
            incidents[incidentCount++] = (Incident){ {gx, gy}, TYPE_CRIME, true, 0, 22.0f };
    }

    // Update incidents time
    for (int i = 0; i < incidentCount; i++) {
        if (incidents[i].active) incidents[i].timeActive += dt;
    }

    // Update units
    for (int i = 0; i < unitCount; i++) {
        EmergencyUnit* u = &units[i];

        if (u->state == STATE_IDLE) {
            int best = -1;
            float bestDist = 999999.0f;

            for (int j = 0; j < incidentCount; j++) {
                if (!incidents[j].active) continue;

                bool match =
                    (u->type == UNIT_FIRE && incidents[j].type == TYPE_FIRE) ||
                    (u->type == UNIT_AMBULANCE && incidents[j].type == TYPE_MEDICAL) ||
                    (u->type == UNIT_POLICE && incidents[j].type == TYPE_CRIME);

                if (!match) continue;

                float dist = (float)(abs(u->pos.x - incidents[j].pos.x) + abs(u->pos.y - incidents[j].pos.y));
                if (dist < bestDist) {
                    bestDist = dist;
                    best = j;
                }
            }

            if (best != -1) {
                u->target = incidents[best].pos;
                if (FindPath(u->pos, u->target, u->path, &u->pathLength)) {
                    u->state = STATE_MOVING;
                    u->pathIndex = 0;
                }
            }
        }
        else if (u->state == STATE_MOVING) {
            u->moveTimer += dt;
            if (u->moveTimer >= 0.12f) {
                u->moveTimer = 0.0f;
                if (u->pathIndex < u->pathLength - 1) {
                    u->pathIndex++;
                    u->pos = u->path[u->pathIndex];
                }
                else {
                    u->state = STATE_RESOLVING;
                    u->resolveTimer = 0.0f;
                }
            }
        }
        else if (u->state == STATE_RESOLVING) {
            u->resolveTimer += dt;
            if (u->resolveTimer >= 1.8f) {
                for (int j = 0; j < incidentCount; j++) {
                    if (incidents[j].active &&
                        incidents[j].pos.x == u->pos.x &&
                        incidents[j].pos.y == u->pos.y) {

                        // Record statistics
                        incidentsResolved++;
                        totalResponseTime += incidents[j].timeActive;

                        incidents[j].active = false;
                        break;
                    }
                }
                u->state = STATE_IDLE;
                u->pathLength = 0;
            }
        }
    }
}

void DrawSimulation(void) {
    BeginDrawing();
    ClearBackground((Color) { 18, 18, 28, 255 });

    // Grid
    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_HEIGHT; y++) {
            Rectangle cell = { (float)(x * CELL_SIZE), (float)(y * CELL_SIZE), (float)CELL_SIZE, (float)CELL_SIZE };
            if (gridObstacles[x][y]) {
                DrawRectangleRec(cell, (Color) { 55, 55, 75, 255 });
            }
            else {
                DrawRectangleLinesEx(cell, 1.0f, (Color) { 35, 35, 55, 255 });
            }
        }
    }

    // Incidents
    for (int i = 0; i < incidentCount; i++) {
        if (!incidents[i].active) continue;

        Vector2 c = GetCellCenter(incidents[i].pos);
        DrawCircleV(c, 14.0f, incidentColors[incidents[i].type]);
        DrawCircleV(c, 7.0f, WHITE);

        const char* label = (incidents[i].type == TYPE_FIRE) ? "F" :
            (incidents[i].type == TYPE_MEDICAL) ? "M" : "C";
        DrawText(label, (int)c.x - 4, (int)c.y - 6, 12, BLACK);

        // Timer bar
        float ratio = 1.0f - (incidents[i].timeActive / incidents[i].maxTime);
        if (ratio < 0) ratio = 0;
        DrawRectangle((int)c.x - 16, (int)c.y - 22, (int)(32 * ratio), 5, RED);
        DrawRectangleLines((int)c.x - 16, (int)c.y - 22, 32, 5, BLACK);
    }

    // Units + paths
    for (int i = 0; i < unitCount; i++) {
        EmergencyUnit* u = &units[i];

        if (u->state == STATE_MOVING && u->pathLength > 1) {
            for (int j = u->pathIndex; j < u->pathLength - 1; j++) {
                Vector2 p1 = GetCellCenter(u->path[j]);
                Vector2 p2 = GetCellCenter(u->path[j + 1]);
                DrawLineEx(p1, p2, 3.0f, (Color) { 255, 255, 255, 90 });
            }
        }

        Vector2 c = GetCellCenter(u->pos);
        DrawCircleV(c, 15.0f, unitColors[u->type]);
        DrawCircleLines((int)c.x, (int)c.y, 15, BLACK);

        const char* label = (u->type == UNIT_FIRE) ? "F" :
            (u->type == UNIT_AMBULANCE) ? "A" : "P";
        DrawText(label, (int)c.x - 5, (int)c.y - 6, 14, BLACK);

        if (u->state == STATE_RESOLVING) {
            DrawCircleLines((int)c.x, (int)c.y, 20, GREEN);
        }
    }

    // ========== STATISTICS PANEL ==========
    DrawRectangle(0, SCREEN_HEIGHT - 110, SCREEN_WIDTH, 110, (Color) { 25, 25, 40, 255 });
    DrawLine(0, SCREEN_HEIGHT - 110, SCREEN_WIDTH, SCREEN_HEIGHT - 110, GRAY);

    // Left - Stats
    DrawText("SIMULATION STATISTICS", 20, SCREEN_HEIGHT - 100, 18, LIGHTGRAY);
    DrawText(TextFormat("Incidents Resolved: %d", incidentsResolved), 20, SCREEN_HEIGHT - 75, 18, RAYWHITE);

    int activeIncidents = 0;
    for (int i = 0; i < incidentCount; i++) if (incidents[i].active) activeIncidents++;
    DrawText(TextFormat("Active Incidents: %d", activeIncidents), 20, SCREEN_HEIGHT - 52, 18, RAYWHITE);
    DrawText(TextFormat("Active Units: %d", unitCount), 20, SCREEN_HEIGHT - 29, 18, RAYWHITE);

    // Middle
    float avgResponse = (incidentsResolved > 0) ? (totalResponseTime / incidentsResolved) : 0.0f;
    DrawText(TextFormat("Avg Response Time: %.1fs", avgResponse), 280, SCREEN_HEIGHT - 75, 18, RAYWHITE);
    DrawText(TextFormat("Simulation Time: %.1fs", gameTime), 280, SCREEN_HEIGHT - 52, 18, LIGHTGRAY);

    // Right - Controls
    DrawText("CONTROLS", 580, SCREEN_HEIGHT - 100, 18, LIGHTGRAY);
    DrawText(TextFormat("Speed: %.1fx", simulationSpeed), 580, SCREEN_HEIGHT - 75, 18, RAYWHITE);
    DrawText(paused ? "PAUSED" : "RUNNING", 580, SCREEN_HEIGHT - 52, 18, paused ? ORANGE : LIME);
    DrawText("[Space] Pause   [+] Faster   [-] Slower", 580, SCREEN_HEIGHT - 29, 16, GRAY);

    EndDrawing();
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Emergency Response Simulator");
    SetTargetFPS(60);
    SetRandomSeed((unsigned int)time(NULL));

    InitColors();
    InitSimulation();

    while (!WindowShouldClose()) {
        UpdateSimulation();
        DrawSimulation();
    }

    CloseWindow();
    return 0;
}