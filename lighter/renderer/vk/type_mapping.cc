//
//  type_mapping.cc
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/type_mapping.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace type {

VkVertexInputRate ConvertVertexInputRate(VertexInputRate rate) {
  switch (rate) {
    case VertexInputRate::kVertex:
      return VK_VERTEX_INPUT_RATE_VERTEX;
    case VertexInputRate::kInstance:
      return VK_VERTEX_INPUT_RATE_INSTANCE;
  }
}

VkFormat ConvertDataFormat(DataFormat format) {
  switch (format) {
    case DataFormat::kSFloat32:
      return VK_FORMAT_R32_SFLOAT;
    case DataFormat::kSFloat32Vec2:
      return VK_FORMAT_R32G32_SFLOAT;
    case DataFormat::kSFloat32Vec3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case DataFormat::kSFloat32Vec4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
  }
}

VkPrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology) {
  switch (topology) {
    case PrimitiveTopology::kPointList:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveTopology::kLineList:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveTopology::kLineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveTopology::kTriangleList:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::kTriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case PrimitiveTopology::kTriangleFan:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  }
}

} /* namespace type */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
