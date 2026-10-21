#include "ui_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void CheckWindow(int width, int height)
{
    Rectangle viewport = FitVirtualViewport(width, height, 1600, 900);
    Vector2 virtualPoint = {0};
    const Rectangle controls[] = {
        {416,830,72,44}, {806,830,84,44}, {898,830,96,44},
        {1002,830,92,44}, {1102,830,104,44}, {1214,830,86,44},
        {1308,830,86,44}, {1482,830,88,44},
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
    float mapScale = fminf(mapPanel.width / 1100.0f, mapPanel.height / 700.0f);
    Rectangle cityView = {mapPanel.x + (mapPanel.width - 1100.0f * mapScale) * 0.5f,
                          mapPanel.y + (mapPanel.height - 700.0f * mapScale) * 0.5f,
                          1100.0f * mapScale, 700.0f * mapScale};
    Vector2 intendedCity = {550.0f, 350.0f};
    Vector2 mapCanvas = {cityView.x + intendedCity.x * cityView.width / 1100.0f,
                         cityView.y + intendedCity.y * cityView.height / 700.0f};
    Vector2 mapScreen = {viewport.x + mapCanvas.x * viewport.width / 1600.0f,
                         viewport.y + mapCanvas.y * viewport.height / 900.0f};
    assert(ScreenToVirtualPoint(mapScreen, viewport, 1600, 900, &virtualPoint));
    Vector2 roundTrip = {(virtualPoint.x - cityView.x) * 1100.0f / cityView.width,
                         (virtualPoint.y - cityView.y) * 700.0f / cityView.height};
    assert(fabsf(roundTrip.x - intendedCity.x) < 0.01f);
    assert(fabsf(roundTrip.y - intendedCity.y) < 0.01f);
}

int main(void)
{
    CheckWindow(1120, 700);
    CheckWindow(1440, 810);
    CheckWindow(1920, 1080);
    puts("resized-window click coordinate checks passed");
    return 0;
}
