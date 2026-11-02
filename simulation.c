#include "simulation.h"
#include "city_layout.h"

#include <math.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ROAD_NODE_COUNT CITY_LAYOUT_ROAD_NODE_COUNT
#define ROAD_EDGE_COUNT CITY_LAYOUT_ROAD_EDGE_COUNT

static int TrafficCarCount(TrafficLevel level)
{
    if (level == TRAFFIC_MEDIUM) return 14;
    if (level == TRAFFIC_HEAVY) return MAX_CIVILIAN_CARS;
    return 6;
}

const char *SimulationTrafficLevelName(TrafficLevel level)
{
    switch (level) {
        case TRAFFIC_LIGHT: return "LIGHT";
        case TRAFFIC_MEDIUM: return "MEDIUM";
        case TRAFFIC_HEAVY: return "HEAVY";
        default: return "LIGHT";
    }
}

const char *SimulationCityConditionName(CityCondition condition)
{
    switch (condition) {
        case CITY_CLEAR: return "CLEAR";
        case CITY_RAIN: return "RAIN";
        case CITY_FOG: return "FOG";
        case CITY_WIND: return "WIND";
        case CITY_NIGHT: return "NIGHT";
        case CITY_SNOW: return "SNOW";
        default: return "CLEAR";
    }
}

static void LogEvent(Simulation *simulation, const char *message)
{
    if (!simulation || !message) return;
    if (simulation->eventCount == MAX_SIMULATION_EVENTS) {
        memmove(&simulation->events[0], &simulation->events[1],
                sizeof(simulation->events[0]) * (MAX_SIMULATION_EVENTS - 1));
        simulation->eventCount--;
    }
    SimulationEvent *event = &simulation->events[simulation->eventCount++];
    event->time = simulation->elapsedTime;
    snprintf(event->message, sizeof(event->message), "%s", message);
}

static unsigned int AutoIncidentRandomNext(Simulation *simulation)
{
    // Xorshift32 keeps this feature reproducible without sharing rand() state.
    unsigned int value = simulation->autoIncidentRandomState;
    if (value == 0u) value = AUTO_INCIDENT_DEFAULT_SEED;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    simulation->autoIncidentRandomState = value;
    return value;
}

static float AutoIncidentRandomInterval(Simulation *simulation)
{
    return 5.0f + (float)(AutoIncidentRandomNext(simulation) % 26u);
}

void SimulationSetAutoIncidents(Simulation *simulation, int enabled, unsigned int seed)
{
    if (!simulation) return;
    int turnOn = enabled != 0;
    if (turnOn == simulation->autoIncidentsEnabled) {
        // Also allow callers and regression checks to restart the sequence with
        // a known seed while generation is already enabled.
        if (turnOn && seed != 0u) {
            simulation->autoIncidentRandomState = seed;
            simulation->autoIncidentNextAt = simulation->elapsedTime +
                                             AutoIncidentRandomInterval(simulation);
        }
        return;
    }
    simulation->autoIncidentsEnabled = turnOn;
    simulation->autoIncidentCapacitySuspended = 0;
    if (turnOn) {
        simulation->autoIncidentRandomState = seed ? seed : AUTO_INCIDENT_DEFAULT_SEED;
        simulation->autoIncidentNextAt = simulation->elapsedTime +
                                         AutoIncidentRandomInterval(simulation);
        LogEvent(simulation, "Automatic incident generation enabled.");
    } else {
        simulation->autoIncidentNextAt = 0.0f;
        LogEvent(simulation, "Automatic incident generation disabled.");
    }
}

float SimulationAutoIncidentSecondsUntilNext(const Simulation *simulation)
{
    if (!simulation || !simulation->autoIncidentsEnabled ||
        simulation->autoIncidentCapacitySuspended) return -1.0f;
    float remaining = simulation->autoIncidentNextAt - simulation->elapsedTime;
    return remaining > 0.0f ? remaining : 0.0f;
}

static void GenerateAutoIncidentIfDue(Simulation *simulation)
{
    if (!simulation || !simulation->autoIncidentsEnabled ||
        simulation->autoIncidentCapacitySuspended ||
        simulation->elapsedTime < simulation->autoIncidentNextAt) return;

    // Calls become incidents only after classification, so protect both fixed
    // arrays before creating a new queued report.
    if (simulation->callCount >= MAX_CALLS ||
        simulation->incidentCount >= MAX_INCIDENTS) {
        simulation->autoIncidentCapacitySuspended = 1;
        LogEvent(simulation, "Automatic incident generation paused: call or incident capacity is full.");
        return;
    }

    static const IncidentType types[] = {
        INCIDENT_FIRE, INCIDENT_CRIME, INCIDENT_ACCIDENT, INCIDENT_MEDICAL
    };
    static const char *reports[4][3] = {
        {"Caller reports a small fire in a waste bin near the street.",
         "Caller reports smoke from a building; people may be inside.",
         "Caller reports flames spreading and people trapped inside."},
        {"Caller reports a suspicious person near a closed shop.",
         "Caller reports a break-in in progress at a shop.",
         "Caller reports an assault with a weapon in progress."},
        {"Caller reports a minor vehicle collision; traffic is moving.",
         "Caller reports two vehicles collided and one lane is blocked.",
         "Caller reports a multiple-vehicle collision with serious injuries."},
        {"Caller reports a person feeling unwell and asking for medical help.",
         "Caller reports an injured person who needs urgent medical help.",
         "Caller reports a person unresponsive and not breathing."}
    };

    int typeIndex = (int)(AutoIncidentRandomNext(simulation) % 4u);
    int severity = 1 + (int)(AutoIncidentRandomNext(simulation) % 3u);
    int buildingIndex = (int)(AutoIncidentRandomNext(simulation) % CITY_LAYOUT_BUILDING_COUNT);
    Rectangle building = CITY_LAYOUT_BUILDINGS[buildingIndex];
    Vector2 location = {building.x + building.width * 0.5f,
                        building.y + building.height * 0.5f};
    if (!SimulationCreateCall(simulation, location,
                              reports[typeIndex][severity - 1])) {
        simulation->autoIncidentCapacitySuspended = 1;
        LogEvent(simulation, "Automatic incident generation paused: call queue is full.");
        return;
    }

    int callIndex = simulation->callCount - 1;
    SimulationSetCallTruth(simulation, callIndex, types[typeIndex], severity);
    EmergencyCall *call = &simulation->calls[callIndex];
    if (simulation->dispatchMode == DISPATCH_AUTOMATIC)
        call->overridePending = 1;
    char event[SIMULATION_EVENT_LENGTH];
    snprintf(event, sizeof(event), "Automatic %s call generated at building %d.",
             types[typeIndex] == INCIDENT_FIRE ? "fire" :
             types[typeIndex] == INCIDENT_CRIME ? "police" :
             types[typeIndex] == INCIDENT_ACCIDENT ? "traffic accident" : "medical",
             buildingIndex + 1);
    LogEvent(simulation, event);

    // Schedule from current simulation time. A large update produces at most
    // one call and never attempts to catch up with a burst.
    simulation->autoIncidentNextAt = simulation->elapsedTime +
                                     AutoIncidentRandomInterval(simulation);
}

void SimulationSetCityCondition(Simulation *simulation, CityCondition condition)
{
    if (!simulation) return;
    if (condition < CITY_CLEAR || condition >= CITY_CONDITION_COUNT) condition = CITY_CLEAR;
    if (simulation->cityCondition == condition) return;
    simulation->cityCondition = condition;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "City conditions changed to %s.",
             SimulationCityConditionName(condition));
    LogEvent(simulation, message);
}

static const char *ServiceEventName(IncidentType type)
{
    IncidentType service = SimulationServiceForIncident(type);
    return service == INCIDENT_FIRE ? "Fire" :
           service == INCIDENT_CRIME ? "Police" : "Ambulance";
}

static int ServiceUnitNumber(const Simulation *simulation, int vehicleIndex)
{
    int number = 0;
    for (int i = 0; i <= vehicleIndex; i++)
        if (simulation->vehicles[i].serviceType == simulation->vehicles[vehicleIndex].serviceType)
            number++;
    return number;
}

typedef CityRoadEdge RoadEdge;
#define roadNodes CITY_LAYOUT_ROAD_NODES
#define roadEdges CITY_LAYOUT_ROAD_EDGES



static float DistanceBetween(Vector2 a, Vector2 b)

{

    float dx = b.x - a.x;

    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);

}

static int RoadEdgeForNodes(int fromNode, int toNode)
{
    for (int i = 0; i < ROAD_EDGE_COUNT; i++)
        if ((roadEdges[i].from == fromNode && roadEdges[i].to == toNode) ||
            (roadEdges[i].to == fromNode && roadEdges[i].from == toNode)) return i;
    return -1;
}

static Vector2 CivilianLanePosition(int fromNode, int toNode, float progress,
                                    float shoulderOffset)
{
    Vector2 from = roadNodes[fromNode];
    Vector2 to = roadNodes[toNode];
    float length = DistanceBetween(from, to);
    float dx = (to.x - from.x) / length;
    float dy = (to.y - from.y) / length;
    // One lane per direction. The right-hand normal places opposing traffic
    // on opposite sides of the road automatically when direction is reversed.
    int edgeIndex = RoadEdgeForNodes(fromNode, toNode);
    float halfRoad = CityLayoutRoadWidth(edgeIndex) * 0.5f;
    float laneOffset = fminf(15.0f, halfRoad - 8.0f);
    if (laneOffset < 0.0f) laneOffset = 0.0f;
    float maxShoulderOffset = fmaxf(0.0f, halfRoad - laneOffset - 7.0f);
    float offset = laneOffset + fminf(shoulderOffset, maxShoulderOffset);
    return (Vector2){from.x + (to.x - from.x) * progress - dy * offset,
                     from.y + (to.y - from.y) * progress + dx * offset};
}

static Vector2 ClosestPointOnSegment(Vector2 point, Vector2 a, Vector2 b)
{
    Vector2 ab = {b.x - a.x, b.y - a.y};
    float lengthSquared = ab.x * ab.x + ab.y * ab.y;
    if (lengthSquared <= 0.0f) return a;

    float t = ((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / lengthSquared;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Vector2){a.x + t * ab.x, a.y + t * ab.y};
}

static int FindNearestRoadEdge(Vector2 point, Vector2 *projection)
{
    int bestEdge = 0;
    float bestDistance = 1.0e30f;

    for (int i = 0; i < ROAD_EDGE_COUNT; i++) {
        Vector2 candidate = ClosestPointOnSegment(point,
            roadNodes[roadEdges[i].from], roadNodes[roadEdges[i].to]);
        float distance = DistanceBetween(point, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestEdge = i;
            if (projection) *projection = candidate;
        }
    }
    return bestEdge;
}

static void FindRoadPaths(float distances[ROAD_NODE_COUNT][ROAD_NODE_COUNT],
                          int next[ROAD_NODE_COUNT][ROAD_NODE_COUNT])
{
    const float infinity = 1.0e30f;
    for (int i = 0; i < ROAD_NODE_COUNT; i++) {
        for (int j = 0; j < ROAD_NODE_COUNT; j++) {
            distances[i][j] = i == j ? 0.0f : infinity;
            next[i][j] = i == j ? j : -1;
        }
    }

    for (int i = 0; i < ROAD_EDGE_COUNT; i++) {
        int a = roadEdges[i].from;
        int b = roadEdges[i].to;
        distances[a][b] = DistanceBetween(roadNodes[a], roadNodes[b]);
        distances[b][a] = distances[a][b];
        next[a][b] = b;
        next[b][a] = a;
    }

    for (int k = 0; k < ROAD_NODE_COUNT; k++) {
        for (int i = 0; i < ROAD_NODE_COUNT; i++) {
            for (int j = 0; j < ROAD_NODE_COUNT; j++) {
                float candidate = distances[i][k] + distances[k][j];
                if (candidate < distances[i][j]) {
                    distances[i][j] = candidate;
                    next[i][j] = next[i][k];
                }
            }
        }
    }
}

static void AddRoutePoint(Vehicle *vehicle, Vector2 point)
{
    if (vehicle->routeCount > 0 &&
        DistanceBetween(vehicle->route[vehicle->routeCount - 1], point) < 0.1f) return;
    if (vehicle->routeCount < MAX_ROUTE_POINTS)
        vehicle->route[vehicle->routeCount++] = point;
}

static void BuildVehicleRoute(Vehicle *vehicle, Vector2 destination)
{
    vehicle->routeCount = 0;
    vehicle->routeIndex = 1;

    Vector2 startProjection;
    Vector2 endProjection;
    int startEdge = FindNearestRoadEdge(vehicle->position, &startProjection);
    int endEdge = FindNearestRoadEdge(destination, &endProjection);
    AddRoutePoint(vehicle, vehicle->position);

    if (startEdge == endEdge) {
        AddRoutePoint(vehicle, startProjection);
        AddRoutePoint(vehicle, endProjection);
        AddRoutePoint(vehicle, destination);
        return;
    }

    float distances[ROAD_NODE_COUNT][ROAD_NODE_COUNT];
    int next[ROAD_NODE_COUNT][ROAD_NODE_COUNT];
    FindRoadPaths(distances, next);

    int startEnds[2] = {roadEdges[startEdge].from, roadEdges[startEdge].to};
    int endEnds[2] = {roadEdges[endEdge].from, roadEdges[endEdge].to};
    int bestStart = startEnds[0];
    int bestEnd = endEnds[0];
    float bestDistance = 1.0e30f;

    for (int s = 0; s < 2; s++) {
        for (int e = 0; e < 2; e++) {
            int from = startEnds[s];
            int to = endEnds[e];
            float distance = DistanceBetween(startProjection, roadNodes[from]) +
                             distances[from][to] +
                             DistanceBetween(roadNodes[to], endProjection);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestStart = from;
                bestEnd = to;
            }
        }
    }

    AddRoutePoint(vehicle, startProjection);
    AddRoutePoint(vehicle, roadNodes[bestStart]);
    int node = bestStart;
    while (node != bestEnd && next[node][bestEnd] >= 0) {
        node = next[node][bestEnd];
        AddRoutePoint(vehicle, roadNodes[node]);
    }
    AddRoutePoint(vehicle, endProjection);
    AddRoutePoint(vehicle, destination);
}

