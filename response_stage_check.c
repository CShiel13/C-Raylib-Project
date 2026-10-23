#include "simulation.h"

#include <assert.h>
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

int main(void)
{
    CheckSeverityBasedRequirementsAndFireStages();
    CheckAmbulanceTransportAndManualServiceValidation();
    CheckWaitingForSupportingUnit();
    CheckApartmentFireFrontAndNeighborSpread();
    puts("multi-stage response and progressive fire spread checks passed");
    return 0;
}
