#pragma once

#include <algorithm>

namespace headsup
{
    struct ScreenPoint
    {
        float x = 0.0f, y = 0.0f;
    };

    struct ScreenBox
    {
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
        bool valid = false;

        void Add(float x, float y)
        {
            if (!valid)
            {
                minX = maxX = x;
                minY = maxY = y;
                valid       = true;
                return;
            }
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }

        float Width() const { return maxX - minX; }
        float Height() const { return maxY - minY; }
        float CenterX() const { return (minX + maxX) * 0.5f; }
        float CenterY() const { return (minY + maxY) * 0.5f; }
    };
}