static int SegmentDirection(Vector2 from, Vector2 to, int *edgeOut, float *startProgress)
{
    Vector2 midpoint = {(from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f};
    int edge = FindNearestRoadEdge(midpoint, NULL);
    if (edgeOut) *edgeOut = edge;
    if (edge < 0) return 0;
    Vector2 a = roadNodes[roadEdges[edge].from];
    Vector2 b = roadNodes[roadEdges[edge].to];
    Vector2 axis = {b.x - a.x, b.y - a.y};
    float length2 = axis.x * axis.x + axis.y * axis.y;
    float direction = (to.x - from.x) * axis.x + (to.y - from.y) * axis.y;
    if (startProgress && length2 > 0.0f) {
        *startProgress = ((from.x - a.x) * axis.x + (from.y - a.y) * axis.y) / length2;
        if (*startProgress < 0.0f) *startProgress = 0.0f;
        if (*startProgress > 1.0f) *startProgress = 1.0f;
    }
    return direction >= 0.0f ? 1 : -1;
}

static float SegmentTrafficDelay(const Simulation *simulation, const Vehicle *vehicle,
                                Vector2 from, Vector2 to)
{
    int edge;
    float routeStart = 0.0f;
    int direction = SegmentDirection(from, to, &edge, &routeStart);
    if (edge < 0) return 0.0f;
    RoadEdge road = roadEdges[edge];
    int fromNode = direction > 0 ? road.from : road.to;
    int toNode = direction > 0 ? road.to : road.from;
    float edgeLength = DistanceBetween(roadNodes[fromNode], roadNodes[toNode]);
    float maxDelay = 0.0f;
    for (int i = 0; i < simulation->civilianCount; i++) {
        const CivilianCar *car = &simulation->civilianCars[i];
        if (car->fromNode != fromNode || car->toNode != toNode || car->yieldTime > 0.0f ||
            car->progress <= routeStart) continue;
        float gap = (car->progress - routeStart) * edgeLength;
        float closingSpeed = vehicle->speed - car->currentSpeed;
        if (closingSpeed <= 0.1f) continue;
        float catchTime = gap / closingSpeed;
        float reaction = car->sirenSource >= 0 && car->sirenReactionTimer > 0.0f ?
                         car->sirenReactionTimer : car->sirenReactionDelay;
        float delay = reaction - catchTime;
        if (delay > maxDelay) maxDelay = delay;
    }
    return maxDelay > 0.0f ? maxDelay : 0.0f;
}

static float EstimateRouteETA(const Simulation *simulation, const Vehicle *vehicle,
                              int startIndex)
{
    float eta = 0.0f;
    Vector2 from = vehicle->position;
    for (int i = startIndex; i < vehicle->routeCount; i++) {
        Vector2 to = vehicle->route[i];
        eta += DistanceBetween(from, to) / vehicle->speed;
        eta += SegmentTrafficDelay(simulation, vehicle, from, to);
        from = to;
    }
    return eta;
}

static float EstimateVehicleETA(const Simulation *simulation, const Vehicle *vehicle,
                                Vector2 destination)
{
    Vehicle estimate = *vehicle;
    BuildVehicleRoute(&estimate, destination);
    return EstimateRouteETA(simulation, &estimate, 1);
}

static float EmergencyVehicleTrafficFactor(const Simulation *simulation, const Vehicle *vehicle)
{
    Vector2 next = vehicle->position;
    if (vehicle->routeIndex < vehicle->routeCount) next = vehicle->route[vehicle->routeIndex];
    int edge;
    float emergencyProgress = 0.0f;
    int direction = SegmentDirection(vehicle->position, next, &edge, &emergencyProgress);
    if (edge < 0) return 1.0f;
    RoadEdge road = roadEdges[edge];
    int fromNode = direction > 0 ? road.from : road.to;
    int toNode = direction > 0 ? road.to : road.from;
    float edgeLength = DistanceBetween(roadNodes[fromNode], roadNodes[toNode]);
    float speedLimit = vehicle->speed;

    // Only a non-yielding car physically ahead in the same directed lane can
    // slow this unit. Cars elsewhere on the road network have no effect.
    for (int i = 0; i < simulation->civilianCount; i++) {
        const CivilianCar *car = &simulation->civilianCars[i];
        if (car->fromNode != fromNode || car->toNode != toNode || car->yieldTime > 0.0f ||
            car->progress <= emergencyProgress) continue;
        float gap = (car->progress - emergencyProgress) * edgeLength;
        if (gap >= 52.0f) continue;
        float followSpeed = car->currentSpeed + fmaxf(0.0f, gap - 18.0f) * 1.25f;
        if (gap < 18.0f) followSpeed = fminf(followSpeed, car->currentSpeed);
        if (followSpeed < speedLimit) speedLimit = followSpeed;
    }
    if (speedLimit < 0.0f) speedLimit = 0.0f;
    return speedLimit / vehicle->speed;
}

float SimulationEstimateIncidentETA(const Simulation *simulation, int incidentIndex)
{
    if (!simulation || incidentIndex < 0 || incidentIndex >= simulation->incidentCount)
        return -1.0f;
    const Incident *incident = &simulation->incidents[incidentIndex];
    if (incident->status == INCIDENT_FAILED || incident->status == INCIDENT_RESOLVED) return 0.0f;

    float overallEta = 0.0f;
    for (int service = 0; service < SERVICE_COUNT; service++) {
        int required = incident->requiredUnitsByService[service];
        if (required <= 0) continue;
        float estimates[MAX_VEHICLES];
        int count = 0;
        for (int i = 0; i < simulation->vehicleCount; i++) {
            const Vehicle *vehicle = &simulation->vehicles[i];
            if (vehicle->serviceType != (IncidentType)service) continue;
            if (vehicle->assignedIncident == incidentIndex) {
                estimates[count++] = vehicle->arrived ? 0.0f :
                    EstimateRouteETA(simulation, vehicle, vehicle->routeIndex);
            } else if (vehicle->available) {
                estimates[count++] = EstimateVehicleETA(simulation, vehicle, incident->position);
            }
        }
        if (count < required) return -1.0f;
        for (int i = 0; i < count; i++)
            for (int j = i + 1; j < count; j++)
                if (estimates[j] < estimates[i]) {
                    float temp = estimates[i]; estimates[i] = estimates[j]; estimates[j] = temp;
                }
        if (estimates[required - 1] > overallEta) overallEta = estimates[required - 1];
    }
    return overallEta;
}

float SimulationEstimateUnitETA(const Simulation *simulation, int vehicleIndex,
                                int incidentIndex)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount ||
        incidentIndex < 0 || incidentIndex >= simulation->incidentCount) return -1.0f;
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    const Incident *incident = &simulation->incidents[incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED ||
        vehicle->serviceType >= SERVICE_COUNT ||
        incident->requiredUnitsByService[vehicle->serviceType] <= 0) return -1.0f;
    if (vehicle->assignedIncident == incidentIndex) {
        if (vehicle->activity == UNIT_TRANSPORTING || vehicle->activity == UNIT_RETURNING)
            return EstimateRouteETA(simulation, vehicle, vehicle->routeIndex);
        if (vehicle->arrived) return 0.0f;
        return EstimateRouteETA(simulation, vehicle, vehicle->routeIndex);
    }
    if (vehicle->available) return EstimateVehicleETA(simulation, vehicle, incident->position);
    return -1.0f;
}



IncidentType SimulationServiceForIncident(IncidentType type)

{

    // Traffic accidents and medical calls use the shared ambulance fleet.

    if (type == INCIDENT_MEDICAL || type == INCIDENT_ACCIDENT) return INCIDENT_ACCIDENT;

    return type;

}



int GetRequiredUnits(IncidentType type, int severity)

{

    if (severity <= 1) return 1;

    if (severity == 2) return 2;



    // There are only two police units, so a critical crime needs both.

    if (type == INCIDENT_CRIME) return 2;

    return 3;

}

static void ConfigureIncidentRequirements(Incident *incident)
{
    if (!incident) return;
    if (incident->type == INCIDENT_FIRE && incident->fireRoomsBurning < 1)
        incident->fireRoomsBurning = 1;
    memset(incident->requiredUnitsByService, 0, sizeof(incident->requiredUnitsByService));
    incident->requiredUnits = GetRequiredUnits(incident->type, incident->severity);
    IncidentType primaryService = SimulationServiceForIncident(incident->type);
    incident->requiredUnitsByService[primaryService] = incident->requiredUnits;
    incident->needsSceneSecurity = 0;
    incident->requiresTransport = 0;

    // Support crews are reserved for incidents where severity justifies them.
    if (incident->type == INCIDENT_FIRE && incident->severity >= 3) {
        incident->requiredUnitsByService[INCIDENT_CRIME] = 1;
        incident->requiredUnitsByService[INCIDENT_ACCIDENT] = 1;
        incident->needsSceneSecurity = 1;
        incident->requiresTransport = 1;
    } else if (incident->type == INCIDENT_ACCIDENT && incident->severity >= 2) {
        incident->requiredUnitsByService[INCIDENT_CRIME] = 1;
        incident->needsSceneSecurity = 1;
        incident->requiresTransport = 1;
    } else if (incident->type == INCIDENT_MEDICAL && incident->severity >= 2) {
        incident->requiresTransport = 1;
        if (incident->severity >= 3) {
            incident->requiredUnitsByService[INCIDENT_CRIME] = 1;
            incident->needsSceneSecurity = 1;
        }
    } else if (incident->type == INCIDENT_CRIME && incident->severity >= 3) {
        incident->requiredUnitsByService[INCIDENT_ACCIDENT] = 1;
        incident->needsSceneSecurity = 1;
        incident->requiresTransport = 1;
    }

    incident->totalRequiredUnits = 0;
    for (int service = 0; service < SERVICE_COUNT; service++)
        incident->totalRequiredUnits += incident->requiredUnitsByService[service];

    incident->workRequired = incident->type == INCIDENT_FIRE ?
        3.5f + incident->severity * 2.0f - incident->requiredUnits * 0.6f :
        incident->type == INCIDENT_CRIME ? 3.5f :
        incident->type == INCIDENT_MEDICAL ? 3.0f : 4.0f;
}

int CountRequiredUnits(const Incident *incident)
{
    if (!incident) return 0;
    return incident->totalRequiredUnits > 0 ? incident->totalRequiredUnits :
           incident->requiredUnits;
}



// Severity is the base score; deadline pressure raises it continuously.
float SimulationIncidentPriority(const Incident *incident)
{
    if (!incident || incident->deadline <= 0.0f) return 0.0f;
    float urgency = incident->ageTime / incident->deadline;
    if (urgency < 0.0f) urgency = 0.0f;
    if (urgency > 1.0f) urgency = 1.0f;
    return (float)incident->severity * 100.0f + urgency * 100.0f;
}


int CountAssignedUnits(const Simulation *simulation, int incidentIndex)

{

    int count = 0;

    for (int i = 0; i < simulation->vehicleCount; i++) {

        if (simulation->vehicles[i].assignedIncident == incidentIndex) count++;

    }

    return count;

}

int CountAssignedUnitsByService(const Simulation *simulation, int incidentIndex,
                                IncidentType serviceType)
{
    if (!simulation || serviceType < INCIDENT_FIRE || serviceType >= SERVICE_COUNT) return 0;
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (simulation->vehicles[i].assignedIncident == incidentIndex &&
            simulation->vehicles[i].serviceType == serviceType) count++;
    return count;
}



int CountArrivedUnits(const Simulation *simulation, int incidentIndex)

{

    int count = 0;

    for (int i = 0; i < simulation->vehicleCount; i++) {

        if (simulation->vehicles[i].assignedIncident == incidentIndex &&

            simulation->vehicles[i].arrived) count++;

    }

    return count;

}

