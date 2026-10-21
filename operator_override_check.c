#include "../outputs/simulation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void CheckOverrideServicePipeline(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_AUTOMATIC;

    assert(SimulationCreateCall(&sim, (Vector2){640, 350},
        "Two vehicles collided; one lane blocked and injuries unclear."));
    assert(SimulationSetCallTruth(&sim, 0, INCIDENT_ACCIDENT, 2));
    EmergencyCall *call = &sim.calls[0];
    call->overridePending = 1;

    IncidentType suggested;
    int severity;
    assert(SimulationSuggestCallClassification(&sim, 0, &suggested, &severity));
    assert(suggested == INCIDENT_ACCIDENT && severity == 2);
    SimulationProcessOperator(&sim);
    assert(!call->classified && !call->dispatched);
    assert(sim.incidentCount == 0); /* queued calls do not advance before input */

    /* A wrong fire override dispatches only fire units at first. It must not
       report feedback or send ambulances before the wrong team arrives. */
    call->overridePending = 0;
    call->operatorOverride = 1;
    call->operatorChoiceType = INCIDENT_FIRE;
    call->operatorWrongService = 1;
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, severity));
    SimulationProcessOperator(&sim);
    assert(call->classified && call->dispatched);
    assert(call->classifiedType == INCIDENT_FIRE);
    assert(call->trueType == INCIDENT_ACCIDENT);
    assert(!call->feedbackReady);
    assert(SimulationServiceForIncident(call->trueType) == INCIDENT_ACCIDENT);
    assert(CountAssignedUnits(&sim, call->incidentIndex) == 2);
    for (int i = 0; i < sim.vehicleCount; i++)
        if (sim.vehicles[i].assignedIncident == call->incidentIndex)
            assert(sim.vehicles[i].serviceType == INCIDENT_FIRE);

    for (int step = 0; step < 300 && !call->mismatchRecorded; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(call->mismatchRecorded);
    assert(sim.incidents[call->incidentIndex].type == INCIDENT_ACCIDENT);
    assert(call->dispatched);
    assert(CountAssignedUnits(&sim, call->incidentIndex) == 2);
    for (int i = 0; i < sim.vehicleCount; i++) {
        if (sim.vehicles[i].assignedIncident == call->incidentIndex)
            assert(sim.vehicles[i].serviceType == INCIDENT_ACCIDENT);
        else if (sim.vehicles[i].serviceType == INCIDENT_FIRE)
            assert(sim.vehicles[i].available);
    }
    assert(!call->feedbackReady); /* mismatch feedback waits for final outcome */
    for (int step = 0; step < 400 && !call->feedbackReady; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(call->feedbackReady && !call->callerSatisfied);

    /* Multiple incoming calls remain queued one at a time until each is
       accepted; processing the first must not silently classify the next. */
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_AUTOMATIC;
    assert(SimulationCreateCall(&sim, (Vector2){600, 300}, "Smoke in a small building."));
    assert(SimulationCreateCall(&sim, (Vector2){700, 300}, "Person collapsed and unresponsive."));
    sim.calls[0].overridePending = 1;
    sim.calls[1].overridePending = 1;
    SimulationProcessOperator(&sim);
    assert(!sim.calls[0].classified && !sim.calls[1].classified);
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 1));
    sim.calls[0].overridePending = 0;
    SimulationProcessOperator(&sim);
    assert(sim.calls[0].dispatched);
    assert(!sim.calls[1].classified && sim.calls[1].overridePending);
}

static void CheckCorrectChoiceFeedbackAtCompletion(void)
{
    Simulation sim;
    SimulationInit(&sim);
    assert(SimulationCreateCall(&sim, (Vector2){780, 280},
        "Two vehicles collided; one lane blocked."));
    assert(SimulationSetCallTruth(&sim, 0, INCIDENT_ACCIDENT, 1));
    EmergencyCall *call = &sim.calls[0];
    call->operatorOverride = 1;
    call->operatorChoiceType = INCIDENT_ACCIDENT;
    call->operatorWrongService = 0;
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_ACCIDENT, 1));
    SimulationProcessOperator(&sim);
    assert(call->dispatched && !call->feedbackReady);
    for (int step = 0; step < 400 && !call->feedbackReady; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(call->feedbackReady && call->callerSatisfied);
}

static void CheckTimeoutSuggestionAndFailedFeedback(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_AUTOMATIC;
    assert(SimulationCreateCall(&sim, (Vector2){320, 280},
        "Flames spreading; caller reports people trapped inside."));
    assert(SimulationSetCallTruth(&sim, 0, INCIDENT_FIRE, 3));
    IncidentType aiType;
    int aiSeverity;
    assert(SimulationSuggestCallClassification(&sim, 0, &aiType, &aiSeverity));
    assert(aiType == INCIDENT_FIRE && aiSeverity == 3);
    sim.calls[0].operatorOverride = 1; /* timeout accepts the AI suggestion */
    sim.calls[0].operatorChoiceType = aiType;
    assert(SimulationClassifyCall(&sim, 0, aiType, aiSeverity));
    SimulationProcessOperator(&sim);
    assert(sim.calls[0].dispatched && !sim.calls[0].feedbackReady);

    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&sim, (Vector2){600, 400}, "Caller reports a break-in."));
    assert(SimulationSetCallTruth(&sim, 0, INCIDENT_CRIME, 1));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_CRIME, 1));
    for (int step = 0; step < 320 && !sim.calls[0].feedbackReady; step++)
        SimulationUpdate(&sim, 0.1f);
    assert(sim.incidents[0].status == INCIDENT_FAILED);
    assert(sim.calls[0].feedbackReady && !sim.calls[0].callerSatisfied);
}

static void CheckMultiUnitAndUnavailableRetry(void)
{
    Simulation sim;
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_AUTOMATIC;
    assert(SimulationCreateCall(&sim, (Vector2){650, 380},
        "Flames spreading; caller reports people trapped inside."));
    assert(SimulationClassifyCall(&sim, 0, INCIDENT_FIRE, 3));
    SimulationProcessOperator(&sim);
    assert(sim.calls[0].dispatched);
    assert(CountAssignedUnits(&sim, sim.calls[0].incidentIndex) == 3);

    assert(SimulationCreateCall(&sim, (Vector2){700, 420},
        "Smoke from a building fire."));
    assert(SimulationClassifyCall(&sim, 1, INCIDENT_FIRE, 1));
    SimulationProcessOperator(&sim);
    assert(!sim.calls[1].dispatched);
    assert(strstr(sim.calls[1].decisionReason, "Waiting and retrying") != NULL);

    /* Existing Manual mode still leaves incoming calls for the operator. */
    SimulationInit(&sim);
    sim.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&sim, (Vector2){600, 400}, "Caller reports smoke."));
    SimulationProcessOperator(&sim);
    assert(!sim.calls[0].classified && !sim.calls[0].dispatched);
}

int main(void)
{
    CheckOverrideServicePipeline();
    CheckCorrectChoiceFeedbackAtCompletion();
    CheckTimeoutSuggestionAndFailedFeedback();
    CheckMultiUnitAndUnavailableRetry();
    puts("operator override pipeline checks passed");
    return 0;
}
