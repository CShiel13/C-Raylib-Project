#ifndef SIMULATION_H
#define SIMULATION_H

#include "incidents.h"

typedef enum {
    DISPATCH_PRIORITY = 0,
    DISPATCH_NEAREST_FIRST
} DispatchStrategy;

typedef enum {
    DISPATCH_AUTOMATIC = 0,
    DISPATCH_MANUAL
} DispatchMode;

typedef struct {
    Incident incidents[MAX_INCIDENTS];
    int incidentCount;
    Vehicle vehicles[MAX_VEHICLES];
    int vehicleCount;
    int dispatchedByType[SERVICE_COUNT];
    int resolvedByType[SERVICE_COUNT];
    int failedByType[SERVICE_COUNT];
    float totalResponseTimeByType[SERVICE_COUNT];
    DispatchStrategy dispatchStrategy;
    DispatchMode dispatchMode;
    TrafficLevel trafficLevel;
    CivilianCar civilianCars[MAX_CIVILIAN_CARS];
    int civilianCount;
} Simulation;

typedef struct {
    int resolved;
    int failed;
    float averageResponseTime;
} StrategyResult;

typedef struct {
    StrategyResult nearestFirst;
    StrategyResult priority;
} StrategyComparison;

void SimulationInit(Simulation *simulation);
int SimulationCreateIncident(Simulation *simulation, IncidentType type, Vector2 position);
IncidentType SimulationServiceForIncident(IncidentType type);
int GetRequiredUnits(IncidentType type, int severity);
float SimulationIncidentPriority(const Incident *incident);
int CountAssignedUnits(const Simulation *simulation, int incidentIndex);
int CountArrivedUnits(const Simulation *simulation, int incidentIndex);
void SimulationUpdate(Simulation *simulation, float deltaTime);
void SimulationSimulateQueue(Simulation *simulation);
int SimulationCountStatus(const Simulation *simulation, IncidentStatus status);
int SimulationCountInProgress(const Simulation *simulation);
int SimulationCountAvailableVehicles(const Simulation *simulation, IncidentType serviceType);
int SimulationCountUnresolved(const Simulation *simulation);
int SimulationDispatchVehicles(Simulation *simulation, int incidentIndex,
                               unsigned int vehicleMask);
float SimulationEstimateIncidentETA(const Simulation *simulation, int incidentIndex);
void SimulationSetTrafficLevel(Simulation *simulation, TrafficLevel level);
const char *SimulationTrafficLevelName(TrafficLevel level);
void SimulationRunStrategyComparison(StrategyComparison *comparison);

#endif