int CountArrivedUnitsByService(const Simulation *simulation, int incidentIndex,
                               IncidentType serviceType)
{
    if (!simulation || serviceType < INCIDENT_FIRE || serviceType >= SERVICE_COUNT) return 0;
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (simulation->vehicles[i].assignedIncident == incidentIndex &&
            simulation->vehicles[i].serviceType == serviceType &&
            simulation->vehicles[i].arrived) count++;
    return count;
}



static int FindNearestAvailableVehicle(const Simulation *simulation,

                                       IncidentType serviceType,

                                       Vector2 position)

{

    int bestIndex = -1;

    float bestDistance = 1.0e30f;



    for (int i = 0; i < simulation->vehicleCount; i++) {

        const Vehicle *v = &simulation->vehicles[i];

        if (!v->available || v->serviceType != serviceType) continue;

        float d = DistanceBetween(v->position, position);

        if (d < bestDistance) {

            bestDistance = d;

            bestIndex = i;

        }

    }

    return bestIndex;

}



static void ReleaseIncidentVehicles(Simulation *simulation, int incidentIndex)

{

    for (int i = 0; i < simulation->vehicleCount; i++) {

        Vehicle *v = &simulation->vehicles[i];

        if (v->assignedIncident == incidentIndex) {

            v->arrived = 0;
            v->available = 0;
            v->activity = UNIT_RETURNING;
            BuildVehicleRoute(v, v->stationPosition);
            v->assignedIncident = -1;

        }

    }

}


static void RecordCallFeedback(Simulation *simulation, const Incident *incident)
{
    if (!simulation || !incident || incident->callIndex < 0 ||
        incident->callIndex >= simulation->callCount) return;
    EmergencyCall *call = &simulation->calls[incident->callIndex];
    call->feedbackReady = 1;
    call->callerSatisfied = incident->status == INCIDENT_RESOLVED &&
                            !call->operatorWrongService;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Caller %s after call #%02d %s.",
             call->callerSatisfied ? "satisfied" : "dissatisfied",
             incident->callIndex + 1,
             incident->status == INCIDENT_RESOLVED ? "was resolved" : "failed");
    LogEvent(simulation, message);
}



static void FailIncident(Simulation *simulation, int incidentIndex)

{

    Incident *incident = &simulation->incidents[incidentIndex];

    if (incident->status == INCIDENT_RESOLVED ||

        incident->status == INCIDENT_FAILED) return;



    incident->status = INCIDENT_FAILED;
    RecordCallFeedback(simulation, incident);

    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Call #%02d failed: %s deadline expired.",
             incident->callIndex >= 0 ? incident->callIndex + 1 : incidentIndex + 1,
             incident->type == INCIDENT_FIRE ? "fire" :
             incident->type == INCIDENT_CRIME ? "crime" :
             incident->type == INCIDENT_MEDICAL ? "medical" : "accident");
    LogEvent(simulation, message);

    simulation->failedByType[SimulationServiceForIncident(incident->type)]++;

    ReleaseIncidentVehicles(simulation, incidentIndex);

}



void SimulationInit(Simulation *simulation)

{

    if (!simulation) return;

    // Reset every array entry and state field, including fields added by newer
    // simulation features. Counts alone are not enough when a reset happens
    // while units, calls, or pedestrians still hold references to old state.
    memset(simulation, 0, sizeof(*simulation));

    simulation->incidentCount = 0;

    simulation->vehicleCount = 9;
    simulation->dispatchStrategy = DISPATCH_PRIORITY;
    simulation->dispatchMode = DISPATCH_AUTOMATIC;
    simulation->trafficLevel = TRAFFIC_LIGHT;
    simulation->cityCondition = CITY_CLEAR;
    simulation->civilianCount = 0;
    simulation->pedestrianCount = MAX_PEDESTRIANS;
    simulation->hospitalPosition = CITY_LAYOUT_HOSPITAL_ENTRANCE;
    simulation->apartmentFireDrill = 0;
    simulation->evacuationEventLogged = 0;
    simulation->fireCascadeCount = 0;
    simulation->fireCascadeLimitLogged = 0;
    simulation->callCount = 0;
    simulation->elapsedTime = 0.0f;
    simulation->eventCount = 0;



    for (int i = 0; i < SERVICE_COUNT; i++) {

        simulation->dispatchedByType[i] = 0;

        simulation->resolvedByType[i] = 0;

        simulation->failedByType[i] = 0;

        simulation->totalResponseTimeByType[i] = 0.0f;

    }



    simulation->vehicles[0] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={300,270}, .stationPosition={300,270}, .speed=130.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[1] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={330,270}, .stationPosition={330,270}, .speed=130.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[2] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={315,245}, .stationPosition={315,245}, .speed=130.0f, .available=1, .assignedIncident=-1};



    simulation->vehicles[3] = (Vehicle){.serviceType=INCIDENT_CRIME, .position={665,600}, .stationPosition={665,600}, .speed=145.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[4] = (Vehicle){.serviceType=INCIDENT_CRIME, .position={695,600}, .stationPosition={695,600}, .speed=145.0f, .available=1, .assignedIncident=-1};



    simulation->vehicles[5] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={868,586}, .stationPosition={868,586}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[6] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={890,586}, .stationPosition={890,586}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[7] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={868,565}, .stationPosition={868,565}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[8] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={890,565}, .stationPosition={890,565}, .speed=155.0f, .available=1, .assignedIncident=-1};

    for (int i = 0; i < MAX_PEDESTRIANS; i++) {
        Pedestrian *p = &simulation->pedestrians[i];
        *p = (Pedestrian){0};
        p->sidewalk = i % 6;
        p->direction = (i % 3) ? 1 : -1;
        p->speed = 18.0f + (float)(i % 5) * 2.3f;
        p->state = PEDESTRIAN_WALKING;
        p->behaviorIncident = -1;
        p->routeIndex = 0;
        p->targetNode = -1;
        float sidewalkMin[6] = {20, 453, 828, 20, 453, 828};
        float sidewalkMax[6] = {338, 714, 1094, 338, 714, 1094};
        float span = sidewalkMax[p->sidewalk] - sidewalkMin[p->sidewalk] - 14.0f;
        p->position = (Vector2){sidewalkMin[p->sidewalk] + 7.0f + (float)((i * 31) % (int)span),
                                p->sidewalk < 3 ? 282.0f : 370.0f};
        p->routeProgress = (p->position.x - sidewalkMin[p->sidewalk]) /
                           (sidewalkMax[p->sidewalk] - sidewalkMin[p->sidewalk]);
        if (p->routeProgress < 0.0f) p->routeProgress = 0.0f;
        if (p->routeProgress > 1.0f) p->routeProgress = 1.0f;
    }
    SimulationSetTrafficLevel(simulation, TRAFFIC_LIGHT);
    // Start timed call generation by default without emitting a user action
    // event during initialization or reset.
    simulation->autoIncidentsEnabled = 1;
    simulation->autoIncidentRandomState = AUTO_INCIDENT_DEFAULT_SEED;
    simulation->autoIncidentNextAt = simulation->elapsedTime +
                                     AutoIncidentRandomInterval(simulation);

}

int SimulationStartApartmentFireDrillAtSeverity(Simulation *simulation, int severity)
{
    if (!simulation) return 0;
    if (severity < 1) severity = 1;
    if (severity > 3) severity = 3;
    DispatchMode mode = simulation->dispatchMode;
    SimulationInit(simulation);
    simulation->dispatchMode = mode;
    simulation->apartmentFireDrill = 1;
    SimulationSetCityCondition(simulation, severity >= 3 ? CITY_WIND : CITY_CLEAR);
    SimulationSetTrafficLevel(simulation, severity >= 3 ? TRAFFIC_MEDIUM : TRAFFIC_LIGHT);
    const char *report = severity >= 3 ?
        "Apartment fire: flames spreading, residents trapped, several injured." :
        severity == 2 ? "Smoke reported in an apartment building; residents are evacuating." :
                        "Small fire reported in one apartment unit; residents are evacuating.";
    if (!SimulationCreateCall(simulation, (Vector2){624, 131}, report)) return 0;
    int callIndex = simulation->callCount - 1;
    simulation->calls[callIndex].fireBuildingIndex = 4;
    simulation->calls[callIndex].fireCascadeStep = 0;
    simulation->fireCascadeCount = 1;
    SimulationSetCallTruth(simulation, callIndex, INCIDENT_FIRE, severity);
    LogEvent(simulation, severity >= 3 ?
             "Apartment fire drill started at high severity; fire, police and EMS response may be needed." :
             severity == 2 ? "Apartment fire drill started at medium severity; fire crews respond first." :
                             "Apartment fire drill started at low severity; one fire unit is sufficient.");
    return 1;
}

int SimulationStartApartmentFireDrill(Simulation *simulation)
{
    return SimulationStartApartmentFireDrillAtSeverity(simulation, 3);
}

void SimulationSetTrafficLevel(Simulation *simulation, TrafficLevel level)
{
    if (!simulation) return;
    if (level < TRAFFIC_LIGHT || level >= TRAFFIC_LEVEL_COUNT) level = TRAFFIC_LIGHT;
    simulation->trafficLevel = level;
    int oldCount = simulation->civilianCount;
    int newCount = TrafficCarCount(level);
    for (int i = oldCount; i < newCount; i++) {
        int edgeIndex = i % ROAD_EDGE_COUNT;
        RoadEdge edge = roadEdges[edgeIndex];
        int direction = ((i / ROAD_EDGE_COUNT) + edgeIndex) % 2;
        CivilianCar *car = &simulation->civilianCars[i];
        car->fromNode = direction ? edge.to : edge.from;
        car->toNode = direction ? edge.from : edge.to;
        car->progress = (float)((i * 37) % 73) / 100.0f;
        // The short East Ward exit segment is only 35 map units long. Seed its
        // deterministic cars with enough headway so they do not begin overlapped.
        if (edgeIndex == 3)
            car->progress = 0.20f + (float)(i / ROAD_EDGE_COUNT) * 0.30f;
        car->speed = 40.0f + (float)((i * 17) % 21);
        car->currentSpeed = car->speed;
        car->yieldTime = 0.0f;
        car->routeCursor = i;
        car->laneIndex = 0;
        car->sirenReactionDelay = 0.25f + (float)((i * 17 + 5) % 13) * 0.11f;
        car->sirenReactionTimer = -1.0f;
        car->sirenSource = -1;
        Vector2 directionVector = {roadNodes[car->toNode].x - roadNodes[car->fromNode].x,
                                   roadNodes[car->toNode].y - roadNodes[car->fromNode].y};
        car->headingDegrees = atan2f(directionVector.y, directionVector.x) * RAD2DEG;
        car->position = CivilianLanePosition(car->fromNode, car->toNode,
                                             car->progress, 0.0f);
    }
    simulation->civilianCount = newCount;
}

static int NextRoadNode(int fromNode, int currentNode, int cursor)
{
    int candidates[ROAD_NODE_COUNT];
    int count = 0;
    for (int i = 0; i < ROAD_EDGE_COUNT; i++) {
        if (roadEdges[i].from == currentNode && roadEdges[i].to != fromNode)
            candidates[count++] = roadEdges[i].to;
        else if (roadEdges[i].to == currentNode && roadEdges[i].from != fromNode)
            candidates[count++] = roadEdges[i].from;
    }
    if (count == 0) {
        for (int i = 0; i < ROAD_EDGE_COUNT; i++) {
            if (roadEdges[i].from == currentNode) candidates[count++] = roadEdges[i].to;
            else if (roadEdges[i].to == currentNode) candidates[count++] = roadEdges[i].from;
        }
    }
    if (count == 0) return fromNode;

    // Continue straight through a junction whenever possible. This avoids
    // snapping a car across lanes during an arbitrary turn and keeps opposing
    // traffic on its own side of the road.
    Vector2 incoming = {roadNodes[currentNode].x - roadNodes[fromNode].x,
                        roadNodes[currentNode].y - roadNodes[fromNode].y};
    float incomingLength = DistanceBetween((Vector2){0,0}, incoming);
    int best = cursor % count;
    float bestAlignment = -2.0f;
    for (int i = 0; i < count; i++) {
        Vector2 outgoing = {roadNodes[candidates[i]].x - roadNodes[currentNode].x,
                            roadNodes[candidates[i]].y - roadNodes[currentNode].y};
        float outgoingLength = DistanceBetween((Vector2){0,0}, outgoing);
        float alignment = (incoming.x * outgoing.x + incoming.y * outgoing.y) /
                          (incomingLength * outgoingLength);
        if (alignment > bestAlignment + 0.001f) {
            bestAlignment = alignment;
            best = i;
        } else if (fabsf(alignment - bestAlignment) <= 0.001f && i == cursor % count) {
            best = i;
        }
    }
    return candidates[best];
}

static int RoadNodeDegree(int node)
{
    int degree = 0;
    for (int i = 0; i < ROAD_EDGE_COUNT; i++)
        if (roadEdges[i].from == node || roadEdges[i].to == node) degree++;
    return degree;
}

