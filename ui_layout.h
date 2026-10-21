#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "raylib.h"

// Fit the virtual Control Room canvas inside a resizable window without distortion.
static Rectangle FitVirtualViewport(int screenWidth, int screenHeight,
                                    int canvasWidth, int canvasHeight)
{
    if (screenWidth <= 0 || screenHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0)
        return (Rectangle){0};
    float scaleX = (float)screenWidth / (float)canvasWidth;
    float scaleY = (float)screenHeight / (float)canvasHeight;
    float scale = scaleX < scaleY ? scaleX : scaleY;
    float width = (float)canvasWidth * scale;
    float height = (float)canvasHeight * scale;
    return (Rectangle){(screenWidth - width) * 0.5f,
                       (screenHeight - height) * 0.5f, width, height};
}

// Convert window pointer coordinates into the virtual UI coordinate system.
static int ScreenToVirtualPoint(Vector2 screenPoint, Rectangle viewport,
                                int canvasWidth, int canvasHeight, Vector2 *virtualPoint)
{
    if (!virtualPoint || viewport.width <= 0.0f || viewport.height <= 0.0f ||
        !CheckCollisionPointRec(screenPoint, viewport)) return 0;
    virtualPoint->x = (screenPoint.x - viewport.x) * canvasWidth / viewport.width;
    virtualPoint->y = (screenPoint.y - viewport.y) * canvasHeight / viewport.height;
    return 1;
}

#endif
