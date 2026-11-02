#ifndef MANUAL_DISPATCH_H
#define MANUAL_DISPATCH_H

#include "simulation.h"

// Shared readiness rule for the manual operator's assignment panel and controls.
static int ManualCallNeedsVehicleAssignment(const Simulation* simulation, int callIndex)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount) return 0;
    const EmergencyCall* call = &simulation->calls[callIndex];
    int incidentIndex = call->incidentIndex;
    if (!call->classified || incidentIndex < 0 || incidentIndex >= simulation->incidentCount)
        return 0;
    const Incident* incident = &simulation->incidents[incidentIndex];
    if (incident->status == INCIDENT_RESOLVED || incident->status == INCIDENT_FAILED) return 0;
    return CountAssignedUnits(simulation, incidentIndex) < CountRequiredUnits(incident);
}

#endif