static int IntersectionHasRightOfWay(const Simulation *simulation, int carIndex, int node)
{
    if (RoadNodeDegree(node) < 3) return 1;
    const CivilianCar *car = &simulation->civilianCars[carIndex];
    float approachDistance = (1.0f - car->progress) *
        DistanceBetween(roadNodes[car->fromNode], roadNodes[car->toNode]);

    // Only the lead car in each inbound lane may request the intersection.
    // In particular, followers must not block the lead car while yielding.
    for (int i = 0; i < simulation->civilianCount; i++) {
        if (i == carIndex) continue;
        const CivilianCar *other = &simulation->civilianCars[i];
        if (other->fromNode == car->fromNode && other->toNode == node &&
            other->progress > car->progress &&
            (other->progress - car->progress) *
                DistanceBetween(roadNodes[car->fromNode], roadNodes[node]) < 42.0f) return 0;
    }

    for (int i = 0; i < simulation->civilianCount; i++) {
        if (i == carIndex) continue;
        const CivilianCar *other = &simulation->civilianCars[i];
        // Only a car physically inside the junction blocks it. A queued car
        // on the outgoing road must not hold the intersection forever.
        if (other->fromNode == node &&
            DistanceBetween(other->position, roadNodes[node]) < 24.0f) return 0;

        // Arbitrate cars at the head of crossing approaches by stable car
        // index. Vehicles farther back do not reserve the junction.
        if (other->toNode == node && other->fromNode != car->fromNode) {
            float otherApproach = (1.0f - other->progress) *
                DistanceBetween(roadNodes[other->fromNode], roadNodes[other->toNode]);
            int otherIsHead = 1;
            for (int j = 0; j < simulation->civilianCount; j++) {
                if (j == i) continue;
                const CivilianCar *follower = &simulation->civilianCars[j];
                if (follower->fromNode == other->fromNode && follower->toNode == node &&
                    follower->progress > other->progress &&
                    (follower->progress - other->progress) *
                        DistanceBetween(roadNodes[other->fromNode], roadNodes[node]) < 42.0f) {
                    otherIsHead = 0;
                    break;
                }
            }
            if (otherIsHead && i < carIndex && otherApproach < 34.0f &&
                approachDistance < 42.0f) return 0;
        }
    }
    return 1;
}

static int EmergencyApproachingCivilian(const Simulation *simulation, const CivilianCar *car)
{
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (vehicle->available || vehicle->arrived || vehicle->assignedIncident < 0 ||
            vehicle->assignedIncident >= simulation->incidentCount ||
            vehicle->routeIndex >= vehicle->routeCount) continue;
        const Incident *incident = &simulation->incidents[vehicle->assignedIncident];
        if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED) continue;
        int emergencyEdge;
        float emergencyProgress = 0.0f;
        int direction = SegmentDirection(vehicle->position, vehicle->route[vehicle->routeIndex],
                                         &emergencyEdge, &emergencyProgress);
        if (emergencyEdge != RoadEdgeForNodes(car->fromNode, car->toNode)) continue;
        RoadEdge road = roadEdges[emergencyEdge];
        int emergencyFrom = direction > 0 ? road.from : road.to;
        int emergencyTo = direction > 0 ? road.to : road.from;
        if (car->fromNode != emergencyFrom || car->toNode != emergencyTo ||
            emergencyProgress >= car->progress) continue;
        float edgeLength = DistanceBetween(roadNodes[emergencyFrom], roadNodes[emergencyTo]);
        float gap = (car->progress - emergencyProgress) * edgeLength;
        if (gap > 82.0f) continue;
        return i;
    }
    return -1;
}

static float CivilianLeaderGap(const Simulation *simulation, int carIndex)
{
    const CivilianCar *car = &simulation->civilianCars[carIndex];
    int edge = RoadEdgeForNodes(car->fromNode, car->toNode);
    float edgeLength = DistanceBetween(roadNodes[car->fromNode], roadNodes[car->toNode]);
    float nearestGap = 1.0e30f;
    for (int i = 0; i < simulation->civilianCount; i++) {
        if (i == carIndex) continue;
        const CivilianCar *other = &simulation->civilianCars[i];
        if (RoadEdgeForNodes(other->fromNode, other->toNode) != edge ||
            other->fromNode != car->fromNode || other->toNode != car->toNode ||
            other->laneIndex != car->laneIndex || other->progress <= car->progress) continue;
        float gap = (other->progress - car->progress) * edgeLength;
        if (gap < nearestGap) nearestGap = gap;
    }
    return nearestGap;
}

static void UpdateCivilianTraffic(Simulation *simulation, float deltaTime)
{
    float previousSpeeds[MAX_CIVILIAN_CARS];
    for (int i = 0; i < simulation->civilianCount; i++) {
        CivilianCar *car = &simulation->civilianCars[i];
        previousSpeeds[i] = car->currentSpeed;
        int approaching = EmergencyApproachingCivilian(simulation, car);
        if (approaching >= 0) {
            if (car->sirenSource != approaching) {
                car->sirenSource = approaching;
                car->sirenReactionTimer = car->sirenReactionDelay;
            }
            if (car->sirenReactionTimer > 0.0f) {
                car->sirenReactionTimer -= deltaTime;
                if (car->sirenReactionTimer <= 0.0f) car->yieldTime = 1.0f;
            } else {
                car->yieldTime = 1.0f;
            }
        } else {
            car->sirenSource = -1;
            car->sirenReactionTimer = -1.0f;
        }
        if (approaching < 0 && car->yieldTime > 0.0f) {
            car->yieldTime -= deltaTime;
            if (car->yieldTime < 0.0f) car->yieldTime = 0.0f;
        }
    }

    for (int i = 0; i < simulation->civilianCount; i++) {
        CivilianCar *car = &simulation->civilianCars[i];
        float targetSpeed = car->yieldTime > 0.0f ? car->speed * 0.04f : car->speed;
        float gap = CivilianLeaderGap(simulation, i);
        if (gap < 36.0f) {
            float leaderSpeed = previousSpeeds[i];
            float edgeLength = DistanceBetween(roadNodes[car->fromNode], roadNodes[car->toNode]);
            for (int j = 0; j < simulation->civilianCount; j++) {
                const CivilianCar *other = &simulation->civilianCars[j];
                if (j == i || other->fromNode != car->fromNode || other->toNode != car->toNode ||
                    other->laneIndex != car->laneIndex || other->progress <= car->progress) continue;
                float candidate = (other->progress - car->progress) * edgeLength;
                if (fabsf(candidate - gap) < 0.01f) { leaderSpeed = previousSpeeds[j]; break; }
            }
            float followSpeed = leaderSpeed * fmaxf(0.0f, (gap - 16.0f) / 20.0f);
            if (followSpeed < targetSpeed) targetSpeed = followSpeed;
        }

        float edgeLength = DistanceBetween(roadNodes[car->fromNode], roadNodes[car->toNode]);
        float remaining = (1.0f - car->progress) * edgeLength;
        int node = car->toNode;
        if (RoadNodeDegree(node) >= 3 && !IntersectionHasRightOfWay(simulation, i, node) &&
            remaining < 42.0f && deltaTime > 0.0f) {
            float approachSpeed = fmaxf(0.0f, remaining - 19.0f) / deltaTime;
            if (approachSpeed < targetSpeed) targetSpeed = approachSpeed;
        }

        float rate = targetSpeed < car->currentSpeed ? 180.0f : 58.0f;
        float speedChange = rate * deltaTime;
        if (targetSpeed < car->currentSpeed)
            car->currentSpeed = fmaxf(targetSpeed, car->currentSpeed - speedChange);
        else
            car->currentSpeed = fminf(targetSpeed, car->currentSpeed + speedChange);

        float distanceLeft = car->currentSpeed * deltaTime;
        int guard = 0;
        while (distanceLeft > 0.001f && guard++ < 8) {
            edgeLength = DistanceBetween(roadNodes[car->fromNode], roadNodes[car->toNode]);
            remaining = (1.0f - car->progress) * edgeLength;
            node = car->toNode;
            if (RoadNodeDegree(node) >= 3 && !IntersectionHasRightOfWay(simulation, i, node) &&
                remaining <= distanceLeft + 19.0f) {
                float allowed = fmaxf(0.0f, remaining - 19.0f);
                car->progress += allowed / edgeLength;
                distanceLeft = 0.0f;
                break;
            }
            if (distanceLeft >= remaining) {
                distanceLeft -= remaining;
                int previous = car->fromNode;
                car->fromNode = car->toNode;
                car->toNode = NextRoadNode(previous, car->fromNode, car->routeCursor++ + i);
                car->progress = 0.0f;
            } else {
                car->progress += distanceLeft / edgeLength;
                distanceLeft = 0.0f;
            }
        }
        float shoulder = car->yieldTime > 0.0f ? 12.0f : 0.0f;
        Vector2 directionVector = {roadNodes[car->toNode].x - roadNodes[car->fromNode].x,
                                   roadNodes[car->toNode].y - roadNodes[car->fromNode].y};
        car->headingDegrees = atan2f(directionVector.y, directionVector.x) * RAD2DEG;
        car->position = CivilianLanePosition(car->fromNode, car->toNode,
                                             car->progress, shoulder);
    }
}



int SimulationCreateIncident(Simulation *simulation, IncidentType type, Vector2 position)

{

    if (!simulation || type < INCIDENT_FIRE || type > INCIDENT_MEDICAL) return 0;

    if (simulation->incidentCount >= MAX_INCIDENTS) return 0;



    Incident *in = &simulation->incidents[simulation->incidentCount];

    *in = (Incident){0};

    in->type = type;
    in->callIndex = -1;
    in->fireBuildingIndex = -1;
    in->fireCascadeStep = -1;

    in->status = INCIDENT_PENDING;

    in->position = position;

    in->severity = 1;

    ConfigureIncidentRequirements(in);

    in->deadline = type == INCIDENT_FIRE ? 35.0f :

                   type == INCIDENT_CRIME ? 30.0f :

                   type == INCIDENT_MEDICAL ? 25.0f : 30.0f;

    simulation->incidentCount++;

    return 1;

}

static void ClassifyCallFromReport(const EmergencyCall *call, IncidentType *type,
                                   int *severity);

int SimulationCreateCall(Simulation *simulation, Vector2 location, const char *report)
{
    if (!simulation || simulation->callCount >= MAX_CALLS || !report) return 0;
    EmergencyCall *call = &simulation->calls[simulation->callCount];
    *call = (EmergencyCall){0};
    call->location = location;
    call->receivedAt = simulation->elapsedTime;
    call->classifiedType = INCIDENT_FIRE;
    call->fireBuildingIndex = -1;
    call->fireCascadeStep = -1;
    call->incidentIndex = -1;
    snprintf(call->report, sizeof(call->report), "%s", report);
    ClassifyCallFromReport(call, &call->trueType, &call->trueSeverity);
    snprintf(call->decisionReason, sizeof(call->decisionReason),
             "Awaiting operator classification.");
    simulation->callCount++;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Call #%02d received at %.0f, %.0f.",
             simulation->callCount, location.x, location.y);
    LogEvent(simulation, message);
    return 1;
}

int SimulationSetCallTruth(Simulation *simulation, int callIndex,
                           IncidentType trueType, int trueSeverity)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount ||
        trueType < INCIDENT_FIRE || trueType > INCIDENT_MEDICAL ||
        trueSeverity < 1 || trueSeverity > 3) return 0;
    EmergencyCall *call = &simulation->calls[callIndex];
    if (call->classified || call->dispatched) return 0;
    call->trueType = trueType;
    call->trueSeverity = trueSeverity;
    return 1;
}

