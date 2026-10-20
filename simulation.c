#include "simulation.h"

#include <math.h>
#include <stddef.h>
#include <ctype.h>
#include <stdio.h>

#define ROAD_NODE_COUNT 8
#define ROAD_EDGE_COUNT 7

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

typedef struct {
    int from;
    int to;
} RoadEdge;

// Road centers match the roads drawn in main.c: a central east-west road and
// two north-south roads joined at their intersections.
static const Vector2 roadNodes[ROAD_NODE_COUNT] = {
    {0, 326}, {396, 326}, {771, 326}, {1100, 326},
    {396, 80}, {396, 630}, {771, 80}, {771, 630}
};

static const RoadEdge roadEdges[ROAD_EDGE_COUNT] = {
    {0, 1}, {1, 2}, {2, 3}, {1, 4}, {1, 5}, {2, 6}, {2, 7}
};



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
    float offset = 15.0f + shoulderOffset;
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

    IncidentType service = SimulationServiceForIncident(incident->type);
    float estimates[MAX_VEHICLES];
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (vehicle->serviceType != service) continue;
        if (vehicle->assignedIncident == incidentIndex) {
            if (vehicle->arrived) estimates[count++] = 0.0f;
            else estimates[count++] = EstimateRouteETA(simulation, vehicle, vehicle->routeIndex);
        } else if (vehicle->available) {
            estimates[count++] = EstimateVehicleETA(simulation, vehicle, incident->position);
        }
    }
    if (count < incident->requiredUnits) return -1.0f;
    for (int i = 0; i < count; i++)
        for (int j = i + 1; j < count; j++)
            if (estimates[j] < estimates[i]) {
                float temp = estimates[i]; estimates[i] = estimates[j]; estimates[j] = temp;
            }
    return estimates[incident->requiredUnits - 1];
}

float SimulationEstimateUnitETA(const Simulation *simulation, int vehicleIndex,
                                int incidentIndex)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount ||
        incidentIndex < 0 || incidentIndex >= simulation->incidentCount) return -1.0f;
    const Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    const Incident *incident = &simulation->incidents[incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED ||
        vehicle->serviceType != SimulationServiceForIncident(incident->type)) return -1.0f;
    if (vehicle->assignedIncident == incidentIndex) {
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



int CountArrivedUnits(const Simulation *simulation, int incidentIndex)

{

    int count = 0;

    for (int i = 0; i < simulation->vehicleCount; i++) {

        if (simulation->vehicles[i].assignedIncident == incidentIndex &&

            simulation->vehicles[i].arrived) count++;

    }

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

            v->available = 1;

            v->assignedIncident = -1;

            v->arrived = 0;
            v->routeCount = 0;
            v->routeIndex = 0;

        }

    }

}



static void FailIncident(Simulation *simulation, int incidentIndex)

{

    Incident *incident = &simulation->incidents[incidentIndex];

    if (incident->status == INCIDENT_RESOLVED ||

        incident->status == INCIDENT_FAILED) return;



    incident->status = INCIDENT_FAILED;

    simulation->failedByType[SimulationServiceForIncident(incident->type)]++;

    ReleaseIncidentVehicles(simulation, incidentIndex);

}



void SimulationInit(Simulation *simulation)

