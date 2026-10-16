#include "../outputs/simulation.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(sim.dispatchMode == DISPATCH_AUTOMATIC);
    assert(SimulationCreateIncident(&sim, INCIDENT_FIRE, (Vector2){600, 500}));
    SimulationUpdate(&sim, 0.1f);
    assert(CountAssignedUnits(&sim, 0) == 1);

    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateIncident(&sim, INCIDENT_FIRE, (Vector2){600, 500}));
    sim.incidents[0].severity = 3;
    sim.incidents[0].requiredUnits = 3;
    SimulationUpdate(&sim, 0.1f);
    assert(CountAssignedUnits(&sim, 0) == 0);
    assert(SimulationDispatchVehicles(&sim, 0, 1u << 5) == 0); /* ambulance rejected */
    assert(SimulationDispatchVehicles(&sim, 0, 1u << 0) == 1);
    assert(SimulationDispatchVehicles(&sim, 0, (1u << MAX_VEHICLES) - 1u) == 2);
    assert(CountAssignedUnits(&sim, 0) == 3);
    assert(SimulationDispatchVehicles(&sim, 0, (1u << MAX_VEHICLES) - 1u) == 0);
    for (int step = 0; step < 300 && sim.incidents[0].status != INCIDENT_RESOLVED; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(sim.incidents[0].status == INCIDENT_RESOLVED);

    SimulationInit(&sim);
    assert(sim.civilianCount == 6);
    assert(SimulationCreateIncident(&sim, INCIDENT_MEDICAL, (Vector2){930, 500}));
    Vector2 before = sim.civilianCars[0].position;
    SimulationSetTrafficLevel(&sim, TRAFFIC_MEDIUM);
    assert(sim.civilianCount == 14);
    SimulationSetTrafficLevel(&sim, TRAFFIC_HEAVY);
    assert(sim.civilianCount == MAX_CIVILIAN_CARS);
    SimulationUpdate(&sim, 0.5f);
    assert(fabsf(sim.civilianCars[0].position.x - before.x) > 0.01f ||
           fabsf(sim.civilianCars[0].position.y - before.y) > 0.01f);

    Simulation denseRun;
    SimulationInit(&denseRun);
    SimulationSetTrafficLevel(&denseRun, TRAFFIC_HEAVY);
    const int roadEnds[7][2] = {{0,1},{1,2},{2,3},{1,4},{1,5},{2,6},{2,7}};
    int hasBothDirections[7][2] = {{0}};
    for (int step = 0; step < 1200; step++) {
        SimulationUpdate(&denseRun, 0.1f);
        for (int node = 1; node <= 2; node++) {
            Vector2 junction = node == 1 ? (Vector2){396, 326} : (Vector2){771, 326};
            int inside = 0;
            for (int i = 0; i < denseRun.civilianCount; i++) {
                float d = hypotf(denseRun.civilianCars[i].position.x - junction.x,
                                 denseRun.civilianCars[i].position.y - junction.y);
                if (d < 14.0f) inside++;
            }
            assert(inside <= 1); /* only one car occupies a junction conflict point */
        }
        for (int i = 0; i < denseRun.civilianCount; i++) {
            CivilianCar *car = &denseRun.civilianCars[i];
            const Vector2 nodes[8] = {{0,326},{396,326},{771,326},{1100,326},
                                      {396,80},{396,630},{771,80},{771,630}};
            Vector2 a = nodes[car->fromNode], b = nodes[car->toNode];
            float length = hypotf(b.x - a.x, b.y - a.y);
            Vector2 center = {a.x + (b.x-a.x)*car->progress, a.y + (b.y-a.y)*car->progress};
            float offset = (car->position.x-center.x)*(-(b.y-a.y)/length) +
                           (car->position.y-center.y)*((b.x-a.x)/length);
            assert(fabsf(offset - 15.0f) < 0.02f);
            int edge = -1;
            for (int e = 0; e < 7; e++)
                if ((car->fromNode == roadEnds[e][0] && car->toNode == roadEnds[e][1]) ||
                    (car->fromNode == roadEnds[e][1] && car->toNode == roadEnds[e][0])) edge = e;
            assert(edge >= 0);
            int direction = car->fromNode == roadEnds[edge][0] ? 0 : 1;
            hasBothDirections[edge][direction] = 1;
            float canonicalOffset = direction == 0 ? offset : -offset;
            assert(fabsf(canonicalOffset - (direction == 0 ? 15.0f : -15.0f)) < 0.02f);
            for (int j = i + 1; j < denseRun.civilianCount; j++) {
                CivilianCar *other = &denseRun.civilianCars[j];
                if (car->fromNode != other->fromNode || car->toNode != other->toNode ||
                    car->laneIndex != other->laneIndex) continue;
                float gap = fabsf(car->progress - other->progress) * length;
                assert(gap >= 14.0f);
            }
        }
    }
    for (int edge = 0; edge < 7; edge++)
        assert(hasBothDirections[edge][0] && hasBothDirections[edge][1]);

    Simulation first, repeat;
    SimulationInit(&first);
    SimulationInit(&repeat);
    SimulationSetTrafficLevel(&first, TRAFFIC_HEAVY);
    SimulationSetTrafficLevel(&repeat, TRAFFIC_HEAVY);
    for (int step = 0; step < 250; step++) {
        SimulationUpdate(&first, 0.1f);
        SimulationUpdate(&repeat, 0.1f);
    }
    for (int i = 0; i < first.civilianCount; i++) {
        assert(fabsf(first.civilianCars[i].position.x - repeat.civilianCars[i].position.x) < 0.001f);
        assert(fabsf(first.civilianCars[i].position.y - repeat.civilianCars[i].position.y) < 0.001f);
        assert(fabsf(first.civilianCars[i].sirenReactionDelay -
                     repeat.civilianCars[i].sirenReactionDelay) < 0.001f);
    }
    assert(first.civilianCars[0].sirenReactionDelay !=
           first.civilianCars[1].sirenReactionDelay);

    SimulationInit(&sim);
    SimulationSetTrafficLevel(&sim, TRAFFIC_HEAVY);
    assert(SimulationCreateIncident(&sim, INCIDENT_MEDICAL, (Vector2){920, 500}));
    SimulationSimulateQueue(&sim);
    assert(SimulationCountStatus(&sim, INCIDENT_RESOLVED) == 1);

    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateIncident(&sim, INCIDENT_FIRE, (Vector2){900,326}));
    float clearEta = SimulationEstimateIncidentETA(&sim, 0);
    CivilianCar *blocker = &sim.civilianCars[0];
    blocker->fromNode = 1;
    blocker->toNode = 2;
    blocker->progress = 0.05f;
    blocker->speed = 20.0f;
    blocker->currentSpeed = 20.0f;
    blocker->sirenReactionDelay = 0.8f;
    blocker->sirenReactionTimer = -1.0f;
    blocker->sirenSource = -1;
    float blockedEta = SimulationEstimateIncidentETA(&sim, 0);
    assert(blockedEta > clearEta + 0.5f);

    Simulation actualClear, actualBlocked;
    SimulationInit(&actualClear);
    SimulationInit(&actualBlocked);
    assert(SimulationCreateIncident(&actualClear, INCIDENT_FIRE, (Vector2){900,326}));
    assert(SimulationCreateIncident(&actualBlocked, INCIDENT_FIRE, (Vector2){900,326}));
    CivilianCar *actualBlocker = &actualBlocked.civilianCars[0];
    actualBlocker->fromNode = 1;
    actualBlocker->toNode = 2;
    actualBlocker->progress = 0.05f;
    actualBlocker->speed = actualBlocker->currentSpeed = 20.0f;
    actualBlocker->sirenReactionDelay = 0.8f;
    float elapsed = 0.0f;
    while (elapsed < 20.0f &&
           (actualClear.incidents[0].status != INCIDENT_RESOLVED ||
            actualBlocked.incidents[0].status != INCIDENT_RESOLVED)) {
        SimulationUpdate(&actualClear, 0.1f);
        SimulationUpdate(&actualBlocked, 0.1f);
        elapsed += 0.1f;
    }
    assert(actualClear.incidents[0].status == INCIDENT_RESOLVED);
    assert(actualBlocked.incidents[0].status == INCIDENT_RESOLVED);
    assert(actualBlocked.incidents[0].responseTime > actualClear.incidents[0].responseTime);

    SimulationInit(&sim);
    SimulationSetTrafficLevel(&sim, TRAFFIC_HEAVY);
    assert(SimulationCreateIncident(&sim, INCIDENT_FIRE, sim.civilianCars[0].position));
    sim.vehicles[0].available = 0;
    sim.vehicles[0].assignedIncident = 0;
    sim.vehicles[0].position = (Vector2){430,326};
    sim.vehicles[0].route[1] = (Vector2){771,326};
    sim.vehicles[0].routeCount = 2;
    sim.vehicles[0].routeIndex = 1;
    sim.vehicles[0].arrived = 0;
    sim.incidents[0].status = INCIDENT_DISPATCHED;
    sim.civilianCars[0].fromNode = 1;
    sim.civilianCars[0].toNode = 2;
    sim.civilianCars[0].progress = 0.25f;
    sim.civilianCars[0].position = (Vector2){537,341};
    int initiallyReacting = 0;
    float firstReactionDelay = sim.civilianCars[0].sirenReactionDelay;
    for (int step = 0; step < 30 && sim.civilianCars[0].yieldTime <= 0.0f; step++) {
        SimulationUpdate(&sim, 0.05f);
        if (sim.civilianCars[0].sirenReactionTimer > 0.0f) initiallyReacting = 1;
    }
    assert(initiallyReacting);
    assert(sim.civilianCars[0].yieldTime > 0.0f);
    assert(firstReactionDelay != sim.civilianCars[1].sirenReactionDelay);
    sim.vehicles[0].position = (Vector2){1090, 620};
    SimulationUpdate(&sim, 1.2f);
    assert(sim.civilianCars[0].yieldTime == 0.0f);

    puts("dispatch/traffic checks passed");
    return 0;
}
