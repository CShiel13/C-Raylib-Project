#include "simulation.h"
#include "manual_dispatch.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void RunUntilResolved(Simulation *simulation, int incidentIndex,
                             int *seenSecurity, int *seenFire, int *seenTreatment,
                             int *seenTransport)
{
    int lastStage = -1;
    for (int step = 0; step < 320 &&
         simulation->incidents[incidentIndex].status != INCIDENT_RESOLVED &&
         simulation->incidents[incidentIndex].status != INCIDENT_FAILED; step++) {
        SimulationUpdate(simulation, 0.1f);
        IncidentStatus status = simulation->incidents[incidentIndex].status;
        int stage = status == INCIDENT_SECURING ? 0 :
                    status == INCIDENT_FIREFIGHTING ? 1 :
                    status == INCIDENT_TREATING ? 2 :
                    status == INCIDENT_TRANSPORTING ? 3 : -1;
        if (stage >= 0) {
            assert(stage >= lastStage);
            lastStage = stage;
            if (stage == 0) *seenSecurity = 1;
            if (stage == 1) *seenFire = 1;
            if (stage == 2) *seenTreatment = 1;
            if (stage == 3) *seenTransport = 1;
        }
    }
}

static void CheckSeverityBasedRequirementsAndFireStages(void)
{
    Simulation small;
    SimulationInit(&small);
    assert(SimulationCreateCall(&small, (Vector2){600, 400}, "Small bin fire."));
    assert(SimulationClassifyCall(&small, 0, INCIDENT_FIRE, 1));
    Incident *smallFire = &small.incidents[small.calls[0].incidentIndex];
    assert(smallFire->requiredUnitsByService[INCIDENT_FIRE] == 1);
    assert(smallFire->requiredUnitsByService[INCIDENT_CRIME] == 0);
    assert(smallFire->requiredUnitsByService[INCIDENT_ACCIDENT] == 0);

    Simulation sim;
    SimulationInit(&sim);
    assert(SimulationCreateCall(&sim, (Vector2){600, 400},
                                "Apartment building fire; people trapped."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    Incident *incident = &sim.incidents[sim.calls[0].incidentIndex];
    assert(incident->requiredUnitsByService[INCIDENT_FIRE] == 3);
    assert(incident->requiredUnitsByService[INCIDENT_CRIME] == 1);
    assert(incident->requiredUnitsByService[INCIDENT_ACCIDENT] == 1);
    assert(CountRequiredUnits(incident) == 5);
    assert(incident->workRequired > smallFire->workRequired);
    SimulationProcessOperator(&sim);
    assert(sim.calls[0].dispatched);
    assert(CountAssignedUnits(&sim, sim.calls[0].incidentIndex) == 5);
    assert(CountAssignedUnitsByService(&sim, sim.calls[0].incidentIndex, INCIDENT_FIRE) == 3);
    assert(CountAssignedUnitsByService(&sim, sim.calls[0].incidentIndex, INCIDENT_CRIME) == 1);
    assert(CountAssignedUnitsByService(&sim, sim.calls[0].incidentIndex, INCIDENT_ACCIDENT) == 1);

    int security = 0, fire = 0, treatment = 0, transport = 0;
    RunUntilResolved(&sim, sim.calls[0].incidentIndex,
                     &security, &fire, &treatment, &transport);
    assert(security && fire && treatment && transport);
    assert(incident->sceneSecured && incident->primaryWorkComplete &&
           incident->patientCareComplete && incident->transportComplete);
    assert(incident->status == INCIDENT_RESOLVED);
}

static void CheckAmbulanceTransportAndManualServiceValidation(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(SimulationCreateCall(&sim, (Vector2){600, 400}, "Patient injured and needs help."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_MEDICAL, 2));
    Incident *incident = &sim.incidents[sim.calls[0].incidentIndex];
    assert(incident->requiredUnitsByService[INCIDENT_ACCIDENT] == 2);
    assert(incident->requiresTransport);
    SimulationProcessOperator(&sim);
    int security = 0, fire = 0, treatment = 0, transport = 0;
    RunUntilResolved(&sim, sim.calls[0].incidentIndex,
                     &security, &fire, &treatment, &transport);
    assert(treatment && transport && incident->status == INCIDENT_RESOLVED);

    Simulation manual;
    SimulationInit(&manual);
    manual.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&manual, (Vector2){600, 400}, "Apartment fire."));
    assert(SimulationClassifyCall(&manual, 0, INCIDENT_FIRE, 3));
    unsigned int fireOnly = (1u << 0) | (1u << 1) | (1u << 2);
    assert(!SimulationDispatchCallVehicles(&manual, 0, fireOnly));
    unsigned int mixed = fireOnly | (1u << 3) | (1u << 5);
    assert(!SimulationDispatchCallVehicles(&manual, 0, fireOnly | (1u << 3) | (1u << 4)));
    assert(SimulationDispatchCallVehicles(&manual, 0, mixed) == 5);
    assert(CountAssignedUnits(&manual, manual.calls[0].incidentIndex) == 5);
}

