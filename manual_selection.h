#ifndef MANUAL_SELECTION_H
#define MANUAL_SELECTION_H

#include "simulation.h"

#define MANUAL_VEHICLE_ROW_COUNT MAX_VEHICLES

static Rectangle ManualClassifyButtonBounds(void)
{
    return (Rectangle){1224.0f, 430.0f, 350.0f, 42.0f};
}

// The classify button occupies this area only before classification. Its
// rectangle overlaps the first unit row, so never let it intercept unit clicks
// after the call has been classified.
static int ManualClassifyButtonHit(const Simulation *simulation, int callIndex,
                                   Vector2 mouse)
{
    if (!simulation || callIndex < 0 || callIndex >= simulation->callCount) return 0;
    const EmergencyCall *call = &simulation->calls[callIndex];
    return !call->classified && !call->dispatched &&
           CheckCollisionPointRec(mouse, ManualClassifyButtonBounds());
}

// These rectangles are in the same virtual UI coordinates used to draw the
// details panel. Pointer input is converted to this coordinate space once.
static Rectangle ManualVehicleRowBounds(int row)
{
    return (Rectangle){1222.0f, 326.0f + (float)row * 30.0f, 350.0f, 27.0f};
}

static Rectangle ManualVehicleCheckboxBounds(int row)
{
    return (Rectangle){1507.0f, 329.0f + (float)row * 30.0f, 21.0f, 21.0f};
}

// Produce the displayed ordering once so drawing and hit testing map a row to
// the same vehicle, even when unsuitable vehicle types are interleaved.
static int ManualVehicleRows(const Simulation *simulation, const Incident *incident,
                             int vehicleIndices[MANUAL_VEHICLE_ROW_COUNT])
{
    if (!simulation || !incident || !vehicleIndices) return 0;
    int rowCount = 0;
    for (int i = 0; i < simulation->vehicleCount && rowCount < MANUAL_VEHICLE_ROW_COUNT; i++) {
        IncidentType service = simulation->vehicles[i].serviceType;
        if (service < INCIDENT_FIRE || service >= SERVICE_COUNT ||
            incident->requiredUnitsByService[service] <= 0) continue;
        vehicleIndices[rowCount++] = i;
    }
    return rowCount;
}

static int ManualVehicleSelectedForService(unsigned int mask, const Simulation *simulation,
                                           IncidentType service)
{
    int count = 0;
    for (int i = 0; i < simulation->vehicleCount; i++)
        if ((mask & (1u << i)) && simulation->vehicles[i].serviceType == service) count++;
    return count;
}

static unsigned int ManualVehicleToggleSelection(const Simulation *simulation,
                                                  const Incident *incident, int incidentIndex,
                                                  int row,
                                                  Vector2 mouse,
                                                  unsigned int selectedMask)
{
    if (!simulation || !incident || row < 0 || row >= MANUAL_VEHICLE_ROW_COUNT)
        return selectedMask;
    if (!CheckCollisionPointRec(mouse, ManualVehicleRowBounds(row)) &&
        !CheckCollisionPointRec(mouse, ManualVehicleCheckboxBounds(row)))
        return selectedMask;

    int vehicleIndices[MANUAL_VEHICLE_ROW_COUNT];
    int rowCount = ManualVehicleRows(simulation, incident, vehicleIndices);
    if (row >= rowCount) return selectedMask;

    int vehicleIndex = vehicleIndices[row];
    if (vehicleIndex < 0 || vehicleIndex >= simulation->vehicleCount ||
        !simulation->vehicles[vehicleIndex].available)
        return selectedMask;

    unsigned int bit = 1u << vehicleIndex;
    if (selectedMask & bit) return selectedMask & ~bit;
    IncidentType service = simulation->vehicles[vehicleIndex].serviceType;
    int alreadyAssigned = CountAssignedUnitsByService(simulation, incidentIndex, service);
    int remainingForService = incident->requiredUnitsByService[service] - alreadyAssigned;
    if (remainingForService > 0 &&
        ManualVehicleSelectedForService(selectedMask, simulation, service) < remainingForService)
        return selectedMask | bit;
    return selectedMask;
}

#endif
