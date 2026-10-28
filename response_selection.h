#ifndef RESPONSE_SELECTION_H
#define RESPONSE_SELECTION_H

#include "simulation.h"

#include <math.h>

// A responder can be followed while travelling to an incident or working at
// the scene. Returning, transporting, and available units are not responders
// for this view.
static int ResponseUnitIsViewable(const Vehicle *vehicle, int incidentIndex)
{
    return vehicle->assignedIncident == incidentIndex &&
           (vehicle->activity == UNIT_RESPONDING || vehicle->activity == UNIT_ON_SCENE);
}

static int ResponseUnitCount(const Simulation *simulation, int incidentIndex)
{
    if (!simulation || incidentIndex < 0 || incidentIndex >= simulation->incidentCount)
        return 0;
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if (ResponseUnitIsViewable(&simulation->vehicles[i], incidentIndex)) count++;
    return count;
}

static int ResponseUnitAt(const Simulation *simulation, int incidentIndex, int ordinal)
{
    if (!simulation || incidentIndex < 0 || incidentIndex >= simulation->incidentCount ||
        ordinal < 0) return -1;
    int found = 0;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (!ResponseUnitIsViewable(&simulation->vehicles[i], incidentIndex)) continue;
        if (found++ == ordinal) return i;
    }
    return -1;
}

static int ResponseUnitOrdinal(const Simulation *simulation, int incidentIndex,
                               int vehicleIndex)
{
    if (!simulation || vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount)
        return -1;
    int ordinal = 0;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        if (!ResponseUnitIsViewable(&simulation->vehicles[i], incidentIndex)) continue;
        if (i == vehicleIndex) return ordinal;
        ordinal++;
    }
    return -1;
}

static int ResponseFirstViewableUnit(const Simulation *simulation, int incidentIndex)
{
    if (ResponseUnitCount(simulation, incidentIndex) <= 0) return -1;
    for (int i = 0; i < simulation->vehicleCount; i++) {
        const Vehicle *vehicle = &simulation->vehicles[i];
        if (ResponseUnitIsViewable(vehicle, incidentIndex) &&
            vehicle->activity == UNIT_RESPONDING && !vehicle->arrived)
            return i;
    }
    return ResponseUnitAt(simulation, incidentIndex, 0);
}

// Direction is derived from this vehicle's current simulation position and
// next route point, so switching the selected index also switches camera aim.
static Vector3 ResponseUnitForward(const Vehicle *vehicle)
{
    Vector2 from = vehicle->position;
    Vector2 to = {from.x + 1.0f, from.y};
    if (vehicle->routeIndex < vehicle->routeCount) {
        to = vehicle->route[vehicle->routeIndex];
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        if (dx * dx + dy * dy < 0.01f && vehicle->routeIndex + 1 < vehicle->routeCount)
            to = vehicle->route[vehicle->routeIndex + 1];
    } else if (vehicle->routeCount >= 2) {
        from = vehicle->route[vehicle->routeCount - 2];
        to = vehicle->route[vehicle->routeCount - 1];
    }

    Vector3 forward = {to.x - from.x, 0.0f, to.y - from.y};
    float length = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (length < 0.001f) return (Vector3){1.0f, 0.0f, 0.0f};
    return (Vector3){forward.x / length, 0.0f, forward.z / length};
}

#endif