static void CheckWaitingForSupportingUnit(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(SimulationCreateCall(&sim, (Vector2){600, 400}, "Apartment fire."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    for (int i = 5; i < 9; i++) sim.vehicles[i].available = 0;
    SimulationProcessOperator(&sim);
    assert(!sim.calls[0].dispatched);
    assert(CountAssignedUnits(&sim, sim.calls[0].incidentIndex) == 0);
    sim.vehicles[5].available = 1;
    SimulationProcessOperator(&sim);
    assert(sim.calls[0].dispatched);
    assert(CountAssignedUnits(&sim, sim.calls[0].incidentIndex) == 5);
}

static void CheckEscalatedRequirementsCanBeDispatchedAgain(void)
{
    Simulation automatic;
    SimulationInit(&automatic);
    assert(SimulationCreateCall(&automatic, (Vector2){80, 326}, "Small smoke report."));
    assert(SimulationClassifyCall(&automatic, 0, INCIDENT_FIRE, 1));
    for (int i = 0; i < automatic.vehicleCount; i++)
        if (automatic.vehicles[i].serviceType == INCIDENT_FIRE)
            automatic.vehicles[i].speed = 1.0f;
    SimulationProcessOperator(&automatic);
    assert(automatic.calls[0].dispatched);
    assert(CountAssignedUnitsByService(&automatic, 0, INCIDENT_FIRE) == 1);
    float originalDispatchTime = automatic.calls[0].dispatchedAt;

    for (int step = 0; step < 101; step++) SimulationUpdate(&automatic, 0.1f);
    Incident *autoIncident = &automatic.incidents[automatic.calls[0].incidentIndex];
    assert(autoIncident->severity == 2);
    assert(CountAssignedUnitsByService(&automatic, 0, INCIDENT_FIRE) == 2);
    assert(automatic.calls[0].dispatchedAt == originalDispatchTime);

    for (int step = 101; step < 202; step++) SimulationUpdate(&automatic, 0.1f);
    assert(autoIncident->severity == 3);
    assert(CountAssignedUnitsByService(&automatic, 0, INCIDENT_FIRE) == 3);
    assert(CountAssignedUnitsByService(&automatic, 0, INCIDENT_CRIME) == 1);
    assert(CountAssignedUnitsByService(&automatic, 0, INCIDENT_ACCIDENT) == 1);
    assert(CountAssignedUnits(&automatic, 0) == 5);
    assert(automatic.calls[0].dispatchedAt == originalDispatchTime);

    Simulation manual;
    SimulationInit(&manual);
    manual.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&manual, (Vector2){80, 326}, "Small smoke report."));
    assert(!ManualCallNeedsVehicleAssignment(&manual, 0));
    assert(SimulationClassifyCall(&manual, 0, INCIDENT_FIRE, 1));
    assert(ManualCallNeedsVehicleAssignment(&manual, 0));
    for (int i = 0; i < manual.vehicleCount; i++)
        if (manual.vehicles[i].serviceType == INCIDENT_FIRE)
            manual.vehicles[i].speed = 1.0f;
    assert(SimulationDispatchCallVehicles(&manual, 0, 1u << 0) == 1);
    assert(!ManualCallNeedsVehicleAssignment(&manual, 0));
    float manualDispatchTime = manual.calls[0].dispatchedAt;
    for (int step = 0; step < 101; step++) SimulationUpdate(&manual, 0.1f);
    Incident *manualIncident = &manual.incidents[manual.calls[0].incidentIndex];
    assert(manualIncident->severity == 2);
    assert(manual.calls[0].dispatched);
    assert(ManualCallNeedsVehicleAssignment(&manual, 0));
    assert(CountAssignedUnits(&manual, 0) == 1);

    // No additional unit is dispatched while the remaining suitable vehicle
    // is unavailable, and an incorrect service selection remains rejected.
    manual.vehicles[1].available = 0;
    assert(SimulationDispatchCallVehicles(&manual, 0, 1u << 1) == 0);
    manual.vehicles[1].available = 1;
    assert(SimulationDispatchCallVehicles(&manual, 0, (1u << 1) | (1u << 3)) == 0);
    assert(CountAssignedUnits(&manual, 0) == 1);
    assert(SimulationDispatchCallVehicles(&manual, 0, 1u << 1) == 1);
    assert(!ManualCallNeedsVehicleAssignment(&manual, 0));
    assert(CountAssignedUnitsByService(&manual, 0, INCIDENT_FIRE) == 2);
    assert(manual.calls[0].dispatchedAt == manualDispatchTime);
}