{

    simulation->incidentCount = 0;

    simulation->vehicleCount = 9;
    simulation->dispatchStrategy = DISPATCH_PRIORITY;
    simulation->dispatchMode = DISPATCH_AUTOMATIC;
    simulation->trafficLevel = TRAFFIC_LIGHT;
    simulation->civilianCount = 0;
    simulation->callCount = 0;
    simulation->elapsedTime = 0.0f;



    for (int i = 0; i < SERVICE_COUNT; i++) {

        simulation->dispatchedByType[i] = 0;

        simulation->resolvedByType[i] = 0;

        simulation->failedByType[i] = 0;

        simulation->totalResponseTimeByType[i] = 0.0f;

    }



    simulation->vehicles[0] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={300,270}, .stationPosition={300,270}, .speed=130.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[1] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={330,270}, .stationPosition={330,270}, .speed=130.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[2] = (Vehicle){.serviceType=INCIDENT_FIRE, .position={315,245}, .stationPosition={315,245}, .speed=130.0f, .available=1, .assignedIncident=-1};



    simulation->vehicles[3] = (Vehicle){.serviceType=INCIDENT_CRIME, .position={650,380}, .stationPosition={650,380}, .speed=145.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[4] = (Vehicle){.serviceType=INCIDENT_CRIME, .position={680,380}, .stationPosition={680,380}, .speed=145.0f, .available=1, .assignedIncident=-1};



    simulation->vehicles[5] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={770,270}, .stationPosition={770,270}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[6] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={800,270}, .stationPosition={800,270}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[7] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={770,300}, .stationPosition={770,300}, .speed=155.0f, .available=1, .assignedIncident=-1};

    simulation->vehicles[8] = (Vehicle){.serviceType=INCIDENT_ACCIDENT, .position={800,300}, .stationPosition={800,300}, .speed=155.0f, .available=1, .assignedIncident=-1};

    SimulationSetTrafficLevel(simulation, TRAFFIC_LIGHT);

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

    if (simulation->incidentCount >= MAX_INCIDENTS) return 0;



    Incident *in = &simulation->incidents[simulation->incidentCount];

    *in = (Incident){0};

    in->type = type;
    in->callIndex = -1;

    in->status = INCIDENT_PENDING;

    in->position = position;

    in->severity = 1;

    in->requiredUnits = GetRequiredUnits(type, in->severity);

    in->deadline = type == INCIDENT_FIRE ? 35.0f :

                   type == INCIDENT_CRIME ? 30.0f :

                   type == INCIDENT_MEDICAL ? 25.0f : 30.0f;

    in->workRequired = type == INCIDENT_FIRE ? 5.0f :

                       type == INCIDENT_CRIME ? 3.5f :

                       type == INCIDENT_MEDICAL ? 3.0f : 4.0f;

    simulation->incidentCount++;

    return 1;

}

