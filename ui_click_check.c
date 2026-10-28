#include "ui_layout.h"
#include "city_layout.h"
#include "response_camera.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void CheckWindow(int width, int height)
{
    Rectangle viewport = FitVirtualViewport(width, height, 1600, 900);
    Vector2 virtualPoint = {0};
    const Rectangle controls[] = {
        {332,830,76,44}, {416,830,72,44}, {496,830,72,44},
        {576,830,64,44}, {648,830,72,44}, {728,830,70,44},
        {806,830,84,44}, {898,830,96,44},
        {1002,830,92,44}, {1102,830,104,44}, {1214,830,86,44},
        {1308,830,86,44}, {1482,830,88,44},
        {1038,111,174,30}, {805,746,58,30}, {900,746,58,30},
        {962,746,148,30}, {1116,746,96,30}, {26,158,254,74},
        {550,340,500,48}, {550,405,500,48}, {550,470,500,48},
        {1260,244,148,23}, {1418,244,148,23},
        {1260,271,148,23}, {1418,271,148,23},
        {1260,300,64,22}, {1330,300,64,22}, {1400,300,64,22},
        {1260,328,306,27}, {1258,326,308,18}, {1260,412,306,28},
        {470,625,210,54}, {695,625,210,54}, {920,625,210,54},
        {430,175,740,550}
    };
    for (int i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
        Vector2 intended = {controls[i].x + controls[i].width * 0.5f,
                            controls[i].y + controls[i].height * 0.5f};
        Vector2 screenPoint = {viewport.x + intended.x * viewport.width / 1600.0f,
                               viewport.y + intended.y * viewport.height / 900.0f};
        assert(ScreenToVirtualPoint(screenPoint, viewport, 1600, 900, &virtualPoint));
        assert(fabsf(virtualPoint.x - intended.x) < 0.01f);
        assert(fabsf(virtualPoint.y - intended.y) < 0.01f);
        assert(CheckCollisionPointRec(virtualPoint, controls[i]));
    }
    if (viewport.x > 0.0f)
        assert(!ScreenToVirtualPoint((Vector2){viewport.x - 1.0f, viewport.y + 1.0f},
                                     viewport, 1600, 900, &virtualPoint));
    if (viewport.y > 0.0f)
        assert(!ScreenToVirtualPoint((Vector2){viewport.x + 1.0f, viewport.y - 1.0f},
                                     viewport, 1600, 900, &virtualPoint));

    Rectangle mapPanel = {300, 104, 930, 700};
    float mapScale = fminf(mapPanel.width / (float)CITY_LAYOUT_WIDTH, mapPanel.height / 700.0f);
    Rectangle cityView = {mapPanel.x + (mapPanel.width - (float)CITY_LAYOUT_WIDTH * mapScale) * 0.5f,
                          mapPanel.y + (mapPanel.height - 700.0f * mapScale) * 0.5f,
                          (float)CITY_LAYOUT_WIDTH * mapScale, 700.0f * mapScale};
    Vector2 intendedCity = {550.0f, 350.0f};
    Vector2 mapCanvas = {cityView.x + intendedCity.x * cityView.width / (float)CITY_LAYOUT_WIDTH,
                         cityView.y + intendedCity.y * cityView.height / 700.0f};
    Vector2 mapScreen = {viewport.x + mapCanvas.x * viewport.width / 1600.0f,
                         viewport.y + mapCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(mapScreen, viewport, 1600, 900, &virtualPoint));
    Vector2 roundTrip = {(virtualPoint.x - cityView.x) * (float)CITY_LAYOUT_WIDTH / cityView.width,
                         (virtualPoint.y - cityView.y) * 700.0f / cityView.height};
    assert(fabsf(roundTrip.x - intendedCity.x) < 0.01f);
    assert(fabsf(roundTrip.y - intendedCity.y) < 0.01f);

    // Camera drag and wheel input use the same converted virtual coordinates
    // and ignore the unit selector/HUD controls drawn over the 3D viewport.
    Rectangle responseViewport = {310, 150, 910, 642};
    Rectangle reserved[] = {
        {322,163,226,25}, {795,746,58,30}, {900,746,58,30},
        {962,746,148,30}, {1116,746,96,30}
    };
    Vector2 cameraCanvas = {700.0f, 420.0f};
    Vector2 cameraScreen = {viewport.x + cameraCanvas.x * viewport.width / 1600.0f,
                            viewport.y + cameraCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(cameraScreen, viewport, 1600, 900, &virtualPoint));
    assert(ResponseCameraPointerAllowed(virtualPoint, responseViewport,
                                        reserved, (int)(sizeof(reserved) / sizeof(reserved[0]))));
    Vector2 controlCanvas = {1164.0f, 761.0f};
    Vector2 controlScreen = {viewport.x + controlCanvas.x * viewport.width / 1600.0f,
                             viewport.y + controlCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(controlScreen, viewport, 1600, 900, &virtualPoint));
    assert(!ResponseCameraPointerAllowed(virtualPoint, responseViewport,
                                        reserved, (int)(sizeof(reserved) / sizeof(reserved[0]))));
}

int main(void)
{
    CheckWindow(1120, 700);
    CheckWindow(1440, 810);
    CheckWindow(1920, 1080);
    puts("resized-window click coordinate checks passed");
    return 0;
}