static void CheckInvalidInputsAndResetState(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(!SimulationCreateIncident(NULL, INCIDENT_FIRE, (Vector2){0, 0}));
    assert(!SimulationCreateIncident(&sim, (IncidentType)-1, (Vector2){0, 0}));
    assert(!SimulationCreateIncident(&sim, (IncidentType)(INCIDENT_MEDICAL + 1),
                                     (Vector2){0, 0}));
    SimulationUpdate(NULL, 0.1f);
    SimulationUpdate(&sim, NAN);
    SimulationUpdate(&sim, INFINITY);
    SimulationUpdate(&sim, -0.1f);
    assert(sim.elapsedTime == 0.0f && sim.incidentCount == 0);

    assert(SimulationCreateCall(&sim, (Vector2){300, 300}, "Smoke reported."));
    SimulationSetTrafficLevel(&sim, TRAFFIC_HEAVY);
    SimulationSetCityCondition(&sim, CITY_WIND);
    assert(sim.callCount == 1 && sim.civilianCount == MAX_CIVILIAN_CARS);
    SimulationInit(&sim);
    assert(sim.callCount == 0 && sim.incidentCount == 0 && sim.eventCount == 0);
    assert(sim.elapsedTime == 0.0f && sim.civilianCount == 6);
    assert(sim.dispatchMode == DISPATCH_AUTOMATIC && sim.cityCondition == CITY_CLEAR);
    for (int i = 0; i < sim.vehicleCount; i++) {
        assert(sim.vehicles[i].available);
        assert(sim.vehicles[i].activity == UNIT_AVAILABLE);
        assert(sim.vehicles[i].assignedIncident == -1);
        assert(sim.vehicles[i].routeCount == 0);
    }
}

