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
    CityCondition cityCondition;
    CivilianCar civilianCars[MAX_CIVILIAN_CARS];
    int civilianCount;
    Pedestrian pedestrians[MAX_PEDESTRIANS];
    int pedestrianCount;
    int apartmentFireDrill;
    int evacuationEventLogged;
    Vector2 hospitalPosition;
    EmergencyCall calls[MAX_CALLS];
    int callCount;
    float elapsedTime;
    SimulationEvent events[MAX_SIMULATION_EVENTS];
    int eventCount;
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
int SimulationStartApartmentFireDrill(Simulation *simulation);
int SimulationStartApartmentFireDrillAtSeverity(Simulation *simulation, int severity);
int SimulationCreateIncident(Simulation *simulation, IncidentType type, Vector2 position);
int SimulationCreateCall(Simulation *simulation, Vector2 location, const char *report);
int SimulationSetCallTruth(Simulation *simulation, int callIndex,
                           IncidentType trueType, int trueSeverity);
int SimulationSuggestCallClassification(const Simulation *simulation, int callIndex,
                                        IncidentType *type, int *severity);
int SimulationClassifyCall(Simulation *simulation, int callIndex,
                           IncidentType type, int severity);
int SimulationDispatchCallVehicles(Simulation *simulation, int callIndex,
                                   unsigned int vehicleMask);
void SimulationProcessOperator(Simulation *simulation);
IncidentType SimulationServiceForIncident(IncidentType type);
int GetRequiredUnits(IncidentType type, int severity);
float SimulationIncidentPriority(const Incident *incident);
int CountAssignedUnits(const Simulation *simulation, int incidentIndex);
int CountArrivedUnits(const Simulation *simulation, int incidentIndex);
int CountAssignedUnitsByService(const Simulation *simulation, int incidentIndex,
                                IncidentType serviceType);
int CountArrivedUnitsByService(const Simulation *simulation, int incidentIndex,
                               IncidentType serviceType);
int CountRequiredUnits(const Incident *incident);
void SimulationUpdate(Simulation *simulation, float deltaTime);
void SimulationSimulateQueue(Simulation *simulation);
int SimulationCountStatus(const Simulation *simulation, IncidentStatus status);
int SimulationCountInProgress(const Simulation *simulation);
int SimulationCountAvailableVehicles(const Simulation *simulation, IncidentType serviceType);
int SimulationCountUnresolved(const Simulation *simulation);
int SimulationDispatchVehicles(Simulation *simulation, int incidentIndex,
                               unsigned int vehicleMask);
float SimulationEstimateIncidentETA(const Simulation *simulation, int incidentIndex);
float SimulationEstimateUnitETA(const Simulation *simulation, int vehicleIndex,
                                int incidentIndex);
void SimulationSetTrafficLevel(Simulation *simulation, TrafficLevel level);
const char *SimulationTrafficLevelName(TrafficLevel level);
void SimulationSetCityCondition(Simulation *simulation, CityCondition condition);
const char *SimulationCityConditionName(CityCondition condition);
void SimulationRunStrategyComparison(StrategyComparison *comparison);

#endif
