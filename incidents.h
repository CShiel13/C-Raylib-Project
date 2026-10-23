#ifndef INCIDENTS_H
#define INCIDENTS_H

#include "raylib.h"

#define MAX_INCIDENTS 100
#define MAX_VEHICLES 9
#define SERVICE_COUNT 3
#define MAX_CALLS 100
#define CALL_REPORT_LENGTH 128
#define CALL_REASON_LENGTH 192
#define MAX_SIMULATION_EVENTS 32
#define SIMULATION_EVENT_LENGTH 128
#define MAX_CIVILIAN_CARS 24
#define MAX_ROUTE_POINTS 16
#define MAX_PEDESTRIANS 12
#define MAX_PEDESTRIAN_ROUTE_POINTS 16

typedef enum {
    INCIDENT_FIRE = 0,
    INCIDENT_CRIME,
    INCIDENT_ACCIDENT,
    INCIDENT_MEDICAL
} IncidentType;

typedef enum {
    INCIDENT_PENDING = 0,
    INCIDENT_DISPATCHED,
    INCIDENT_RESPONDING,
    INCIDENT_RESOLVED,
    INCIDENT_FAILED,
    INCIDENT_SECURING,
    INCIDENT_FIREFIGHTING,
    INCIDENT_TREATING,
    INCIDENT_TRANSPORTING
} IncidentStatus;

typedef enum {
    PEDESTRIAN_WALKING = 0,
    PEDESTRIAN_WAITING_TO_CROSS,
    PEDESTRIAN_CROSSING,
    PEDESTRIAN_EVACUATING,
    PEDESTRIAN_BYSTANDING,
    PEDESTRIAN_RESPECTING_CORDON,
    PEDESTRIAN_YIELDING
} PedestrianState;

typedef enum {
    TRAFFIC_LIGHT = 0,
    TRAFFIC_MEDIUM,
    TRAFFIC_HEAVY,
    TRAFFIC_LEVEL_COUNT
} TrafficLevel;

typedef enum {
    CITY_CLEAR = 0,
    CITY_RAIN,
    CITY_FOG,
    CITY_WIND,
    CITY_NIGHT,
    CITY_CONDITION_COUNT
} CityCondition;

typedef enum {
    UNIT_AVAILABLE = 0,
    UNIT_RESPONDING,
    UNIT_ON_SCENE,
    UNIT_TRANSPORTING,
    UNIT_RETURNING
} VehicleActivity;

typedef struct {
    IncidentType type;
    IncidentStatus status;
    Vector2 position;
    float ageTime;
    float waitTime;
    float travelTime;
    float responseTime;
    float workElapsed;
    float workRequired;
    float deadline;
    int severity;
    // requiredUnits remains the primary service count for compatibility.
    int requiredUnits;
    int requiredUnitsByService[SERVICE_COUNT];
    int totalRequiredUnits;
    int needsSceneSecurity;
    int requiresTransport;
    int sceneSecured;
    int primaryWorkComplete;
    int patientCareComplete;
    int transportComplete;
    float fireRiskElapsed;
    int fireSpreadOccurred;
    int fireContained;
    unsigned int pedestrianReactionMask;
    int responseRecorded;
    int callIndex;
} Incident;

typedef struct {
    IncidentType serviceType;
    Vector2 position;
    Vector2 stationPosition;
    float speed;
    int available;
    int assignedIncident;
    int arrived;
    VehicleActivity activity;
    int patientDelivered;
    Vector2 route[MAX_ROUTE_POINTS];
    int routeCount;
    int routeIndex;
    int trafficDelayLogged;
} Vehicle;

typedef struct {
    Vector2 position;
    float speed;
    float currentSpeed;
    float progress;
    float headingDegrees;
    float sirenReactionDelay;
    float sirenReactionTimer;
    float yieldTime;
    int fromNode;
    int toNode;
    int laneIndex;
    int sirenSource;
    int routeCursor;
} CivilianCar;

typedef struct {
    Vector2 position;
    float speed;
    int sidewalk;
    int direction;
    int evacuating;
    int reactionLogged;
    PedestrianState state;
    int behaviorIncident;
    int routeIndex;
    int targetNode;
    int crossingActive;
    float routeProgress;
} Pedestrian;

typedef struct {
    Vector2 location;
    float receivedAt;
    float classifiedAt;
    float dispatchedAt;
    IncidentType trueType;
    int trueSeverity;
    IncidentType classifiedType;
    int severity;
    int classified;
    int dispatched;
    int overridePending;
    int operatorOverride;
    IncidentType operatorChoiceType;
    int operatorWrongService;
    int mismatchRecorded;
    int feedbackReady;
    int feedbackShown;
    int callerSatisfied;
    int incidentIndex;
    char report[CALL_REPORT_LENGTH];
    char decisionReason[CALL_REASON_LENGTH];
} EmergencyCall;

typedef struct {
    float time;
    char message[SIMULATION_EVENT_LENGTH];
} SimulationEvent;

#endif
