#include "simulation.h"
#include "city_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int CountEventsContaining(const Simulation *simulation, const char *text)
{
    int count = 0;
    for (int i = 0; i < simulation->eventCount; i++)
        if (strstr(simulation->events[i].message, text)) count++;
    return count;
}

static void Advance(Simulation *simulation, float seconds)
{
    while (seconds > 0.0f) {
        float step = seconds > 0.1f ? 0.1f : seconds;
        SimulationUpdate(simulation, step);
        seconds -= step;
    }
}

static int IsBuildingCenter(Vector2 position)
{
    for (int i = 0; i < CITY_LAYOUT_BUILDING_COUNT; i++) {
        const Rectangle building = CITY_LAYOUT_BUILDINGS[i];
        if (fabsf(position.x - (building.x + building.width * 0.5f)) < 0.01f &&
            fabsf(position.y - (building.y + building.height * 0.5f)) < 0.01f)
            return 1;
    }
    return 0;
}

static void CheckDisabledAndReset(void)
{
    Simulation simulation;
    SimulationInit(&simulation);
    assert(!simulation.autoIncidentsEnabled);
    Advance(&simulation, 80.0f);
    assert(simulation.callCount == 0);
    SimulationSetAutoIncidents(&simulation, 1, AUTO_INCIDENT_DEFAULT_SEED);
    assert(simulation.autoIncidentsEnabled);
    SimulationInit(&simulation);
    assert(!simulation.autoIncidentsEnabled);
    assert(SimulationAutoIncidentSecondsUntilNext(&simulation) < 0.0f);
}

static void CheckIntervalsAndPauseSemantics(void)
{
    Simulation simulation;
    SimulationInit(&simulation);
    SimulationSetAutoIncidents(&simulation, 1, 123456u);
    float first = SimulationAutoIncidentSecondsUntilNext(&simulation);
    assert(first >= 5.0f && first <= 30.0f);

    // A paused UI does not call SimulationUpdate; simulated time and countdown
    // therefore remain exactly unchanged while paused.
    float beforePause = simulation.elapsedTime;
    float remainingBeforePause = SimulationAutoIncidentSecondsUntilNext(&simulation);
    assert(simulation.elapsedTime == beforePause);
    assert(SimulationAutoIncidentSecondsUntilNext(&simulation) == remainingBeforePause);

    Advance(&simulation, first * 0.25f);
    assert(simulation.callCount == 0);
    assert(fabsf(SimulationAutoIncidentSecondsUntilNext(&simulation) - first * 0.75f) < 0.02f);
    // Slow and fast controls scale the delta passed to the simulation. The
    // feature consumes only simulated time, never wall-clock time.
    Advance(&simulation, first * 0.75f);
    assert(simulation.callCount == 1);
    float second = SimulationAutoIncidentSecondsUntilNext(&simulation);
    assert(second >= 5.0f && second <= 30.0f);
    Advance(&simulation, second + 0.02f);
    assert(simulation.callCount == 2);
    float third = SimulationAutoIncidentSecondsUntilNext(&simulation);
    assert(third >= 5.0f && third <= 30.0f);

    Simulation fast;
    SimulationInit(&fast);
    SimulationSetAutoIncidents(&fast, 1, 123456u);
    float fastInterval = SimulationAutoIncidentSecondsUntilNext(&fast);
    SimulationUpdate(&fast, fastInterval * 8.0f); // one deliberately oversized update
    assert(fast.callCount == 1); // one call only; no catch-up burst
    float afterLargeStep = SimulationAutoIncidentSecondsUntilNext(&fast);
    assert(afterLargeStep >= 5.0f && afterLargeStep <= 30.0f);

    Simulation slow, quick;
    SimulationInit(&slow);
    SimulationInit(&quick);
    SimulationSetAutoIncidents(&slow, 1, 555u);
    SimulationSetAutoIncidents(&quick, 1, 555u);
    float scheduled = SimulationAutoIncidentSecondsUntilNext(&slow);
    SimulationUpdate(&slow, 0.5f * 0.25f);
    SimulationUpdate(&quick, 0.5f * 8.0f);
    assert(fabsf(slow.elapsedTime - 0.125f) < 0.001f);
    assert(fabsf(quick.elapsedTime - 4.0f) < 0.001f);
    assert(fabsf(SimulationAutoIncidentSecondsUntilNext(&slow) -
                 (scheduled - 0.125f)) < 0.001f);
    assert(fabsf(SimulationAutoIncidentSecondsUntilNext(&quick) -
                 (scheduled - 4.0f)) < 0.001f);
}

