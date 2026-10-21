#include "simulation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int FindEvent(const Simulation *sim, const char *phrase)
{
    for (int i = 0; i < sim->eventCount; i++)
        if (strstr(sim->events[i].message, phrase)) return i;
    return -1;
}

static void CheckConditionsAndReset(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(sim.cityCondition == CITY_CLEAR);
    assert(strcmp(SimulationCityConditionName(sim.cityCondition), "CLEAR") == 0);
    assert(sim.eventCount == 0);
    SimulationSetCityCondition(&sim, CITY_RAIN);
    assert(sim.cityCondition == CITY_RAIN);
    assert(strstr(sim.events[0].message, "RAIN") != NULL);
    SimulationSetCityCondition(&sim, CITY_FOG);
    assert(sim.cityCondition == CITY_FOG);
    assert(strstr(sim.events[1].message, "FOG") != NULL);
    SimulationSetCityCondition(&sim, CITY_CLEAR);
    assert(sim.cityCondition == CITY_CLEAR && sim.eventCount == 3);
    SimulationInit(&sim);
    assert(sim.cityCondition == CITY_CLEAR && sim.eventCount == 0);
    assert(SimulationCreateIncident(&sim, INCIDENT_MEDICAL, (Vector2){770, 270}));
    float clearEta = SimulationEstimateIncidentETA(&sim, 0);
    SimulationSetCityCondition(&sim, CITY_RAIN);
    assert(SimulationEstimateIncidentETA(&sim, 0) == clearEta);
    SimulationSetCityCondition(&sim, CITY_FOG);
    assert(SimulationEstimateIncidentETA(&sim, 0) == clearEta);
}

static void CheckAllIncidentTypesCanResolveAndFail(void)
{
    const IncidentType types[] = {INCIDENT_FIRE, INCIDENT_CRIME,
                                  INCIDENT_ACCIDENT, INCIDENT_MEDICAL};
    const Vector2 positions[] = {{300,270}, {650,380}, {770,270}, {770,270}};
    const int units[] = {0, 3, 5, 5};
    for (int typeIndex = 0; typeIndex < 4; typeIndex++) {
        Simulation sim;
        SimulationInit(&sim);
        sim.dispatchMode = DISPATCH_MANUAL;
        assert(SimulationCreateIncident(&sim, types[typeIndex], positions[typeIndex]));
        assert(sim.incidents[0].status == INCIDENT_PENDING);
        assert(SimulationDispatchVehicles(&sim, 0, 1u << units[typeIndex]) == 1);
        SimulationUpdate(&sim, 0.1f);
        assert(sim.incidents[0].status != INCIDENT_RESOLVED &&
               sim.incidents[0].status != INCIDENT_FAILED);
        for (int step = 0; step < 200 && sim.incidents[0].status != INCIDENT_RESOLVED; step++)
            SimulationUpdate(&sim, 0.1f);
        assert(sim.incidents[0].status == INCIDENT_RESOLVED);

        SimulationInit(&sim);
        sim.dispatchMode = DISPATCH_MANUAL;
        assert(SimulationCreateIncident(&sim, types[typeIndex], positions[typeIndex]));
        for (int step = 0; step < 400 && sim.incidents[0].status != INCIDENT_FAILED; step++)
            SimulationUpdate(&sim, 0.1f);
        assert(sim.incidents[0].status == INCIDENT_FAILED);
    }
}

static void CheckEventLifecycle(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&sim, (Vector2){320, 270}, "Smoke from a small building fire."));
    assert(FindEvent(&sim, "received") == 0);
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 1));
    int classificationEvent = FindEvent(&sim, "classified");
    assert(classificationEvent > 0);
    assert(SimulationDispatchCallVehicles(&sim, 0, 1u << 0) == 1);
    int dispatchEvent = FindEvent(&sim, "dispatched");
    assert(dispatchEvent > classificationEvent);
    for (int step = 0; step < 1000 && sim.incidents[0].status != INCIDENT_RESOLVED; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(sim.incidents[0].status == INCIDENT_RESOLVED);
    int arrivalEvent = FindEvent(&sim, "arrived");
    int resolvedEvent = FindEvent(&sim, "resolved");
    assert(arrivalEvent > dispatchEvent && resolvedEvent > arrivalEvent);
    for (int i = 1; i < sim.eventCount; i++)
        assert(sim.events[i].time >= sim.events[i - 1].time);

    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&sim, (Vector2){600, 500}, "Caller reports a break-in."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_CRIME, 1));
    for (int step = 0; step < 310 && sim.incidents[0].status != INCIDENT_FAILED; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(sim.incidents[0].status == INCIDENT_FAILED);
    assert(FindEvent(&sim, "failed") >= 0);
}

static void CheckBoundedFeed(void)
{
    Simulation sim;
    SimulationInit(&sim);
    for (int i = 0; i < MAX_SIMULATION_EVENTS + 8; i++)
        assert(SimulationCreateCall(&sim, (Vector2){(float)i, 100}, "Report"));
    assert(sim.eventCount == MAX_SIMULATION_EVENTS);
    assert(strstr(sim.events[0].message, "Call #09 received") != NULL);
    assert(strstr(sim.events[MAX_SIMULATION_EVENTS - 1].message,
                  "Call #40 received") != NULL);
    for (int i = 1; i < sim.eventCount; i++)
        assert(sim.events[i].time >= sim.events[i - 1].time);
}

static void CheckTrafficDelayEvent(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateIncident(&sim, INCIDENT_FIRE, (Vector2){900, 326}));
    assert(SimulationDispatchVehicles(&sim, 0, 1u << 0) == 1);
    CivilianCar *blocker = &sim.civilianCars[0];
    blocker->fromNode = 1;
    blocker->toNode = 2;
    blocker->progress = 0.05f;
    blocker->speed = blocker->currentSpeed = 20.0f;
    blocker->sirenReactionDelay = 0.8f;
    blocker->sirenReactionTimer = -1.0f;
    blocker->sirenSource = -1;
    for (int step = 0; step < 100 && FindEvent(&sim, "slowed by traffic") < 0; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(FindEvent(&sim, "slowed by traffic") >= 0);
}

int main(void)
{
    CheckConditionsAndReset();
    CheckAllIncidentTypesCanResolveAndFail();
    CheckEventLifecycle();
    CheckBoundedFeed();
    CheckTrafficDelayEvent();
    puts("event lifecycle, bounded feed, conditions, and traffic event checks passed");
    return 0;
}