int SimulationClassifyCall(Simulation *simulation, int callIndex,
                           IncidentType type, int severity)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount ||
        type < INCIDENT_FIRE || type > INCIDENT_MEDICAL || severity < 1 || severity > 3)
        return 0;
    EmergencyCall *call = &simulation->calls[callIndex];
    if (call->dispatched) return 0;

    Incident *incident = NULL;
    if (call->incidentIndex < 0) {
        if (!SimulationCreateIncident(simulation, type, call->location)) return 0;
        call->incidentIndex = simulation->incidentCount - 1;
        incident = &simulation->incidents[call->incidentIndex];
        incident->callIndex = callIndex;
        incident->fireBuildingIndex = call->fireBuildingIndex;
        incident->fireCascadeStep = call->fireCascadeStep;
        incident->ageTime = fmaxf(0.0f, simulation->elapsedTime - call->receivedAt);
        incident->waitTime = incident->ageTime;
        call->classifiedAt = simulation->elapsedTime;
    } else {
        incident = &simulation->incidents[call->incidentIndex];
        if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED ||
            CountAssignedUnits(simulation, call->incidentIndex) > 0) return 0;
        call->classifiedAt = simulation->elapsedTime;
    }

    incident->type = type;
    incident->severity = severity;
    call->classifiedType = type;
    call->severity = severity;
    if (!call->operatorOverride)
        call->operatorWrongService = SimulationServiceForIncident(type) !=
                                     SimulationServiceForIncident(call->trueType);

    ConfigureIncidentRequirements(incident);
    // A mistaken operator classification should initially send only the
    // chosen service. Supporting crews are requested after the true incident
    // is revealed on arrival and the call is reclassified.
    if (call->operatorOverride && call->operatorWrongService &&
        type == call->operatorChoiceType) {
        IncidentType chosenService = SimulationServiceForIncident(type);
        for (int service = 0; service < SERVICE_COUNT; service++)
            if (service != (int)chosenService) incident->requiredUnitsByService[service] = 0;
        incident->totalRequiredUnits = incident->requiredUnits;
        incident->needsSceneSecurity = 0;
        incident->requiresTransport = 0;
    }
    incident->deadline = type == INCIDENT_FIRE ? 35.0f :
                         type == INCIDENT_CRIME ? 30.0f :
                         type == INCIDENT_MEDICAL ? 25.0f : 30.0f;
    call->classified = 1;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Call #%02d classified: %s, %s priority.",
             callIndex + 1, type == INCIDENT_FIRE ? "fire" :
             type == INCIDENT_CRIME ? "crime" :
             type == INCIDENT_MEDICAL ? "medical" : "accident",
             severity >= 3 ? "high" : severity == 2 ? "medium" : "low");
    LogEvent(simulation, message);
    return 1;
}

static int HasWord(const char *text, const char *word)
{
    if (!text || !word || !*word) return 0;
    for (const char *start = text; *start; start++) {
        const char *a = start;
        const char *b = word;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
            a++;
            b++;
        }
        if (!*b) return 1;
    }
    return 0;
}

static void ClassifyCallFromReport(const EmergencyCall *call, IncidentType *type,
                                   int *severity)
{
    const char *report = call->report;
    if (HasWord(report, "smoke") || HasWord(report, "fire") ||
        HasWord(report, "flame") || HasWord(report, "burning")) {
        *type = INCIDENT_FIRE;
        *severity = HasWord(report, "trapped") || HasWord(report, "spreading") ? 3 :
                    HasWord(report, "building") || HasWord(report, "inside") ? 2 : 1;
    } else if (HasWord(report, "collapsed") || HasWord(report, "unresponsive") ||
               HasWord(report, "injured") || HasWord(report, "breathing") ||
               HasWord(report, "medical")) {
        *type = INCIDENT_MEDICAL;
        *severity = HasWord(report, "unresponsive") || HasWord(report, "not breathing") ? 3 :
                    HasWord(report, "injured") || HasWord(report, "urgent") ? 2 : 1;
    } else if (HasWord(report, "collision") || HasWord(report, "collided") ||
               HasWord(report, "crash") || HasWord(report, "vehicle") ||
               HasWord(report, "traffic")) {
        *type = INCIDENT_ACCIDENT;
        *severity = HasWord(report, "multiple") || HasWord(report, "serious injury") ? 3 :
                    HasWord(report, "blocked") || HasWord(report, "injur") ? 2 : 1;
    } else {
        *type = INCIDENT_CRIME;
        *severity = HasWord(report, "weapon") || HasWord(report, "assault") ? 3 :
                    HasWord(report, "in progress") || HasWord(report, "break-in") ? 2 : 1;
    }
}

int SimulationSuggestCallClassification(const Simulation *simulation, int callIndex,
                                        IncidentType *type, int *severity)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount ||
        !type || !severity) return 0;
    const EmergencyCall *call = &simulation->calls[callIndex];
    ClassifyCallFromReport(call, type, severity);
    float callAge = fmaxf(0.0f, simulation->elapsedTime - call->receivedAt);
    float deadline = *type == INCIDENT_FIRE ? 35.0f :
                     *type == INCIDENT_MEDICAL ? 25.0f : 30.0f;
    if (callAge >= deadline * 0.78f && *severity < 3) *severity = 3;
    else if (callAge >= deadline * 0.5f && *severity < 2) *severity = 2;
    return 1;
}

static void AssignVehicle(Simulation *simulation, int vehicleIndex, int incidentIndex)
{
    Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    Incident *incident = &simulation->incidents[incidentIndex];
    IncidentType service = vehicle->serviceType;
    vehicle->available = 0;
    vehicle->assignedIncident = incidentIndex;
    vehicle->arrived = 0;
    vehicle->activity = UNIT_RESPONDING;
    vehicle->patientDelivered = 0;
    vehicle->trafficDelayLogged = 0;
    BuildVehicleRoute(vehicle, incident->position);
    simulation->dispatchedByType[service]++;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "%s unit %d dispatched to call #%02d.",
             ServiceEventName(vehicle->serviceType), ServiceUnitNumber(simulation, vehicleIndex),
             incident->callIndex >= 0 ? incident->callIndex + 1 : incidentIndex + 1);
    LogEvent(simulation, message);
}

int SimulationDispatchCallVehicles(Simulation *simulation, int callIndex,
                                   unsigned int vehicleMask)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount || !vehicleMask)
        return 0;
    EmergencyCall *call = &simulation->calls[callIndex];
    if (!call->classified || call->incidentIndex < 0 ||
        call->incidentIndex >= simulation->incidentCount) return 0;
    Incident *incident = &simulation->incidents[call->incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED)
        return 0;

    unsigned int validMask = (1u << simulation->vehicleCount) - 1u;
    if ((vehicleMask & ~validMask) != 0) return 0;
    int selectedByService[SERVICE_COUNT] = {0};
    int missingByService[SERVICE_COUNT] = {0};
    int selected = 0;
    for (int service = 0; service < SERVICE_COUNT; service++) {
        missingByService[service] = incident->requiredUnitsByService[service] -
            CountAssignedUnitsByService(simulation, call->incidentIndex, (IncidentType)service);
        if (missingByService[service] < 0) return 0;
    }
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (!(vehicleMask & (1u << i))) continue;
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (!vehicle->available || vehicle->serviceType < INCIDENT_FIRE ||
            vehicle->serviceType >= SERVICE_COUNT) return 0;
        selectedByService[vehicle->serviceType]++;
        selected++;
    }
    for (int service = 0; service < SERVICE_COUNT; service++)
        if (selectedByService[service] != missingByService[service]) return 0;
    if (selected == 0) return 0;

    for (int i = 0; i < simulation->vehicleCount; i++)
        if (vehicleMask & (1u << i)) AssignVehicle(simulation, i, call->incidentIndex);
    int wasDispatched = call->dispatched;
    incident->status = INCIDENT_DISPATCHED;
    call->dispatched = 1;
    if (!wasDispatched && !call->mismatchRecorded)
        call->dispatchedAt = simulation->elapsedTime;
    snprintf(call->decisionReason, sizeof(call->decisionReason),
             wasDispatched ? "Manual operator dispatched %d additional required response units." :
                             "Manual operator dispatched all %d required response units.",
             selected);
    return selected;
}

void SimulationProcessOperator(Simulation *simulation)
{
    if (!simulation || simulation->dispatchMode != DISPATCH_AUTOMATIC) return;
    int order[MAX_CALLS];
    int count = 0;

    // First classify every report. Then rank calls so urgent deadlines are not
    // delayed by a lower-priority call that happened to be created first.
    for (int callIndex = 0; callIndex < simulation->callCount; callIndex++) {
        EmergencyCall *call = &simulation->calls[callIndex];
        // A wrong-service override remains committed to that initial response
        // until the first wrong team reaches the scene and records the mismatch.
        if (call->dispatched && call->operatorWrongService && !call->mismatchRecorded) continue;
        // AUTO OP calls awaiting a human's timed override stay queued until
        // the UI accepts a service. Existing classified calls still dispatch.
        if (call->overridePending && !call->classified) continue;
        if (!call->classified) {
            IncidentType type;
            int severity;
            if (!SimulationSuggestCallClassification(simulation, callIndex, &type, &severity))
                continue;
            if (!SimulationClassifyCall(simulation, callIndex, type, severity)) continue;
        }
        if (call->incidentIndex < 0 || call->incidentIndex >= simulation->incidentCount) continue;
        int insertAt = count;
        if (simulation->dispatchStrategy == DISPATCH_PRIORITY) {
            const Incident *incident = &simulation->incidents[call->incidentIndex];
            float priority = SimulationIncidentPriority(incident);
            while (insertAt > 0) {
                const EmergencyCall *previousCall = &simulation->calls[order[insertAt - 1]];
                const Incident *previous = &simulation->incidents[previousCall->incidentIndex];
                float previousPriority = SimulationIncidentPriority(previous);
                if (previousPriority > priority ||
                    (previousPriority == priority && previousCall->receivedAt <= call->receivedAt)) break;
                order[insertAt] = order[insertAt - 1];
                insertAt--;
            }
        }
        order[insertAt] = callIndex;
        count++;
    }

    for (int ranked = 0; ranked < count; ranked++) {
        int callIndex = order[ranked];
        EmergencyCall *call = &simulation->calls[callIndex];

        Incident *incident = &simulation->incidents[call->incidentIndex];
        if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED) continue;
        int neededByService[SERVICE_COUNT] = {0};
        int canDispatch = 1;
        int neededTotal = 0;
        int availableTotal = 0;
        for (int service = 0; service < SERVICE_COUNT; service++) {
            int assigned = CountAssignedUnitsByService(simulation, call->incidentIndex,
                                                       (IncidentType)service);
            neededByService[service] = incident->requiredUnitsByService[service] - assigned;
            if (neededByService[service] < 0) neededByService[service] = 0;
            int available = SimulationCountAvailableVehicles(simulation, (IncidentType)service);
            neededTotal += neededByService[service];
            availableTotal += available < neededByService[service] ? available : neededByService[service];
            if (available < neededByService[service]) canDispatch = 0;
        }
        if (neededTotal == 0) continue;
        if (!canDispatch) {
            const char *level = incident->severity >= 3 ? "High" :
                                incident->severity == 2 ? "Medium" : "Low";
            snprintf(call->decisionReason, sizeof(call->decisionReason),
                     "%s-severity %s; %d/%d required units available across services. Waiting and retrying.",
                     level, incident->type == INCIDENT_FIRE ? "fire" :
                     incident->type == INCIDENT_CRIME ? "crime" :
                     incident->type == INCIDENT_ACCIDENT ? "road collision" : "medical call",
                     availableTotal, neededTotal);
            continue;
        }

        unsigned int chosenMask = 0;
        for (int service = 0; service < SERVICE_COUNT; service++) {
            for (int n = 0; n < neededByService[service]; n++) {
                int best = -1;
                float bestEta = 1.0e30f;
                for (int i = 0; i < simulation->vehicleCount; i++) {
                    Vehicle *vehicle = &simulation->vehicles[i];
                    if (!vehicle->available || vehicle->serviceType != (IncidentType)service ||
                        (chosenMask & (1u << i))) continue;
                    float eta = EstimateVehicleETA(simulation, vehicle, incident->position);
                    if (eta < bestEta) { bestEta = eta; best = i; }
                }
                if (best < 0) break;
                chosenMask |= 1u << best;
            }
        }
        int chosenCount = 0;
        for (int i = 0; i < simulation->vehicleCount; i++)
            if (chosenMask & (1u << i)) chosenCount++;
        if (chosenCount != neededTotal) continue;

        int wasDispatched = call->dispatched;
        for (int i = 0; i < simulation->vehicleCount; i++)
            if (chosenMask & (1u << i)) AssignVehicle(simulation, i, call->incidentIndex);
        incident->status = INCIDENT_DISPATCHED;
        call->dispatched = 1;
        if (!wasDispatched && !call->mismatchRecorded)
            call->dispatchedAt = simulation->elapsedTime;
        float eta = SimulationEstimateIncidentETA(simulation, call->incidentIndex);
        const char *level = incident->severity >= 3 ? "High" :
                            incident->severity == 2 ? "Medium" : "Low";
        snprintf(call->decisionReason, sizeof(call->decisionReason),
                 wasDispatched ?
                 "%s-severity incident escalated; sending %d additional required units. ETA %.1fs in %s traffic." :
                 "%s-severity incident; sending %d required units across services. ETA %.1fs in %s traffic.",
                 level, neededTotal, eta, SimulationTrafficLevelName(simulation->trafficLevel));
    }
}



static void DispatchAvailableUnits(Simulation *simulation)

