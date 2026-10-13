#ifndef SIMULATION_H
#define SIMULATION_H

#include "incidents.h"

typedef struct {
    Incident incidents[MAX_INCIDENTS];
    int incidentCount;

    Vehicle vehicles[MAX_VEHICLES];
    int vehicleCount;

    int dispatchedByType[SERVICE_COUNT];
    int resolvedByType[SERVICE_COUNT];
    float totalResponseTimeByType[SERVICE_COUNT];
} Simulation;

void SimulationInit(Simulation *simulation);
int SimulationCreateIncident(Simulation *simulation, IncidentType type, Vector2 position);
void SimulationUpdate(Simulation *simulation, float deltaTime);
void SimulationSimulateQueue(Simulation *simulation);
int SimulationCountStatus(const Simulation *simulation, IncidentStatus status);
int SimulationCountInProgress(const Simulation *simulation);
int SimulationCountAvailableVehicles(const Simulation *simulation, IncidentType serviceType);
int SimulationCountUnresolved(const Simulation *simulation);

#endif