static void CheckGeneratedCallsAndModes(void)
{
    int seenTypes[4] = {0};
    Simulation manual;
    SimulationInit(&manual);
    manual.dispatchMode = DISPATCH_MANUAL;
    SimulationSetAutoIncidents(&manual, 1, 98765u);
    for (int i = 0; i < 80; i++) {
        float wait = SimulationAutoIncidentSecondsUntilNext(&manual);
        assert(wait >= 5.0f && wait <= 30.0f);
        SimulationUpdate(&manual, wait + 0.001f);
        assert(manual.callCount == i + 1);
        const EmergencyCall *call = &manual.calls[i];
        assert(call->report[0] != '\0');
        assert(call->trueType >= INCIDENT_FIRE && call->trueType <= INCIDENT_MEDICAL);
        assert(call->trueSeverity >= 1 && call->trueSeverity <= 3);
        assert(call->location.x >= 0.0f && call->location.x <= CITY_LAYOUT_WIDTH);
        assert(call->location.y >= 0.0f && call->location.y <= CITY_LAYOUT_HEIGHT);
        assert(IsBuildingCenter(call->location));
        assert(!call->classified && !call->dispatched);
        assert(!call->overridePending);
        seenTypes[call->trueType] = 1;
        IncidentType suggestion;
        int suggestedSeverity;
        assert(SimulationSuggestCallClassification(&manual, i, &suggestion,
                                                  &suggestedSeverity));
        assert(suggestion == call->trueType);
    }
    for (int type = 0; type < 4; type++) assert(seenTypes[type]);

    Simulation manualFlow;
    SimulationInit(&manualFlow);
    manualFlow.dispatchMode = DISPATCH_MANUAL;
    SimulationSetAutoIncidents(&manualFlow, 1, 42u);
    SimulationUpdate(&manualFlow,
        SimulationAutoIncidentSecondsUntilNext(&manualFlow) + 0.001f);
    assert(manualFlow.callCount == 1 && !manualFlow.calls[0].overridePending);
    assert(SimulationClassifyCall(&manualFlow, 0, manualFlow.calls[0].trueType,
                                  manualFlow.calls[0].trueSeverity));
    Incident *manualIncident = &manualFlow.incidents[manualFlow.calls[0].incidentIndex];
    unsigned int manualMask = 0;
    for (int service = 0; service < SERVICE_COUNT; service++) {
        int needed = manualIncident->requiredUnitsByService[service];
        for (int vehicle = 0; vehicle < manualFlow.vehicleCount && needed > 0; vehicle++) {
            if (!manualFlow.vehicles[vehicle].available ||
                manualFlow.vehicles[vehicle].serviceType != (IncidentType)service) continue;
            manualMask |= 1u << vehicle;
            needed--;
        }
        assert(needed == 0);
    }
    assert(SimulationDispatchCallVehicles(&manualFlow, 0, manualMask) > 0);
    assert(manualFlow.calls[0].dispatched);

    Simulation automatic;
    SimulationInit(&automatic);
    automatic.dispatchMode = DISPATCH_AUTOMATIC;
    SimulationSetAutoIncidents(&automatic, 1, 2u);
    SimulationUpdate(&automatic,
        SimulationAutoIncidentSecondsUntilNext(&automatic) + 0.001f);
    assert(automatic.callCount == 1);
    assert(automatic.calls[0].overridePending);
    assert(!automatic.calls[0].classified && !automatic.calls[0].dispatched);
    // Timer expiry accepts the AI suggestion through the normal operator flow.
    automatic.calls[0].overridePending = 0;
    SimulationProcessOperator(&automatic);
    assert(automatic.calls[0].classified);
    assert(automatic.calls[0].dispatched);
    assert(automatic.calls[0].incidentIndex >= 0);
}

static void CheckReproducibilityAndCapacity(void)
{
    Simulation first, second;
    SimulationInit(&first);
    SimulationInit(&second);
    first.dispatchMode = second.dispatchMode = DISPATCH_MANUAL;
    SimulationSetAutoIncidents(&first, 1, 0xabcdefu);
    SimulationSetAutoIncidents(&second, 1, 0xabcdefu);
    for (int i = 0; i < 10; i++) {
        float a = SimulationAutoIncidentSecondsUntilNext(&first);
        float b = SimulationAutoIncidentSecondsUntilNext(&second);
        assert(a == b);
        SimulationUpdate(&first, a + 0.001f);
        SimulationUpdate(&second, b + 0.001f);
        assert(first.callCount == second.callCount);
        const EmergencyCall *left = &first.calls[i];
        const EmergencyCall *right = &second.calls[i];
        assert(left->trueType == right->trueType);
        assert(left->trueSeverity == right->trueSeverity);
        assert(left->location.x == right->location.x);
        assert(left->location.y == right->location.y);
        assert(strcmp(left->report, right->report) == 0);
    }

    Simulation full;
    SimulationInit(&full);
    full.callCount = MAX_CALLS;
    SimulationSetAutoIncidents(&full, 1, 3u);
    Advance(&full, SimulationAutoIncidentSecondsUntilNext(&full) + 0.02f);
    assert(full.callCount == MAX_CALLS);
    assert(full.autoIncidentCapacitySuspended);
    assert(CountEventsContaining(&full, "capacity is full") == 1);
    for (int i = 0; i < 10; i++) SimulationUpdate(&full, 0.1f);
    assert(CountEventsContaining(&full, "capacity is full") == 1);

    Simulation incidentFull;
    SimulationInit(&incidentFull);
    incidentFull.incidentCount = MAX_INCIDENTS;
    SimulationSetAutoIncidents(&incidentFull, 1, 4u);
    Advance(&incidentFull, SimulationAutoIncidentSecondsUntilNext(&incidentFull) + 0.02f);
    assert(incidentFull.callCount == 0);
    assert(incidentFull.autoIncidentCapacitySuspended);
}

int main(void)
{
    CheckDisabledAndReset();
    CheckIntervalsAndPauseSemantics();
    CheckGeneratedCallsAndModes();
    CheckReproducibilityAndCapacity();
    puts("auto incident scheduling, reports, operator modes, reproducibility, and capacity checks passed");
    return 0;
}