{
    if (simulation->dispatchMode != DISPATCH_AUTOMATIC) return;

    // Nearest-first preserves creation order. Priority dispatch ranks by
    // severity and deadline pressure, favoring longer waits on ties.
    int order[MAX_INCIDENTS];
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        Incident *in = &simulation->incidents[i];
        if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;
        if (in->callIndex >= 0) continue; // Operator calls dispatch atomically through their call record.
        int insertAt = count;
        if (simulation->dispatchStrategy == DISPATCH_PRIORITY) {
            float priority = SimulationIncidentPriority(in);
            while (insertAt > 0) {
                Incident *previous = &simulation->incidents[order[insertAt - 1]];
                float previousPriority = SimulationIncidentPriority(previous);
                if (previousPriority > priority ||
                    (previousPriority == priority && previous->waitTime >= in->waitTime)) break;
                order[insertAt] = order[insertAt - 1];
                insertAt--;
            }
        }
        order[insertAt] = i;
        count++;
    }

    for (int ranked = 0; ranked < count; ranked++) {
        int i = order[ranked];
        Incident *in = &simulation->incidents[i];



        for (int service = 0; service < SERVICE_COUNT; service++) {
            while (CountAssignedUnitsByService(simulation, i, (IncidentType)service) <
                   in->requiredUnitsByService[service]) {
                int vIndex = FindNearestAvailableVehicle(simulation, (IncidentType)service,
                                                         in->position);
                if (vIndex < 0) break;
                AssignVehicle(simulation, vIndex, i);
            }
        }



        if (CountAssignedUnits(simulation, i) > 0 &&

            in->status == INCIDENT_PENDING) {

            in->status = INCIDENT_DISPATCHED;

        }

    }

}

int SimulationDispatchVehicles(Simulation *simulation, int incidentIndex,
                               unsigned int vehicleMask)
{
    if (!simulation || incidentIndex < 0 || incidentIndex >= simulation->incidentCount ||
        !vehicleMask) return 0;
    Incident *incident = &simulation->incidents[incidentIndex];
    if (incident->callIndex >= 0 || incident->status == INCIDENT_RESOLVED ||
        incident->status == INCIDENT_FAILED) return 0;

    int selectedByService[SERVICE_COUNT] = {0};
    int remainingByService[SERVICE_COUNT] = {0};
    int assigned = 0;
    unsigned int validMask = (1u << simulation->vehicleCount) - 1u;
    if (vehicleMask & ~validMask) return 0;
    for (int service = 0; service < SERVICE_COUNT; service++) {
        remainingByService[service] = incident->requiredUnitsByService[service] -
            CountAssignedUnitsByService(simulation, incidentIndex, (IncidentType)service);
        if (remainingByService[service] < 0) return 0;
    }
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (!(vehicleMask & (1u << i))) continue;
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (!vehicle->available || vehicle->serviceType < INCIDENT_FIRE ||
            vehicle->serviceType >= SERVICE_COUNT) return 0;
        selectedByService[vehicle->serviceType]++;
    }
    for (int service = 0; service < SERVICE_COUNT; service++)
        if (selectedByService[service] > remainingByService[service]) return 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (vehicleMask & (1u << i)) {
            AssignVehicle(simulation, i, incidentIndex);
            assigned++;
        }
    if (assigned > 0) incident->status = INCIDENT_DISPATCHED;
    return assigned;
}



static void UpdateEscalation(Simulation *simulation, int i)

{

    Incident *in = &simulation->incidents[i];

    if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) return;



    // Severity increases every 10 seconds spent waiting for enough units.

    int newSeverity = in->waitTime >= 20.0f ? 3 :

                      in->waitTime >= 10.0f ? 2 : 1;

    if (newSeverity > in->severity) {

        in->severity = newSeverity;

        ConfigureIncidentRequirements(in);
        if (in->callIndex >= 0 && in->callIndex < simulation->callCount)
            simulation->calls[in->callIndex].severity = in->severity;

    }

}


static void SetIncidentStage(Simulation *simulation, Incident *incident,
                             IncidentStatus status, const char *message)
{
    if (incident->status == status) return;
    incident->status = status;
    if (message) {
        char event[SIMULATION_EVENT_LENGTH];
        snprintf(event, sizeof(event), "Call #%02d: %s",
                 incident->callIndex >= 0 ? incident->callIndex + 1 :
                 (int)(incident - simulation->incidents) + 1, message);
        LogEvent(simulation, event);
    }
}

static void ResolveIncident(Simulation *simulation, int incidentIndex)
{
    Incident *incident = &simulation->incidents[incidentIndex];
    incident->status = INCIDENT_RESOLVED;
    RecordCallFeedback(simulation, incident);
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Call #%02d resolved: response stages complete.",
             incident->callIndex >= 0 ? incident->callIndex + 1 : incidentIndex + 1);
    LogEvent(simulation, message);
    IncidentType service = SimulationServiceForIncident(incident->type);
    simulation->resolvedByType[service]++;
    simulation->totalResponseTimeByType[service] += incident->responseTime;
    ReleaseIncidentVehicles(simulation, incidentIndex);
}

static const int apartmentFireBlockBuildings[MAX_FIRE_CASCADE_BUILDINGS] = {4, 6, 14, 13, 5, 3};
static const Vector2 apartmentFireBlockPositions[MAX_FIRE_CASCADE_BUILDINGS] = {
    {624, 131}, {663, 208}, {636, 430}, {518, 428}, {481, 210}, {508, 135}
};

static void SpreadApartmentFireToNextBuilding(Simulation *simulation, Incident *source)
{
    if (!simulation || !source || source->fireSpreadAttempted) return;
    source->fireSpreadAttempted = 1;
    source->fireSpreadOccurred = 1;

    int nextStep = source->fireCascadeStep + 1;
    if (nextStep < 1 || nextStep >= MAX_FIRE_CASCADE_BUILDINGS ||
        simulation->fireCascadeCount >= MAX_FIRE_CASCADE_BUILDINGS) {
        if (!simulation->fireCascadeLimitLogged) {
            LogEvent(simulation, "Fire crews are protecting the rest of the block; spread limit reached.");
            simulation->fireCascadeLimitLogged = 1;
        }
        return;
    }

    char report[CALL_REPORT_LENGTH];
    snprintf(report, sizeof(report),
             "Fire spreading to a neighboring building; flames are spreading and people may be trapped.");
    if (!SimulationCreateCall(simulation, apartmentFireBlockPositions[nextStep], report)) {
        LogEvent(simulation, "The block fire could not create another call because the call queue is full.");
        return;
    }

    int callIndex = simulation->callCount - 1;
    EmergencyCall *call = &simulation->calls[callIndex];
    call->fireBuildingIndex = apartmentFireBlockBuildings[nextStep];
    call->fireCascadeStep = nextStep;
    SimulationSetCallTruth(simulation, callIndex, INCIDENT_FIRE, 3);
    if (simulation->dispatchMode == DISPATCH_AUTOMATIC)
        call->overridePending = 1;
    simulation->fireCascadeCount++;

    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message),
             "Fire spread to neighboring building %d; a new high-severity call was created.",
             simulation->fireCascadeCount);
    LogEvent(simulation, message);
}

static void UpdateIncidentResponseStage(Simulation *simulation, int incidentIndex,
                                        float deltaTime)
{
    Incident *incident = &simulation->incidents[incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED) return;

    if (simulation->apartmentFireDrill && incident->fireBuildingIndex >= 0 &&
        incident->fireCascadeStep >= 0 &&
        incident->type == INCIDENT_FIRE && !incident->fireContained &&
        !incident->primaryWorkComplete) {
        incident->fireRiskElapsed += deltaTime;

        // Track a simple six-room fire front. Severity and wind determine how
        // quickly it advances; the visual state is driven by the same count.
        float roomSpreadInterval = incident->severity >= 3 ? 2.6f :
                                   incident->severity == 2 ? 3.2f : 4.0f;
        if (simulation->cityCondition == CITY_WIND) roomSpreadInterval *= 0.78f;
        int roomsReached = 1 + (int)(incident->fireRiskElapsed / roomSpreadInterval);
        if (roomsReached > APARTMENT_FIRE_ROOM_COUNT)
            roomsReached = APARTMENT_FIRE_ROOM_COUNT;
        while (incident->fireRoomsBurning < roomsReached) {
            incident->fireRoomsBurning++;
            char message[SIMULATION_EVENT_LENGTH];
            snprintf(message, sizeof(message), "Apartment fire spread to room %d of %d.",
                     incident->fireRoomsBurning, APARTMENT_FIRE_ROOM_COUNT);
            LogEvent(simulation, message);
        }

        // Neighboring buildings are only threatened by a critical fire that
        // remains uncontrolled. Wind makes that danger develop sooner.
        float neighborSpreadTime = simulation->cityCondition == CITY_WIND ? 6.0f : 12.0f;
        int neighborThresholdRooms = simulation->cityCondition == CITY_WIND ? 3 : 5;
        if (incident->severity >= 3 && !incident->fireSpreadAttempted &&
            incident->fireRoomsBurning >= neighborThresholdRooms &&
            incident->fireRiskElapsed >= neighborSpreadTime) {
            SpreadApartmentFireToNextBuilding(simulation, incident);
        }
    }

    if (incident->needsSceneSecurity && !incident->sceneSecured) {
        int policeRequired = incident->requiredUnitsByService[INCIDENT_CRIME];
        if (CountArrivedUnitsByService(simulation, incidentIndex, INCIDENT_CRIME) < policeRequired) {
            incident->status = CountAssignedUnits(simulation, incidentIndex) > 0 ?
                               INCIDENT_DISPATCHED : INCIDENT_PENDING;
            return;
        }
        SetIncidentStage(simulation, incident, INCIDENT_SECURING,
                         "Police are securing the scene.");
        incident->workElapsed += deltaTime;
        float securityDuration = 1.2f + incident->severity * 0.65f;
        if (incident->workElapsed >= securityDuration) {
            incident->sceneSecured = 1;
            incident->workElapsed = 0.0f;
            LogEvent(simulation, "Police completed the area security stage.");
        }
        return;
    }

    IncidentType primaryService = SimulationServiceForIncident(incident->type);
    int primaryRequired = incident->requiredUnitsByService[primaryService];
    if (!incident->primaryWorkComplete) {
        if (CountArrivedUnitsByService(simulation, incidentIndex, primaryService) < primaryRequired) {
            incident->status = CountAssignedUnits(simulation, incidentIndex) > 0 ?
                               INCIDENT_DISPATCHED : INCIDENT_PENDING;
            return;
        }

        IncidentStatus stageStatus = primaryService == INCIDENT_FIRE ? INCIDENT_FIREFIGHTING :
                                     primaryService == INCIDENT_ACCIDENT ? INCIDENT_TREATING :
                                     INCIDENT_RESPONDING;
        const char *startMessage = primaryService == INCIDENT_FIRE ?
            "Fire crews are setting up and fighting the fire." :
            primaryService == INCIDENT_ACCIDENT ? "Ambulance crew is treating the patient." :
            "Police are handling the incident.";
        SetIncidentStage(simulation, incident, stageStatus, startMessage);
        incident->workElapsed += deltaTime;
        float duration = primaryService == INCIDENT_ACCIDENT ?
                         2.0f + incident->severity * 1.1f : incident->workRequired;
        if (primaryService == INCIDENT_FIRE && !incident->fireContained &&
            incident->workElapsed >= duration * 0.4f) {
            incident->fireContained = 1;
            LogEvent(simulation, incident->fireSpreadOccurred ?
                     "Fire crews contained the spread; nearby buildings are protected." :
                     "Fire crews contained the apartment fire before it spread.");
        }
        if (incident->workElapsed >= duration) {
            incident->primaryWorkComplete = 1;
            if (primaryService == INCIDENT_FIRE) {
                if (!incident->fireContained) {
                    incident->fireContained = 1;
                    LogEvent(simulation, incident->fireSpreadOccurred ?
                             "Fire crews contained the spread; nearby buildings are protected." :
                             "Fire crews contained the apartment fire before it spread.");
                }
            }
            if (primaryService == INCIDENT_ACCIDENT) incident->patientCareComplete = 1;
            incident->workElapsed = 0.0f;
            LogEvent(simulation, primaryService == INCIDENT_FIRE ?
                     "Firefighting stage complete." : primaryService == INCIDENT_ACCIDENT ?
                     "Initial patient treatment complete." : "Primary response stage complete.");
        }
        return;
    }

    int ambulanceRequired = incident->requiredUnitsByService[INCIDENT_ACCIDENT];
    if (ambulanceRequired > 0 && !incident->patientCareComplete) {
        if (CountArrivedUnitsByService(simulation, incidentIndex, INCIDENT_ACCIDENT) <
            ambulanceRequired) {
            incident->status = INCIDENT_DISPATCHED;
            return;
        }
        SetIncidentStage(simulation, incident, INCIDENT_TREATING,
                         "Ambulance crew is treating the patient.");
        incident->workElapsed += deltaTime;
        if (incident->workElapsed >= 2.0f + incident->severity * 1.1f) {
            incident->patientCareComplete = 1;
            incident->workElapsed = 0.0f;
            LogEvent(simulation, "Patient care stage complete.");
        }
        return;
    }

    if (incident->requiresTransport && !incident->transportComplete) {
        int enteringTransport = incident->status != INCIDENT_TRANSPORTING;
        SetIncidentStage(simulation, incident, INCIDENT_TRANSPORTING,
                         "Ambulance is transporting the patient to hospital.");
        if (enteringTransport) {
            for (int v = 0; v < simulation->vehicleCount; v++) {
                Vehicle *vehicle = &simulation->vehicles[v];
                if (vehicle->assignedIncident == incidentIndex &&
                    vehicle->serviceType == INCIDENT_ACCIDENT && vehicle->arrived) {
                    vehicle->activity = UNIT_TRANSPORTING;
                    vehicle->patientDelivered = 0;
                    BuildVehicleRoute(vehicle, simulation->hospitalPosition);
                }
            }
        }
        int ambulances = 0, delivered = 0;
        for (int v = 0; v < simulation->vehicleCount; v++) {
            const Vehicle *vehicle = &simulation->vehicles[v];
            if (vehicle->assignedIncident == incidentIndex &&
                vehicle->serviceType == INCIDENT_ACCIDENT) {
                ambulances++;
                delivered += vehicle->patientDelivered;
            }
        }
        if (ambulances > 0 && delivered == ambulances) {
            incident->transportComplete = 1;
            LogEvent(simulation, "Patient delivered to hospital; ambulance returning to station.");
        }
        return;
    }

    ResolveIncident(simulation, incidentIndex);
}



