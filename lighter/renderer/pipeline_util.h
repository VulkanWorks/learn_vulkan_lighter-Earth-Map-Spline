//
//  pipeline_util.h
//
//  Created by Pujun Lun on 12/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PIPELINE_UTIL_H
#define LIGHTER_RENDERER_PIPELINE_UTIL_H

#include "lighter/renderer/pipeline.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {
namespace pipeline {

// Returns a stencil test that is never passed by any pixel, and does not write
// anything to the stencil buffer.
GraphicsPipelineDescriptor::StencilTestOneFace GetStencilNop();

// Returns a stencil test that compares buffer value with 'reference' using
// 'compare_op', and does not write anything to the stencil buffer.
GraphicsPipelineDescriptor::StencilTestOneFace GetStencilRead(
    CompareOp compare_op, unsigned int reference);

// Returns a stencil test that writes 'reference' value to the stencil buffer
// wherever a pixel passes depth test.
GraphicsPipelineDescriptor::StencilTestOneFace GetStencilWrite(
    unsigned int reference);

// Returns a viewport transform targeting the full frame of 'frame_size'.
GraphicsPipelineDescriptor::Viewport GetFullFrameViewport(
    const glm::ivec2& frame_size);

// Returns a viewport transform that keeps the aspect ratio of objects
// unchanged, and fills the frame as much as possible.
GraphicsPipelineDescriptor::Viewport GetViewport(const glm::ivec2& frame_size,
                                                 float aspect_ratio);

// Returns a scissor that does not clip any part of the frame.
GraphicsPipelineDescriptor::Scissor GetFullFrameScissor(
    const glm::ivec2& frame_size);

} /* namespace pipeline */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PIPELINE_UTIL_H */
