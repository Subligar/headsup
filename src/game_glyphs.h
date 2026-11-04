#pragma once

#include <cstdint>

namespace headsup
{
    // D3DPRIMITIVETYPE's values: this file is built without the Direct3D headers.
    enum PrimitiveType : uint32_t
    {
        kPointList = 1,
        kLineList,
        kLineStrip,
        kTriangleList,
        kTriangleStrip,
        kTriangleFan,
    };
    // Every type counts: the caller reads this many vertices from the draw.
    uint32_t VertexCount(uint32_t primitiveType, uint32_t primitiveCount);

    constexpr uint32_t kQuadVertices = 4;
}