typedef struct {
    const Vector2 *points;
    int count;
} PedestrianRoute;

static const Vector2 pedestrianRoutes[6][13] = {
    {{20,282},{345,282},{345,80},{345,282},{345,370},{20,370},{20,282}},
    {{453,282},{714,282},{720,282},{720,80},{720,282},{720,370},{714,370},{453,370},{447,370},{447,282},{453,282}},
    {{828,282},{1094,282},{1094,80},{1094,282},{822,282},{822,370},{828,370},{1094,370},{1094,630},{1094,370},{822,370},{822,282},{828,282}},
    {{20,370},{345,370},{345,630},{345,370},{345,282},{20,282},{20,370}},
    {{453,370},{714,370},{720,370},{720,630},{720,370},{720,282},{714,282},{453,282},{447,282},{447,370},{453,370}},
    {{828,370},{1094,370},{1094,630},{1094,370},{822,370},{822,282},{828,282},{1094,282},{1094,80},{1094,282},{822,282},{822,370},{828,370}}
};

static const int pedestrianRouteCounts[6] = {7,11,13,7,11,13};

static PedestrianRoute GetPedestrianRoute(int sidewalk)
{
    int index = sidewalk % 6;
    return (PedestrianRoute){pedestrianRoutes[index], pedestrianRouteCounts[index]};
}

static int IsPedestrianCrosswalk(Vector2 a, Vector2 b)
{
    if (fabsf(a.y - b.y) < 0.5f &&
        (fabsf(a.y - 282.0f) < 1.0f || fabsf(a.y - 370.0f) < 1.0f) &&
        fminf(a.x, b.x) <= 1065.0f && fmaxf(a.x, b.x) >= 1065.0f) return 1;
    if (fabsf(a.x - b.x) > 0.5f || fabsf(a.y - b.y) < 40.0f) return 0;
    if (a.y > b.y) { Vector2 swap = a; a = b; b = swap; }
    if (a.y > 282.0f || b.y < 370.0f) return 0;
    return fabsf(a.x - 345.0f) < 1.0f || fabsf(a.x - 447.0f) < 1.0f ||
           fabsf(a.x - 720.0f) < 1.0f || fabsf(a.x - 822.0f) < 1.0f;
}

static Vector2 PedestrianRoutePosition(const Pedestrian *person)
{
    PedestrianRoute route = GetPedestrianRoute(person->sidewalk);
    Vector2 a = route.points[person->routeIndex];
    Vector2 b = route.points[person->routeIndex + 1];
    return (Vector2){a.x + (b.x - a.x) * person->routeProgress,
                     a.y + (b.y - a.y) * person->routeProgress};
}

static void LogPedestrianReaction(Simulation *simulation, int index, unsigned int bit,
                                 const char *description)
{
    if (index < 0 || index >= simulation->incidentCount) return;
    Incident *incident = &simulation->incidents[index];
    if (incident->pedestrianReactionMask & bit) return;
    incident->pedestrianReactionMask |= bit;
    char message[SIMULATION_EVENT_LENGTH];
    snprintf(message, sizeof(message), "Pedestrians %s near call #%02d.", description,
             incident->callIndex >= 0 ? incident->callIndex + 1 : index + 1);
    LogEvent(simulation, message);
}

static int PedestrianCrossingClear(const Simulation *simulation, float x)
{
    for (int i = 0; i < simulation->civilianCount; i++) {
        const CivilianCar *car = &simulation->civilianCars[i];
        if (fabsf(car->position.x - x) < 30.0f && car->position.y > 282.0f &&
            car->position.y < 370.0f) return 0;
    }
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (vehicle->activity == UNIT_AVAILABLE || vehicle->activity == UNIT_ON_SCENE ||
            vehicle->activity == UNIT_RETURNING || vehicle->activity == UNIT_TRANSPORTING)
            continue;
        if (fabsf(vehicle->position.x - x) < 34.0f && vehicle->position.y > 275.0f &&
            vehicle->position.y < 378.0f) return 0;
    }
    return 1;
}

static int NearestHazard(const Simulation *simulation, Vector2 position, float limit)
{
    int bestIndex = -1;
    float bestDistance = limit;
    for (int i = 0; i < simulation->incidentCount; i++) {
        const Incident *incident = &simulation->incidents[i];
        if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED)
            continue;
        if (incident->type != INCIDENT_FIRE && incident->type != INCIDENT_ACCIDENT &&
            !(incident->severity >= 3 && incident->needsSceneSecurity)) continue;
        float distance = DistanceBetween(position, incident->position);
        if (distance < bestDistance) { bestDistance = distance; bestIndex = i; }
    }
    return bestIndex;
}

static int PedestrianAtRouteNode(const Pedestrian *person, int node)
{
    if (person->routeIndex == node && person->routeProgress < 0.015f) return 1;
    return person->routeIndex + 1 == node && person->routeProgress > 0.985f;
}

static float PedestrianDistanceToNode(const Pedestrian *person, PedestrianRoute route, int node)
{
    float total = 0.0f;
    if (person->direction > 0) {
        total += (1.0f - person->routeProgress) *
                 DistanceBetween(route.points[person->routeIndex], route.points[person->routeIndex + 1]);
        for (int i = person->routeIndex + 1; i < node; i++)
            total += DistanceBetween(route.points[i], route.points[i + 1]);
    } else {
        total += person->routeProgress *
                 DistanceBetween(route.points[person->routeIndex], route.points[person->routeIndex + 1]);
        for (int i = person->routeIndex; i > node; i--)
            total += DistanceBetween(route.points[i], route.points[i - 1]);
    }
    return total;
}

static void AdvancePedestrianRoute(const Simulation *simulation, Pedestrian *person,
                                   float distance)
{
    PedestrianRoute route = GetPedestrianRoute(person->sidewalk);
    int guard = 0;
    while (distance > 0.0f && guard++ < 32) {
        Vector2 a = route.points[person->routeIndex];
        Vector2 b = route.points[person->routeIndex + 1];
        float segmentLength = DistanceBetween(a, b);
        float available = person->direction > 0 ?
                          (1.0f - person->routeProgress) * segmentLength :
                          person->routeProgress * segmentLength;
        if (available < 0.01f) {
            if (person->direction > 0) {
                if (person->routeIndex + 2 >= route.count) {
                    person->direction = -1;
                    person->routeProgress = 1.0f;
                } else { person->routeIndex++; person->routeProgress = 0.0f; }
            } else if (person->routeIndex == 0) {
                person->direction = 1;
                person->routeProgress = 0.0f;
            } else { person->routeIndex--; person->routeProgress = 1.0f; }
            person->crossingActive = 0;
            continue;
        }
        int crossing = IsPedestrianCrosswalk(a, b);
        if (crossing && !person->crossingActive) {
            float crossingX = fabsf(a.y - b.y) < 0.5f ? 1065.0f : a.x;
            if (!PedestrianCrossingClear(simulation, crossingX)) {
                person->state = PEDESTRIAN_WAITING_TO_CROSS;
                person->position = PedestrianRoutePosition(person);
                return;
            }
            person->crossingActive = 1;
            person->state = PEDESTRIAN_CROSSING;
        }
        float used = distance < available ? distance : available;
        person->routeProgress += person->direction > 0 ? used / segmentLength : -used / segmentLength;
        distance -= used;
        person->position = PedestrianRoutePosition(person);
        if (crossing && person->crossingActive) person->state = PEDESTRIAN_CROSSING;
        if (used + 0.001f >= available) {
            if (crossing) person->crossingActive = 0;
            if (person->direction > 0) {
                if (person->routeIndex + 2 >= route.count) {
                    person->direction = -1;
                    person->routeProgress = 1.0f;
                } else { person->routeIndex++; person->routeProgress = 0.0f; }
            } else if (person->routeIndex == 0) {
                person->direction = 1;
                person->routeProgress = 0.0f;
            } else { person->routeIndex--; person->routeProgress = 1.0f; }
        }
    }
    person->position = PedestrianRoutePosition(person);
}

static void UpdatePedestrians(Simulation *simulation, float deltaTime)
{
    for (int p = 0; p < simulation->pedestrianCount; p++) {
        Pedestrian *person = &simulation->pedestrians[p];
        PedestrianRoute route = GetPedestrianRoute(person->sidewalk);
        int behaviorIncident = -1;
        int hazard = NearestHazard(simulation, person->position, 180.0f);
        if (hazard >= 0) {
            behaviorIncident = hazard;
            person->state = PEDESTRIAN_EVACUATING;
            person->evacuating = 1;
            Vector2 tangent = {route.points[person->routeIndex + 1].x - route.points[person->routeIndex].x,
                               route.points[person->routeIndex + 1].y - route.points[person->routeIndex].y};
            Vector2 away = {person->position.x - simulation->incidents[hazard].position.x,
                            person->position.y - simulation->incidents[hazard].position.y};
            person->direction = tangent.x * away.x + tangent.y * away.y >= 0.0f ? 1 : -1;
            person->speed = 32.0f;
            LogPedestrianReaction(simulation, hazard, 1u, "are moving away from danger");
        } else {
            int cordon = -1;
            float cordonDistance = 68.0f;
            for (int i = 0; i < simulation->incidentCount; i++) {
                Incident *incident = &simulation->incidents[i];
                if (!incident->needsSceneSecurity || !incident->sceneSecured ||
                    incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED)
                    continue;
                float d = DistanceBetween(person->position, incident->position);
                if (d < cordonDistance) { cordonDistance = d; cordon = i; }
            }
            if (cordon >= 0) {
                behaviorIncident = cordon;
                person->state = PEDESTRIAN_RESPECTING_CORDON;
                person->evacuating = 0;
                Vector2 tangent = {route.points[person->routeIndex + 1].x - route.points[person->routeIndex].x,
                                   route.points[person->routeIndex + 1].y - route.points[person->routeIndex].y};
                Vector2 away = {person->position.x - simulation->incidents[cordon].position.x,
                                person->position.y - simulation->incidents[cordon].position.y};
                person->direction = tangent.x * away.x + tangent.y * away.y >= 0.0f ? 1 : -1;
                person->speed = 24.0f;
                LogPedestrianReaction(simulation, cordon, 2u, "are keeping clear of the police cordon");
            } else {
                int emergency = -1;
                float emergencyX = person->position.x;
                float nearest = 48.0f;
                for (int v = 0; v < simulation->vehicleCount; v++) {
                    Vehicle *vehicle = &simulation->vehicles[v];
                    if (vehicle->activity != UNIT_RESPONDING || vehicle->arrived ||
                        vehicle->routeCount == 0 || vehicle->assignedIncident < 0) continue;
                    float d = DistanceBetween(person->position, vehicle->position);
                    if (d < nearest) { nearest = d; emergency = vehicle->assignedIncident; emergencyX = vehicle->position.x; }
                }
                if (emergency >= 0) {
                    behaviorIncident = emergency;
                    person->state = PEDESTRIAN_YIELDING;
                    person->evacuating = 0;
                    Vector2 tangent = {route.points[person->routeIndex + 1].x - route.points[person->routeIndex].x,
                                       route.points[person->routeIndex + 1].y - route.points[person->routeIndex].y};
                    person->direction = tangent.x * (person->position.x - emergencyX) >= 0.0f ? 1 : -1;
                    person->speed = 28.0f;
                    LogPedestrianReaction(simulation, emergency, 4u,
                                          "are stepping aside for an emergency vehicle");
                } else {
                    int bystander = -1;
                    float nearestIncident = 112.0f;
                    for (int i = 0; i < simulation->incidentCount; i++) {
                        Incident *incident = &simulation->incidents[i];
                        if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED ||
                            incident->type == INCIDENT_FIRE || incident->type == INCIDENT_ACCIDENT ||
                            (incident->type != INCIDENT_MEDICAL && incident->severity > 1)) continue;
                        int gathered = 0;
                        for (int j = 0; j < simulation->pedestrianCount; j++)
                            if (j != p && simulation->pedestrians[j].state == PEDESTRIAN_BYSTANDING &&
                                simulation->pedestrians[j].behaviorIncident == i) gathered++;
                        if (gathered >= 3) continue;
                        float d = DistanceBetween(person->position, incident->position);
                        if (d < nearestIncident) { nearestIncident = d; bystander = i; }
                    }
                    if (bystander >= 0) {
                        Incident *incident = &simulation->incidents[bystander];
                        int targetNode = -1;
                        float targetDistance = 100.0f;
                        for (int n = 0; n < route.count; n++) {
                            float d = DistanceBetween(route.points[n], incident->position);
                            if (d > 42.0f && d < targetDistance) {
                                targetDistance = d;
                                targetNode = n;
                            }
                        }
                        if (targetNode >= 0) {
                            behaviorIncident = bystander;
                            person->state = PEDESTRIAN_BYSTANDING;
                            person->behaviorIncident = bystander;
                            person->targetNode = targetNode;
                            person->direction = (float)targetNode >=
                                (float)person->routeIndex + person->routeProgress ? 1 : -1;
                            person->evacuating = 0;
                            person->speed = 20.0f;
                            LogPedestrianReaction(simulation, bystander, 8u,
                                                  "are gathering at a safe distance");
                        }
                    }
                    if (behaviorIncident < 0) {
                        person->state = PEDESTRIAN_WALKING;
                        person->behaviorIncident = -1;
                        person->evacuating = 0;
                        person->speed = 18.0f + (float)(p % 5) * 2.3f;
                    }
                }
            }
        }
        if (behaviorIncident >= 0) person->behaviorIncident = behaviorIncident;
        if (person->state == PEDESTRIAN_BYSTANDING &&
            PedestrianAtRouteNode(person, person->targetNode)) continue;
        if (person->state != PEDESTRIAN_BYSTANDING) person->targetNode = -1;
        float walkDistance = person->speed * deltaTime;
        if (person->state == PEDESTRIAN_BYSTANDING && person->targetNode >= 0) {
            float toTarget = PedestrianDistanceToNode(person, route, person->targetNode);
            if (walkDistance > toTarget) walkDistance = toTarget;
        }
        AdvancePedestrianRoute(simulation, person, walkDistance);
        if (person->state == PEDESTRIAN_BYSTANDING &&
            PedestrianAtRouteNode(person, person->targetNode)) person->position = route.points[person->targetNode];
    }
}

