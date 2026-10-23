#include "../outputs/manual_selection.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    Simulation simulation;
    SimulationInit(&simulation);
    simulation.dispatchMode = DISPATCH_MANUAL;
    assert(SimulationCreateCall(&simulation, (Vector2){600, 400}, "Building fire"));
    int callIndex = 0;
    Rectangle firstRow = ManualVehicleRowBounds(0);
    Vector2 clickFirstRow = {firstRow.x + 35.0f, firstRow.y + firstRow.height * 0.5f};
    assert(ManualClassifyButtonHit(&simulation, callIndex, clickFirstRow));
    assert(SimulationClassifyCall(&simulation, callIndex, INCIDENT_FIRE, 2));
    assert(!ManualClassifyButtonHit(&simulation, callIndex, clickFirstRow));

    unsigned int selected = 0;
    Rectangle firstBox = ManualVehicleCheckboxBounds(0);
    Vector2 clickFirstCheckbox = {firstBox.x + firstBox.width * 0.5f,
                                  firstBox.y + firstBox.height * 0.5f};
    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 0,
                                             clickFirstCheckbox, selected, 2);
    assert(selected == (1u << 0));

    Rectangle secondRow = ManualVehicleRowBounds(1);
    Vector2 clickSecondRow = {secondRow.x + 35.0f, secondRow.y + secondRow.height * 0.5f};
    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 1,
                                             clickSecondRow, selected, 2);
    assert(selected == ((1u << 0) | (1u << 1)));

    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 0,
                                             clickFirstRow, selected, 2);
    assert(selected == (1u << 1));

    Rectangle secondBox = ManualVehicleCheckboxBounds(1);
    Vector2 clickSecondCheckbox = {secondBox.x + secondBox.width * 0.5f,
                                   secondBox.y + secondBox.height * 0.5f};
    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 1,
                                             clickSecondCheckbox, selected, 2);
    assert(selected == 0);

    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 0,
                                             clickFirstCheckbox, selected, 2);
    selected = ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 1,
                                             clickSecondCheckbox, selected, 2);
    assert(ManualVehicleSelectedCount(selected) == 2);
    Rectangle thirdRow = ManualVehicleRowBounds(2);
    assert(ManualVehicleToggleSelection(&simulation, INCIDENT_FIRE, 2,
                                         (Vector2){thirdRow.x + 35.0f, thirdRow.y + thirdRow.height * 0.5f},
                                         selected, 2) == selected);
    assert(!SimulationDispatchCallVehicles(&simulation, callIndex, (1u << 0) | (1u << 3)));
    assert(SimulationDispatchCallVehicles(&simulation, callIndex, selected));
    assert(simulation.vehicles[0].available == 0);
    assert(simulation.vehicles[1].available == 0);
    puts("manual vehicle row selection checks passed");
    return 0;
}