static void RunTwoCallScenario(DispatchMode mode)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = mode;
    assert(SimulationCreateCall(&sim, (Vector2){396, 270}, "Smoke reported in a room."));
    assert(SimulationCreateCall(&sim, (Vector2){771, 390}, "Suspicious person reported."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 1));
    assert(SimulationClassifyCall(&sim, 1, INCIDENT_CRIME, 1));

    if (mode == DISPATCH_AUTOMATIC) {
        SimulationProcessOperator(&sim);
    } else {
        assert(SimulationDispatchCallVehicles(&sim, 0, 1u << 0) == 1);
        assert(SimulationDispatchCallVehicles(&sim, 1, 1u << 3) == 1);
    }
    assert(sim.calls[0].dispatched && sim.calls[1].dispatched);
    for (int step = 0; step < 1500 &&
         (!sim.calls[0].feedbackReady || !sim.calls[1].feedbackReady); step++)
        SimulationUpdate(&sim, 0.1f);
    assert(sim.calls[0].feedbackReady && sim.calls[1].feedbackReady);
    assert(sim.incidents[sim.calls[0].incidentIndex].status == INCIDENT_RESOLVED);
    assert(sim.incidents[sim.calls[1].incidentIndex].status == INCIDENT_RESOLVED);

    for (int step = 0; step < 1000; step++) SimulationUpdate(&sim, 0.1f);
    for (int i = 0; i < sim.vehicleCount; i++) {
        assert(sim.vehicles[i].available);
        assert(sim.vehicles[i].activity == UNIT_AVAILABLE);
        assert(sim.vehicles[i].assignedIncident == -1);
    }
}

static void CheckMultipleIncidentsInBothModes(void)
{
    RunTwoCallScenario(DISPATCH_AUTOMATIC);
    RunTwoCallScenario(DISPATCH_MANUAL);
}

static void CheckApartmentFireFrontAndNeighborSpread(void)
{
    Simulation small;
    SimulationInit(&small);
    small.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationStartApartmentFireDrillAtSeverity(&small, 1));
    assert(SimulationClassifyCall(&small, 0, INCIDENT_FIRE, 1));
    Incident *smallFire = &small.incidents[small.calls[0].incidentIndex];
    assert(smallFire->fireRoomsBurning == 1);
    for (int i = 0; i < 42; i++) SimulationUpdate(&small, 0.1f);
    assert(smallFire->fireRoomsBurning >= 2);
    assert(!smallFire->fireSpreadOccurred);

    Simulation critical;
    SimulationInit(&critical);
    critical.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationStartApartmentFireDrill(&critical));
    assert(SimulationClassifyCall(&critical, 0, INCIDENT_FIRE, 3));
    Incident *criticalFire = &critical.incidents[critical.calls[0].incidentIndex];
    for (int i = 0; i < 61; i++) SimulationUpdate(&critical, 0.1f);
    assert(criticalFire->fireRoomsBurning >= 3);
    assert(criticalFire->fireSpreadOccurred);
}

static void CheckEastWardCallUsesConnectedRoad(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&sim, (Vector2){1068.0f, 136.0f},
                                "Two vehicles collided; one lane blocked."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_ACCIDENT, 1));
    int incidentIndex = sim.calls[0].incidentIndex;
    assert(incidentIndex >= 0);
    int ambulance = -1;
    for (int i = 0; i < sim.vehicleCount; i++)
        if (sim.vehicles[i].serviceType == INCIDENT_ACCIDENT) { ambulance = i; break; }
    assert(ambulance >= 0);
    assert(SimulationDispatchCallVehicles(&sim, 0, 1u << ambulance) == 1);

    const Vehicle *unit = &sim.vehicles[ambulance];
    int usesEastWardAccess = 0;
    for (int i = 0; i < unit->routeCount; i++)
        if (fabsf(unit->route[i].x - 1065.0f) < 0.01f &&
            unit->route[i].y >= 80.0f && unit->route[i].y <= 326.0f)
            usesEastWardAccess = 1;
    assert(usesEastWardAccess);
}

int main(void)
{
    CheckSeverityBasedRequirementsAndFireStages();
    CheckAmbulanceTransportAndManualServiceValidation();
    CheckWaitingForSupportingUnit();
    CheckEscalatedRequirementsCanBeDispatchedAgain();
    CheckInvalidInputsAndResetState();
    CheckMultipleIncidentsInBothModes();
    CheckApartmentFireFrontAndNeighborSpread();
    CheckEastWardCallUsesConnectedRoad();
    puts("multi-stage response and progressive fire spread checks passed");
    return 0;
}
