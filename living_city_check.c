#include "simulation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int HasEvent(const Simulation *sim, const char *fragment)
{
    for (int i = 0; i < sim->eventCount; i++)
        if (strstr(sim->events[i].message, fragment)) return 1;
    return 0;
}


static int SpreadOccursBeforeCrewsArrive(CityCondition condition)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(SimulationStartApartmentFireDrill(&sim));
    sim.cityCondition = condition;
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    SimulationProcessOperator(&sim);
    for (int i = 0; i < sim.vehicleCount; i++) sim.vehicles[i].speed = 1.0f;
    for (int step = 0; step < 70; step++) SimulationUpdate(&sim, 0.1f);
    return sim.incidents[0].fireSpreadOccurred;
}

static void CheckManualApartmentDrill(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationStartApartmentFireDrill(&sim));
    assert(sim.dispatchMode == DISPATCH_MANUAL);
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    unsigned int units = (1u << 0) | (1u << 1) | (1u << 2) |
                         (1u << 3) | (1u << 5);
    assert(SimulationDispatchCallVehicles(&sim, 0, units));
    assert(sim.calls[0].dispatched);
    assert(CountAssignedUnits(&sim, sim.calls[0].incidentIndex) == 5);
}

int main(void)
{
    CheckManualApartmentDrill();
    assert(SpreadOccursBeforeCrewsArrive(CITY_WIND));
    assert(!SpreadOccursBeforeCrewsArrive(CITY_CLEAR));

    Simulation sim;
    SimulationInit(&sim);
    assert(sim.pedestrianCount == MAX_PEDESTRIANS);
    assert(SimulationStartApartmentFireDrill(&sim));
    assert(sim.apartmentFireDrill && sim.callCount == 1);
    assert(sim.calls[0].trueType == INCIDENT_FIRE && sim.calls[0].trueSeverity == 3);
    assert(sim.cityCondition == CITY_WIND);
    assert(sim.calls[0].location.x == 624 && sim.calls[0].location.y == 131);

    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    SimulationProcessOperator(&sim);
    int incidentIndex = sim.calls[0].incidentIndex;
    assert(incidentIndex >= 0);
    Incident *fire = &sim.incidents[incidentIndex];
    assert(fire->requiredUnitsByService[INCIDENT_FIRE] == 3);
    assert(fire->requiredUnitsByService[INCIDENT_CRIME] == 1);
    assert(fire->requiredUnitsByService[INCIDENT_ACCIDENT] == 1);
    assert(CountAssignedUnits(&sim, incidentIndex) == 5);

    int sawSecurity = 0, sawFirefighting = 0, sawTreatment = 0, sawTransport = 0;
    int sawEvacuation = 0;
    for (int step = 0; step < 1000 && !sim.calls[0].feedbackReady; step++) {
        SimulationUpdate(&sim, 0.1f);
        sawSecurity |= fire->status == INCIDENT_SECURING;
        sawFirefighting |= fire->status == INCIDENT_FIREFIGHTING;
        sawTreatment |= fire->status == INCIDENT_TREATING;
        sawTransport |= fire->status == INCIDENT_TRANSPORTING;
        sawEvacuation |= HasEvent(&sim, "evacuating along sidewalks");
        for (int p = 0; p < sim.pedestrianCount; p++) {
            assert(sim.pedestrians[p].position.x >= 0.0f && sim.pedestrians[p].position.x <= 1100.0f);
            assert(sim.pedestrians[p].position.y == 282.0f || sim.pedestrians[p].position.y == 370.0f);
            assert(sim.pedestrians[p].position.x >= 20.0f);
            assert(sim.pedestrians[p].position.x <= 1080.0f);
        }
    }
    assert(sawSecurity && sawFirefighting && sawTreatment && sawTransport);
    assert(fire->status == INCIDENT_RESOLVED);
    assert(sim.calls[0].feedbackReady);
    assert(fire->transportComplete);
    assert(HasEvent(&sim, "delivered the patient"));
    assert(sawEvacuation);
    assert(fire->fireContained);

    for (int step = 0; step < 500; step++) SimulationUpdate(&sim, 0.1f);
    for (int i = 0; i < sim.vehicleCount; i++) assert(sim.vehicles[i].available);
    assert(HasEvent(&sim, "returned to its station"));
    puts("apartment fire, staged response, evacuation, hospital transport, and return checks passed");
    return 0;
}