void SimulationUpdate(Simulation *simulation, float deltaTime)

{

    if (!simulation || !isfinite(deltaTime) || deltaTime <= 0.0f) return;

    simulation->elapsedTime += deltaTime;

    GenerateAutoIncidentIfDue(simulation);

    UpdateCivilianTraffic(simulation, deltaTime);
    UpdatePedestrians(simulation, deltaTime);



    // Time advances for every non-terminal incident.

    for (int i = 0; i < simulation->incidentCount; i++) {

        Incident *in = &simulation->incidents[i];

        if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;



        in->ageTime += deltaTime;

        if (CountArrivedUnits(simulation, i) < CountRequiredUnits(in)) {

            in->waitTime += deltaTime;

        }

        if (CountAssignedUnits(simulation, i) > 0) in->travelTime += deltaTime;



        UpdateEscalation(simulation, i);

        if (in->ageTime >= in->deadline) {

            FailIncident(simulation, i);

        }

    }



    DispatchAvailableUnits(simulation);
    SimulationProcessOperator(simulation);



    // Move response, hospital-transport, and return journeys on the same road graph.
    for (int vIndex = 0; vIndex < simulation->vehicleCount; vIndex++) {
        Vehicle *v = &simulation->vehicles[vIndex];
        if (v->activity == UNIT_AVAILABLE && !v->available && v->assignedIncident >= 0)
            v->activity = UNIT_RESPONDING;
        if (v->activity == UNIT_AVAILABLE || v->activity == UNIT_ON_SCENE ||
            v->routeCount == 0) continue;
        int i = v->assignedIncident;
        Incident *in = (i >= 0 && i < simulation->incidentCount) ? &simulation->incidents[i] : NULL;
        Vector2 destination = v->activity == UNIT_RETURNING ? v->stationPosition :
                              v->activity == UNIT_TRANSPORTING ? simulation->hospitalPosition :
                              in ? in->position : v->position;
        float timeLeft = deltaTime;
        while (v->routeIndex < v->routeCount && timeLeft > 0.0f) {
            Vector2 waypoint = v->route[v->routeIndex];
            float distance = DistanceBetween(v->position, waypoint);
            float trafficFactor = EmergencyVehicleTrafficFactor(simulation, v);
            float speed = v->speed * trafficFactor;
            if (in && trafficFactor < 0.999f && !v->trafficDelayLogged) {
                char message[SIMULATION_EVENT_LENGTH];
                snprintf(message, sizeof(message), "%s unit %d slowed by traffic en route to call #%02d.",
                         ServiceEventName(v->serviceType), ServiceUnitNumber(simulation, vIndex),
                         in->callIndex >= 0 ? in->callIndex + 1 : i + 1);
                LogEvent(simulation, message);
                v->trafficDelayLogged = 1;
            }
            if (distance < 0.01f) { v->position = waypoint; v->routeIndex++; }
            else if (speed * timeLeft >= distance) {
                v->position = waypoint; timeLeft -= distance / speed; v->routeIndex++;
            } else {
                float moved = speed * timeLeft;
                v->position.x += ((waypoint.x - v->position.x) / distance) * moved;
                v->position.y += ((waypoint.y - v->position.y) / distance) * moved;
                timeLeft = 0.0f;
            }
        }
        if (v->routeIndex < v->routeCount || DistanceBetween(v->position, destination) >= 0.1f) continue;
        v->position = destination;
        if (v->activity == UNIT_RETURNING) {
            v->activity = UNIT_AVAILABLE;
            v->available = 1;
            v->routeCount = 0;
            v->routeIndex = 0;
            LogEvent(simulation, "Emergency unit returned to its station and is available.");
            continue;
        }
        if (v->activity == UNIT_TRANSPORTING) {
            v->patientDelivered = 1;
            v->activity = UNIT_RETURNING;
            BuildVehicleRoute(v, v->stationPosition);
            LogEvent(simulation, "Ambulance delivered the patient and is returning to station.");
            continue;
        }
        if (!in || in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;
        if (in->callIndex >= 0 && in->callIndex < simulation->callCount) {
            EmergencyCall *call = &simulation->calls[in->callIndex];
            if (call->operatorOverride && call->operatorWrongService && !call->mismatchRecorded) {
                char message[SIMULATION_EVENT_LENGTH];
                snprintf(message, sizeof(message), "%s unit %d reached call #%02d; service mismatch recorded.",
                         ServiceEventName(v->serviceType), ServiceUnitNumber(simulation, vIndex), in->callIndex + 1);
                LogEvent(simulation, message);
                call->mismatchRecorded = 1;
                for (int release = 0; release < simulation->vehicleCount; release++) {
                    Vehicle *wrongUnit = &simulation->vehicles[release];
                    if (wrongUnit->assignedIncident != i) continue;
                    wrongUnit->available = 1;
                    wrongUnit->assignedIncident = -1;
                    wrongUnit->arrived = 0;
                    wrongUnit->activity = UNIT_AVAILABLE;
                    wrongUnit->routeCount = 0;
                    wrongUnit->routeIndex = 0;
                }
                call->dispatched = 0;
                if (SimulationClassifyCall(simulation, in->callIndex, call->trueType, call->trueSeverity))
                    SimulationProcessOperator(simulation);
                continue;
            }
        }
        v->arrived = 1;
        v->activity = UNIT_ON_SCENE;
        char message[SIMULATION_EVENT_LENGTH];
        snprintf(message, sizeof(message), "%s unit %d arrived at call #%02d.",
                 ServiceEventName(v->serviceType), ServiceUnitNumber(simulation, vIndex),
                 in->callIndex >= 0 ? in->callIndex + 1 : i + 1);
        LogEvent(simulation, message);
        if (!in->responseRecorded) { in->responseTime = in->ageTime; in->responseRecorded = 1; }
    }

    for (int i = 0; i < simulation->incidentCount; i++)
        UpdateIncidentResponseStage(simulation, i, deltaTime);



    DispatchAvailableUnits(simulation);
    SimulationProcessOperator(simulation);

}



void SimulationSimulateQueue(Simulation *simulation)

{

    int steps = 0;

    const int maxSteps = 200000;

    while (SimulationCountUnresolved(simulation) > 0 && steps < maxSteps) {

        SimulationUpdate(simulation, 0.1f);

        steps++;

    }

}



static int AddScenarioIncident(Simulation *simulation, IncidentType type,
                               Vector2 position, int severity)
{
    if (!SimulationCreateIncident(simulation, type, position)) return 0;
    Incident *incident = &simulation->incidents[simulation->incidentCount - 1];
    incident->severity = severity;
    ConfigureIncidentRequirements(incident);
    return 1;
}


static void CreateComparisonScenario(Simulation *simulation)
{
    // Four fire calls compete for three units; the critical call is deliberately
    // last so the strategies make different initial dispatch choices.
    AddScenarioIncident(simulation, INCIDENT_FIRE, (Vector2){110, 160}, 1);
    AddScenarioIncident(simulation, INCIDENT_FIRE, (Vector2){570, 500}, 1);
    AddScenarioIncident(simulation, INCIDENT_FIRE, (Vector2){950, 500}, 1);
    AddScenarioIncident(simulation, INCIDENT_FIRE, (Vector2){315, 245}, 3);
    simulation->incidents[simulation->incidentCount - 1].deadline = 12.0f;

    AddScenarioIncident(simulation, INCIDENT_CRIME, (Vector2){130, 465}, 1);
    AddScenarioIncident(simulation, INCIDENT_CRIME, (Vector2){940, 460}, 1);
    AddScenarioIncident(simulation, INCIDENT_CRIME, (Vector2){560, 160}, 3);

    AddScenarioIncident(simulation, INCIDENT_ACCIDENT, (Vector2){140, 350}, 1);
    AddScenarioIncident(simulation, INCIDENT_MEDICAL, (Vector2){565, 350}, 1);
    AddScenarioIncident(simulation, INCIDENT_ACCIDENT, (Vector2){940, 350}, 1);
    AddScenarioIncident(simulation, INCIDENT_MEDICAL, (Vector2){940, 140}, 1);
    AddScenarioIncident(simulation, INCIDENT_MEDICAL, (Vector2){150, 160}, 3);
}


static StrategyResult GetStrategyResult(const Simulation *simulation)
{
    StrategyResult result = {0};
    result.resolved = SimulationCountStatus(simulation, INCIDENT_RESOLVED);
    result.failed = SimulationCountStatus(simulation, INCIDENT_FAILED);

    int responseCount = 0;
    for (int i = 0; i < SERVICE_COUNT; i++) {
        responseCount += simulation->resolvedByType[i];
        result.averageResponseTime += simulation->totalResponseTimeByType[i];
    }
    if (responseCount > 0) result.averageResponseTime /= (float)responseCount;
    return result;
}


void SimulationRunStrategyComparison(StrategyComparison *comparison)
{
    if (!comparison) return;

    Simulation base;
    SimulationInit(&base);
    CreateComparisonScenario(&base);

    // Both runs start with identical incidents, vehicles, availability, and stats.
    Simulation nearestFirst = base;
    Simulation priority = base;
    nearestFirst.dispatchStrategy = DISPATCH_NEAREST_FIRST;
    priority.dispatchStrategy = DISPATCH_PRIORITY;

    SimulationSimulateQueue(&nearestFirst);
    SimulationSimulateQueue(&priority);

    comparison->nearestFirst = GetStrategyResult(&nearestFirst);
    comparison->priority = GetStrategyResult(&priority);
}


int SimulationCountStatus(const Simulation *simulation, IncidentStatus status)

{

    int count = 0;

    for (int i = 0; i < simulation->incidentCount; i++)

        if (simulation->incidents[i].status == status) count++;

    return count;

}



int SimulationCountInProgress(const Simulation *simulation)

{

    return SimulationCountStatus(simulation, INCIDENT_DISPATCHED) +
           SimulationCountStatus(simulation, INCIDENT_RESPONDING) +
           SimulationCountStatus(simulation, INCIDENT_SECURING) +
           SimulationCountStatus(simulation, INCIDENT_FIREFIGHTING) +
           SimulationCountStatus(simulation, INCIDENT_TREATING) +
           SimulationCountStatus(simulation, INCIDENT_TRANSPORTING);

}



int SimulationCountAvailableVehicles(const Simulation *simulation, IncidentType serviceType)

{

    int count = 0;

    for (int i = 0; i < simulation->vehicleCount; i++)

        if (simulation->vehicles[i].serviceType == serviceType && simulation->vehicles[i].available) count++;

    return count;

}



int SimulationCountUnresolved(const Simulation *simulation)

{

    int count = 0;

    for (int i = 0; i < simulation->incidentCount; i++) {

        IncidentStatus s = simulation->incidents[i].status;

        if (s != INCIDENT_RESOLVED && s != INCIDENT_FAILED) count++;

    }

    return count;

}
