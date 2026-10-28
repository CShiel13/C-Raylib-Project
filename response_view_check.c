#include "response_selection.h"
#include "response_camera.h"
#include "city_layout.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    Simulation simulation = {0};
    simulation.incidentCount = 1;
    simulation.vehicleCount = 3;

    simulation.vehicles[0] = (Vehicle){
        .serviceType = INCIDENT_FIRE,
        .position = {110.0f, 120.0f},
        .assignedIncident = 0,
        .activity = UNIT_RESPONDING,
        .route = {{150.0f, 120.0f}, {180.0f, 120.0f}},
        .routeCount = 2,
        .routeIndex = 0
    };
    simulation.vehicles[1] = (Vehicle){
        .serviceType = INCIDENT_CRIME,
        .position = {240.0f, 260.0f},
        .assignedIncident = 0,
        .activity = UNIT_RESPONDING,
        .route = {{240.0f, 220.0f}, {240.0f, 190.0f}},
        .routeCount = 2,
        .routeIndex = 0
    };
    simulation.vehicles[2] = (Vehicle){
        .serviceType = INCIDENT_ACCIDENT,
        .position = {300.0f, 300.0f},
        .assignedIncident = -1,
        .activity = UNIT_AVAILABLE,
        .available = 1
    };

    assert(ResponseUnitCount(&simulation, 0) == 2);
    int selected = ResponseFirstViewableUnit(&simulation, 0);
    assert(selected == 0);
    assert(ResponseUnitAt(&simulation, 0, 1) == 1);
    selected = ResponseUnitAt(&simulation, 0, 1);
    assert(selected == 1);
    assert(simulation.vehicles[selected].serviceType == INCIDENT_CRIME);
    assert(ResponseUnitOrdinal(&simulation, 0, selected) == 1);
    Vector3 firstForward = ResponseUnitForward(&simulation.vehicles[0]);
    Vector3 secondForward = ResponseUnitForward(&simulation.vehicles[1]);
    assert(firstForward.x > 0.99f && fabsf(firstForward.z) < 0.01f);
    assert(secondForward.z < -0.99f && fabsf(secondForward.x) < 0.01f);

    ResponseCameraState cameraState = ResponseCameraDefault();
    Camera3D firstCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[0].position),
        firstForward, 0);
    Camera3D secondCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[1].position),
        secondForward, 0);
    assert(fabsf(firstCamera.position.x - secondCamera.position.x) > 0.5f ||
           fabsf(firstCamera.position.z - secondCamera.position.z) > 0.5f);
    assert(fabsf(firstCamera.target.x - secondCamera.target.x) > 0.5f ||
           fabsf(firstCamera.target.z - secondCamera.target.z) > 0.5f);
    assert(firstCamera.target.y == 0.96f && secondCamera.target.y == 0.96f);
    assert(fabsf(firstCamera.target.x -
                 (CityLayoutMapPointToStreet(simulation.vehicles[0].position).x +
                  firstForward.x * 0.22f)) < 0.001f);
    float initialOrbitX = firstCamera.position.x;
    ResponseCameraDrag(&cameraState, 250.0f, 0.0f);
    Camera3D orbitedCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[0].position),
        firstForward, 0);
    assert(fabsf(orbitedCamera.position.x - initialOrbitX) > 0.1f ||
           fabsf(orbitedCamera.position.z - firstCamera.position.z) > 0.1f);
    assert(fabsf(orbitedCamera.target.x - firstCamera.target.x) < 0.001f &&
           fabsf(orbitedCamera.target.z - firstCamera.target.z) < 0.001f);

    ResponseCameraDrag(&cameraState, 10000.0f, -10000.0f);
    assert(cameraState.yawDegrees <= 180.0f && cameraState.yawDegrees >= -180.0f);
    assert(cameraState.pitchDegrees == RESPONSE_CAMERA_MAX_PITCH);
    Camera3D steepCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[0].position),
        firstForward, 0);
    assert(steepCamera.position.y - steepCamera.target.y > 0.0f);
    ResponseCameraDrag(&cameraState, -10000.0f, 10000.0f);
    assert(cameraState.pitchDegrees == RESPONSE_CAMERA_MIN_PITCH);
    Camera3D shallowCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[0].position),
        firstForward, 0);
    assert(shallowCamera.position.y > 1.5f);
    ResponseCameraZoom(&cameraState, 10000.0f);
    assert(cameraState.distance == RESPONSE_CAMERA_MIN_DISTANCE);
    ResponseCameraZoom(&cameraState, -10000.0f);
    assert(cameraState.distance == RESPONSE_CAMERA_MAX_DISTANCE);
    ResponseCameraReset(&cameraState);
    assert(cameraState.yawDegrees == 0.0f && cameraState.pitchDegrees == 32.0f &&
           cameraState.distance == 5.2f);
    Camera3D firstPersonCamera = ResponseCameraBuild(&cameraState,
        CityLayoutMapPointToStreet(simulation.vehicles[0].position),
        firstForward, 1);
    assert(fabsf(firstPersonCamera.position.x -
                 (CityLayoutMapPointToStreet(simulation.vehicles[0].position).x +
                  firstForward.x * 0.45f)) < 0.001f);

    simulation.vehicles[0].activity = UNIT_ON_SCENE;
    simulation.vehicles[0].arrived = 1;
    assert(ResponseUnitCount(&simulation, 0) == 2);
    assert(ResponseUnitAt(&simulation, 0, 0) == 0);

    simulation.vehicles[1].activity = UNIT_AVAILABLE;
    simulation.vehicles[1].available = 1;
    simulation.vehicles[1].assignedIncident = -1;
    assert(ResponseUnitCount(&simulation, 0) == 1);
    assert(ResponseFirstViewableUnit(&simulation, 0) == 0);
    assert(ResponseUnitOrdinal(&simulation, 0, 0) == 0);

    simulation.vehicles[0].activity = UNIT_RETURNING;
    assert(ResponseUnitCount(&simulation, 0) == 0);
    assert(ResponseFirstViewableUnit(&simulation, 0) == -1);
    assert(ResponseUnitAt(&simulation, 0, 0) == -1);
    puts("response unit selection, live route direction, camera limits/reset, and first-person checks passed");
    return 0;
}