int SimulationCreateCall(Simulation *simulation, Vector2 location, const char *report)
{
    if (!simulation || simulation->callCount >= MAX_CALLS || !report) return 0;
    EmergencyCall *call = &simulation->calls[simulation->callCount];
    *call = (EmergencyCall){0};
    call->location = location;
    call->receivedAt = simulation->elapsedTime;
    call->classifiedType = INCIDENT_FIRE;
    call->incidentIndex = -1;
    snprintf(call->report, sizeof(call->report), "%s", report);
    snprintf(call->decisionReason, sizeof(call->decisionReason),
             "Awaiting operator classification.");
    simulation->callCount++;
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
    incident->requiredUnits = GetRequiredUnits(type, severity);
    incident->deadline = type == INCIDENT_FIRE ? 35.0f :
                         type == INCIDENT_CRIME ? 30.0f :
                         type == INCIDENT_MEDICAL ? 25.0f : 30.0f;
    incident->workRequired = type == INCIDENT_FIRE ? 5.0f :
                             type == INCIDENT_CRIME ? 3.5f :
                             type == INCIDENT_MEDICAL ? 3.0f : 4.0f;
    call->classifiedType = type;
    call->severity = severity;
    call->classified = 1;
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

static void AssignVehicle(Simulation *simulation, int vehicleIndex, int incidentIndex)
{
    Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
    Incident *incident = &simulation->incidents[incidentIndex];
    IncidentType service = SimulationServiceForIncident(incident->type);
    vehicle->available = 0;
    vehicle->assignedIncident = incidentIndex;
    vehicle->arrived = 0;
    BuildVehicleRoute(vehicle, incident->position);
    simulation->dispatchedByType[service]++;
}

int SimulationDispatchCallVehicles(Simulation *simulation, int callIndex,
                                   unsigned int vehicleMask)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount || !vehicleMask)
        return 0;
    EmergencyCall *call = &simulation->calls[callIndex];
    if (!call->classified || call->dispatched || call->incidentIndex < 0 ||
        call->incidentIndex >= simulation->incidentCount) return 0;
    Incident *incident = &simulation->incidents[call->incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED)
        return 0;

    unsigned int validMask = (1u << simulation->vehicleCount) - 1u;
    if ((vehicleMask & ~validMask) != 0) return 0;
    int remaining = incident->requiredUnits - CountAssignedUnits(simulation, call->incidentIndex);
    int selected = 0;
    IncidentType service = SimulationServiceForIncident(incident->type);
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (!(vehicleMask & (1u << i))) continue;
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (!vehicle->available || vehicle->serviceType != service) return 0;
        selected++;
    }
    if (selected != remaining) return 0;

    for (int i = 0; i < simulation->vehicleCount; i++)
        if (vehicleMask & (1u << i)) AssignVehicle(simulation, i, call->incidentIndex);
    incident->status = INCIDENT_DISPATCHED;
    call->dispatched = 1;
    call->dispatchedAt = simulation->elapsedTime;
    snprintf(call->decisionReason, sizeof(call->decisionReason),
             "Manual operator dispatched %d suitable %s unit%s.", selected,
             SimulationServiceForIncident(incident->type) == INCIDENT_FIRE ? "fire" :
             SimulationServiceForIncident(incident->type) == INCIDENT_CRIME ? "police" : "ambulance",
             selected == 1 ? "" : "s");
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
        if (call->dispatched) continue;
        if (!call->classified) {
            IncidentType type;
            int severity;
            ClassifyCallFromReport(call, &type, &severity);
            float callAge = fmaxf(0.0f, simulation->elapsedTime - call->receivedAt);
            float deadline = type == INCIDENT_FIRE ? 35.0f :
                             type == INCIDENT_MEDICAL ? 25.0f : 30.0f;
            if (callAge >= deadline * 0.78f && severity < 3) severity = 3;
            else if (callAge >= deadline * 0.5f && severity < 2) severity = 2;
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
        IncidentType service = SimulationServiceForIncident(incident->type);
        int available = SimulationCountAvailableVehicles(simulation, service);
        int needed = incident->requiredUnits - CountAssignedUnits(simulation, call->incidentIndex);
        if (needed < 0) needed = 0;
        if (available < needed) {
            const char *level = incident->severity >= 3 ? "High" :
                                incident->severity == 2 ? "Medium" : "Low";
            snprintf(call->decisionReason, sizeof(call->decisionReason),
                     "%s-severity %s; %d/%d suitable units free. Waiting and retrying.",
                     level, incident->type == INCIDENT_FIRE ? "fire" :
                     incident->type == INCIDENT_CRIME ? "crime" :
                     incident->type == INCIDENT_ACCIDENT ? "road collision" : "medical call",
                     available, needed);
            continue;
        }

        unsigned int chosenMask = 0;
        for (int n = 0; n < needed; n++) {
            int best = -1;
            float bestEta = 1.0e30f;
            for (int i = 0; i < simulation->vehicleCount; i++) {
                Vehicle *vehicle = &simulation->vehicles[i];
                if (!vehicle->available || vehicle->serviceType != service ||
                    (chosenMask & (1u << i))) continue;
                float eta = EstimateVehicleETA(simulation, vehicle, incident->position);
                if (eta < bestEta) { bestEta = eta; best = i; }
            }
            if (best < 0) break;
            chosenMask |= 1u << best;
        }
        int chosenCount = 0;
        for (int i = 0; i < simulation->vehicleCount; i++)
            if (chosenMask & (1u << i)) chosenCount++;
        if (CountAssignedUnits(simulation, call->incidentIndex) + chosenCount <
            incident->requiredUnits) continue;

        for (int i = 0; i < simulation->vehicleCount; i++)
            if (chosenMask & (1u << i)) AssignVehicle(simulation, i, call->incidentIndex);
        incident->status = INCIDENT_DISPATCHED;
        call->dispatched = 1;
        call->dispatchedAt = simulation->elapsedTime;
        float eta = SimulationEstimateIncidentETA(simulation, call->incidentIndex);
        const char *level = incident->severity >= 3 ? "High" :
                            incident->severity == 2 ? "Medium" : "Low";
        snprintf(call->decisionReason, sizeof(call->decisionReason),
                 "%s-severity %s; sending %d units. ETA %.1fs in %s traffic.",
                 level, incident->type == INCIDENT_FIRE ? "fire" :
                 incident->type == INCIDENT_CRIME ? "crime" :
                 incident->type == INCIDENT_ACCIDENT ? "road collision" : "medical call",
                 incident->requiredUnits, eta, SimulationTrafficLevelName(simulation->trafficLevel));
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



        IncidentType service = SimulationServiceForIncident(in->type);

        while (CountAssignedUnits(simulation, i) < in->requiredUnits) {

            int vIndex = FindNearestAvailableVehicle(simulation, service, in->position);

            if (vIndex < 0) break;



            Vehicle *v = &simulation->vehicles[vIndex];

            v->available = 0;

            v->assignedIncident = i;

            v->arrived = 0;
            BuildVehicleRoute(v, in->position);

            simulation->dispatchedByType[service]++;

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

    IncidentType service = SimulationServiceForIncident(incident->type);
    int remaining = incident->requiredUnits - CountAssignedUnits(simulation, incidentIndex);
    int assigned = 0;
    for (int i = 0; i < simulation->vehicleCount && remaining > 0; i++) {
        if (!(vehicleMask & (1u << i))) continue;
        Vehicle *vehicle = &simulation->vehicles[i];
        if (!vehicle->available || vehicle->serviceType != service) continue;
        AssignVehicle(simulation, i, incidentIndex);
        assigned++;
        remaining--;
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

        in->requiredUnits = GetRequiredUnits(in->type, in->severity);
        if (in->callIndex >= 0 && in->callIndex < simulation->callCount)
            simulation->calls[in->callIndex].severity = in->severity;

    }

}



void SimulationUpdate(Simulation *simulation, float deltaTime)

{

    if (deltaTime <= 0.0f) return;

    simulation->elapsedTime += deltaTime;

    UpdateCivilianTraffic(simulation, deltaTime);



    // Time advances for every non-terminal incident.

    for (int i = 0; i < simulation->incidentCount; i++) {

        Incident *in = &simulation->incidents[i];

        if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;



        in->ageTime += deltaTime;

        if (CountArrivedUnits(simulation, i) < in->requiredUnits) {

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



    // Move every dispatched vehicle independently; several can serve one incident.

    for (int vIndex = 0; vIndex < simulation->vehicleCount; vIndex++) {

        Vehicle *v = &simulation->vehicles[vIndex];

        if (v->available || v->assignedIncident < 0 || v->arrived) continue;



        int i = v->assignedIncident;

        Incident *in = &simulation->incidents[i];

        if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;



        float timeLeft = deltaTime;

        while (v->routeIndex < v->routeCount && timeLeft > 0.0f) {
            Vector2 waypoint = v->route[v->routeIndex];
            float distance = DistanceBetween(v->position, waypoint);
            float speed = v->speed * EmergencyVehicleTrafficFactor(simulation, v);
            if (distance < 0.01f) {
                v->position = waypoint;
                v->routeIndex++;
            } else if (speed * timeLeft >= distance) {
                v->position = waypoint;
                timeLeft -= distance / speed;
                v->routeIndex++;
            } else {
                float distanceLeft = speed * timeLeft;
                v->position.x += ((waypoint.x - v->position.x) / distance) * distanceLeft;
                v->position.y += ((waypoint.y - v->position.y) / distance) * distanceLeft;
                timeLeft = 0.0f;
            }
        }

        if (v->routeIndex >= v->routeCount &&
            DistanceBetween(v->position, in->position) < 0.1f) {
            v->position = in->position;
            v->arrived = 1;
            if (!in->responseRecorded) {
                in->responseTime = in->ageTime;
                in->responseRecorded = 1;
            }
        }

    }



    // An incident only starts work once enough assigned units have arrived.

    for (int i = 0; i < simulation->incidentCount; i++) {

        Incident *in = &simulation->incidents[i];

        if (in->status == INCIDENT_RESOLVED || in->status == INCIDENT_FAILED) continue;



        int arrived = CountArrivedUnits(simulation, i);

        if (arrived >= in->requiredUnits) {

            in->status = INCIDENT_RESPONDING;

            in->workElapsed += deltaTime;

            if (in->workElapsed >= in->workRequired) {

                in->status = INCIDENT_RESOLVED;

                simulation->resolvedByType[SimulationServiceForIncident(in->type)]++;

                simulation->totalResponseTimeByType[SimulationServiceForIncident(in->type)] += in->responseTime;

                ReleaseIncidentVehicles(simulation, i);

            }

        } else if (CountAssignedUnits(simulation, i) > 0) {

            in->status = INCIDENT_DISPATCHED;

        } else {

            in->status = INCIDENT_PENDING;

        }

    }



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
    incident->requiredUnits = GetRequiredUnits(type, severity);
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

           SimulationCountStatus(simulation, INCIDENT_RESPONDING);

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
