#ifndef RESPONSE_CAMERA_H
#define RESPONSE_CAMERA_H

#include "raylib.h"

#include <math.h>

#define RESPONSE_CAMERA_MIN_DISTANCE 3.2f
#define RESPONSE_CAMERA_MAX_DISTANCE 8.2f
#define RESPONSE_CAMERA_MIN_PITCH 18.0f
#define RESPONSE_CAMERA_MAX_PITCH 65.0f

typedef struct {
    float yawDegrees;
    float pitchDegrees;
    float distance;
} ResponseCameraState;

static inline ResponseCameraState ResponseCameraDefault(void)
{
    return (ResponseCameraState){0.0f, 32.0f, 5.2f};
}

static inline void ResponseCameraReset(ResponseCameraState *camera)
{
    if (camera) *camera = ResponseCameraDefault();
}

static inline float ResponseCameraClamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

static inline float ResponseCameraWrapYaw(float degrees)
{
    while (degrees > 180.0f) degrees -= 360.0f;
    while (degrees < -180.0f) degrees += 360.0f;
    return degrees;
}

static inline void ResponseCameraDrag(ResponseCameraState *camera, float deltaX,
                                      float deltaY)
{
    if (!camera) return;
    camera->yawDegrees = ResponseCameraWrapYaw(camera->yawDegrees + deltaX * 0.32f);
    camera->pitchDegrees = ResponseCameraClamp(camera->pitchDegrees - deltaY * 0.22f,
                                                RESPONSE_CAMERA_MIN_PITCH,
                                                RESPONSE_CAMERA_MAX_PITCH);
}

static inline void ResponseCameraZoom(ResponseCameraState *camera, float wheelDelta)
{
    if (!camera) return;
    camera->distance = ResponseCameraClamp(camera->distance - wheelDelta * 0.72f,
                                            RESPONSE_CAMERA_MIN_DISTANCE,
                                            RESPONSE_CAMERA_MAX_DISTANCE);
}

static inline Camera3D ResponseCameraBuild(const ResponseCameraState *state,
                                          Vector3 vehiclePosition, Vector3 forward,
                                          int firstPerson)
{
    ResponseCameraState fallback = ResponseCameraDefault();
    if (!state) state = &fallback;
    float forwardLength = sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (forwardLength < 0.001f) {
        forward.x = 1.0f;
        forward.z = 0.0f;
    } else {
        forward.x /= forwardLength;
        forward.z /= forwardLength;
    }

    Camera3D camera = {0};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    if (firstPerson) {
        camera.position = (Vector3){vehiclePosition.x + forward.x * 0.45f,
                                    vehiclePosition.y + 1.18f,
                                    vehiclePosition.z + forward.z * 0.45f};
        camera.target = (Vector3){vehiclePosition.x + forward.x * 13.0f,
                                  vehiclePosition.y + 1.0f,
                                  vehiclePosition.z + forward.z * 13.0f};
        return camera;
    }

    float pitchDegrees = ResponseCameraClamp(state->pitchDegrees,
        RESPONSE_CAMERA_MIN_PITCH, RESPONSE_CAMERA_MAX_PITCH);
    float yawDegrees = ResponseCameraWrapYaw(state->yawDegrees);
    float distance = ResponseCameraClamp(state->distance,
        RESPONSE_CAMERA_MIN_DISTANCE, RESPONSE_CAMERA_MAX_DISTANCE);
    float pitch = pitchDegrees * DEG2RAD;
    float yaw = atan2f(-forward.z, -forward.x) + yawDegrees * DEG2RAD;
    float horizontal = distance * cosf(pitch);
    // Keep the selected vehicle as the orbit's fixed focal point. A small
    // forward bias places it just below centre without letting the camera
    // continue looking down the route after the player has orbited sideways.
    camera.target = (Vector3){vehiclePosition.x + forward.x * 0.22f,
                              vehiclePosition.y + 0.96f,
                              vehiclePosition.z + forward.z * 0.22f};
    camera.position = (Vector3){camera.target.x + cosf(yaw) * horizontal,
                                camera.target.y + sinf(pitch) * distance,
                                camera.target.z + sinf(yaw) * horizontal};
    return camera;
}

// Coordinates are already converted to the fixed virtual UI canvas. Reserved
// rectangles keep camera drags away from selectable unit rows and HUD controls.
static inline int ResponseCameraPointerAllowed(Vector2 pointer, Rectangle viewport,
                                               const Rectangle *reserved,
                                               int reservedCount)
{
    if (!CheckCollisionPointRec(pointer, viewport)) return 0;
    for (int i = 0; reserved && i < reservedCount; i++)
        if (CheckCollisionPointRec(pointer, reserved[i])) return 0;
    return 1;
}

#endif
