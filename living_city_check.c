#include "simulation.h"
#include "city_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int HasEvent(const Simulation *sim, const char *fragment)
{
    for (int i = 0; i < sim->eventCount; i++)
        if (strstr(sim->events[i].message, fragment)) return 1;
    return 0;
}

static int IsOnPedestrianPavement(Vector2 p)
{
    if ((fabsf(p.y - 282.0f) < 0.01f || fabsf(p.y - 370.0f) < 0.01f) &&
        p.x >= 20.0f && p.x <= 1094.0f) return 1;
    if ((fabsf(p.x - 345.0f) < 0.01f || fabsf(p.x - 720.0f) < 0.01f ||
         fabsf(p.x - 1094.0f) < 0.01f) && p.y >= 80.0f && p.y <= 630.0f) return 1;
    if ((fabsf(p.x - 447.0f) < 0.01f || fabsf(p.x - 822.0f) < 0.01f) &&
        p.y >= 282.0f && p.y <= 370.0f) return 1;
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
    assert(sim.hospitalPosition.x == CITY_LAYOUT_HOSPITAL_ENTRANCE.x &&
           sim.hospitalPosition.y == CITY_LAYOUT_HOSPITAL_ENTRANCE.y);
    assert(sim.hospitalPosition.x > CITY_LAYOUT_HOSPITAL_FOOTPRINT.x +
                                      CITY_LAYOUT_HOSPITAL_FOOTPRINT.width);
    assert(sim.hospitalPosition.x >= CITY_LAYOUT_HOSPITAL_BAY.x &&
           sim.hospitalPosition.x <= CITY_LAYOUT_HOSPITAL_BAY.x +
                                      CITY_LAYOUT_HOSPITAL_BAY.width);
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
    int sawTransportRouteToHospital = 0;
    for (int step = 0; step < 1000 && !sim.calls[0].feedbackReady; step++) {
        SimulationUpdate(&sim, 0.1f);
        sawSecurity |= fire->status == INCIDENT_SECURING;
        sawFirefighting |= fire->status == INCIDENT_FIREFIGHTING;
        sawTreatment |= fire->status == INCIDENT_TREATING;
        sawTransport |= fire->status == INCIDENT_TRANSPORTING;
        if (fire->status == INCIDENT_TRANSPORTING) {
            for (int unit = 0; unit < sim.vehicleCount; unit++) {
                const Vehicle *ambulance = &sim.vehicles[unit];
                if (ambulance->assignedIncident != incidentIndex ||
                    ambulance->activity != UNIT_TRANSPORTING ||
                    ambulance->routeCount <= 0) continue;
                Vector2 destination = ambulance->route[ambulance->routeCount - 1];
                assert(fabsf(destination.x - CITY_LAYOUT_HOSPITAL_ENTRANCE.x) < 0.01f);
                assert(fabsf(destination.y - CITY_LAYOUT_HOSPITAL_ENTRANCE.y) < 0.01f);
                sawTransportRouteToHospital = 1;
            }
        }
        for (int p = 0; p < sim.pedestrianCount; p++)
            sawEvacuation |= sim.pedestrians[p].evacuating ||
                             sim.pedestrians[p].state == PEDESTRIAN_EVACUATING;
        for (int p = 0; p < sim.pedestrianCount; p++) {
            assert(sim.pedestrians[p].position.x >= 0.0f && sim.pedestrians[p].position.x <= CITY_LAYOUT_WIDTH);
            assert(sim.pedestrians[p].position.y >= 80.0f && sim.pedestrians[p].position.y <= 630.0f);
            assert(IsOnPedestrianPavement(sim.pedestrians[p].position));
        }
    }
    assert(sawSecurity && sawFirefighting && sawTreatment && sawTransport);
    assert(sawTransportRouteToHospital);
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
