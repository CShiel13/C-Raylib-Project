#include "simulation.h"
#include <math.h>

static float DistanceBetween(Vector2 a, Vector2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static int FindNearestAvailableVehicle(const Simulation *simulation,
                                      IncidentType requiredService,
                                      Vector2 incidentPosition)
{
    int bestIndex = -1;
    float bestDistance = 1.0e30f;

    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (!vehicle->available || vehicle->serviceType != requiredService) continue;

        float distance = DistanceBetween(vehicle->position, incidentPosition);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void SimulationInit(Simulation *simulation)
{
    simulation->incidentCount = 0;
    simulation->vehicleCount = 5;

    for (int i = 0; i < SERVICE_COUNT; i++) {
        simulation->dispatchedByType[i] = 0;
        simulation->resolvedByType[i] = 0;
        simulation->totalResponseTimeByType[i] = 0.0f;
    }

    // Three fire engines allow up to three fire incidents to be handled concurrently.
    // Units stay at their last location after resolving an incident and can be
    // assigned directly to the next waiting incident instead of returning to base.
    simulation->vehicles[0] = (Vehicle){
        .serviceType = INCIDENT_FIRE,
        .position = {300, 270},
        .stationPosition = {300, 270},
        .speed = 130.0f,
        .available = 1,
        .assignedIncident = -1
    };
    simulation->vehicles[1] = (Vehicle){
        .serviceType = INCIDENT_FIRE,
        .position = {330, 270},
        .stationPosition = {330, 270},
        .speed = 130.0f,
        .available = 1,
        .assignedIncident = -1
    };
    simulation->vehicles[2] = (Vehicle){
        .serviceType = INCIDENT_FIRE,
        .position = {315, 245},
        .stationPosition = {315, 245},
        .speed = 130.0f,
        .available = 1,
        .assignedIncident = -1
    };
    simulation->vehicles[3] = (Vehicle){
        .serviceType = INCIDENT_CRIME,
        .position = {660, 380},
        .stationPosition = {660, 380},
        .speed = 145.0f,
        .available = 1,
        .assignedIncident = -1
    };
    simulation->vehicles[4] = (Vehicle){
        .serviceType = INCIDENT_ACCIDENT,
        .position = {790, 270},
        .stationPosition = {790, 270},
        .speed = 155.0f,
        .available = 1,
        .assignedIncident = -1
    };
}

int SimulationCreateIncident(Simulation *simulation, IncidentType type, Vector2 position)
{
    if (simulation->incidentCount >= MAX_INCIDENTS) return 0;

    Incident *incident = &simulation->incidents[simulation->incidentCount++];
    incident->type = type;
    incident->status = INCIDENT_PENDING;
    incident->position = position;
    incident->ageTime = 0.0f;
    incident->workElapsed = 0.0f;
    incident->responseTime = 0.0f;
    incident->responseDuration = (type == INCIDENT_FIRE) ? 5.0f :
                                 (type == INCIDENT_CRIME) ? 3.5f : 4.0f;
    incident->assignedVehicle = -1;
    return 1;
}

static void DispatchPendingIncidents(Simulation *simulation)
{
    // Oldest pending incidents are considered first.
    for (int i = 0; i < simulation->incidentCount; i++) {
        Incident *incident = &simulation->incidents[i];
        if (incident->status != INCIDENT_PENDING) continue;

        int vehicleIndex = FindNearestAvailableVehicle(
            simulation, incident->type, incident->position);

        if (vehicleIndex >= 0) {
            Vehicle *vehicle = &simulation->vehicles[vehicleIndex];
            vehicle->available = 0;
            vehicle->assignedIncident = i;
            incident->assignedVehicle = vehicleIndex;
            incident->status = INCIDENT_DISPATCHED;
            simulation->dispatchedByType[incident->type]++;
        }
    }
}

void SimulationUpdate(Simulation *simulation, float deltaTime)
{
    if (deltaTime <= 0.0f) return;

    // Incident age advances while pending, travelling, and being handled.
    for (int i = 0; i < simulation->incidentCount; i++) {
        if (simulation->incidents[i].status != INCIDENT_RESOLVED) {
            simulation->incidents[i].ageTime += deltaTime;
        }
    }

    DispatchPendingIncidents(simulation);

    for (int i = 0; i < simulation->incidentCount; i++) {
        Incident *incident = &simulation->incidents[i];
        if (incident->status != INCIDENT_DISPATCHED &&
            incident->status != INCIDENT_RESPONDING) continue;

        if (incident->assignedVehicle < 0 ||
            incident->assignedVehicle >= simulation->vehicleCount) continue;

        Vehicle *vehicle = &simulation->vehicles[incident->assignedVehicle];

        if (incident->status == INCIDENT_DISPATCHED) {
            Vector2 difference = {
                incident->position.x - vehicle->position.x,
                incident->position.y - vehicle->position.y
            };
            float distance = sqrtf(difference.x * difference.x +
                                   difference.y * difference.y);

            if (distance <= vehicle->speed * deltaTime || distance < 1.0f) {
                vehicle->position = incident->position;
                incident->status = INCIDENT_RESPONDING;
                incident->responseTime = incident->ageTime;
                incident->workElapsed = 0.0f;
            } else if (distance > 0.0f) {
                vehicle->position.x += (difference.x / distance) * vehicle->speed * deltaTime;
                vehicle->position.y += (difference.y / distance) * vehicle->speed * deltaTime;
            }
        } else if (incident->status == INCIDENT_RESPONDING) {
            incident->workElapsed += deltaTime;

            if (incident->workElapsed >= incident->responseDuration) {
                incident->status = INCIDENT_RESOLVED;
                simulation->resolvedByType[incident->type]++;
                simulation->totalResponseTimeByType[incident->type] += incident->responseTime;

                // Keep the unit at the incident location. The dispatch loop will
                // send it to the next waiting incident of the same service type.
                vehicle->available = 1;
                vehicle->assignedIncident = -1;
                incident->assignedVehicle = -1;
            }
        }
    }

    // Re-run dispatch after completions so released vehicles can pick up waiting work
    // during the same simulation update.
    DispatchPendingIncidents(simulation);
}

void SimulationSimulateQueue(Simulation *simulation)
{
    // Advance the current queue in simulated 0.1-second steps. The limit prevents
    // an accidental infinite loop if a future feature creates an unresolvable queue.
    int safetySteps = 0;
    const int maxSteps = 200000;

    while (SimulationCountUnresolved(simulation) > 0 && safetySteps < maxSteps) {
        SimulationUpdate(simulation, 0.1f);
        safetySteps++;
    }
}

int SimulationCountStatus(const Simulation *simulation, IncidentStatus status)
{
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        if (simulation->incidents[i].status == status) count++;
    }
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
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (simulation->vehicles[i].serviceType == serviceType &&
            simulation->vehicles[i].available) count++;
    }
    return count;
}

int SimulationCountUnresolved(const Simulation *simulation)
{
    int count = 0;
    for (int i = 0; i < simulation->incidentCount; i++) {
        if (simulation->incidents[i].status != INCIDENT_RESOLVED) count++;
    }
    return count;
}
