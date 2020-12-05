//
//  pipeline.h
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_PIPELINE_H
#define LIGHTER_RENDERER_VK_PIPELINE_H

#include <memory>

#include "lighter/common/util.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

class Pipeline : public renderer::Pipeline {
 public:
  Pipeline(SharedContext context, const GraphicsPipelineDescriptor& descriptor);

  Pipeline(SharedContext context, const ComputePipelineDescriptor& descriptor);

  // This class is neither copyable nor movable.
  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  ~Pipeline() override;

  // Binds to this pipeline. This should be called when 'command_buffer' is
  // recording commands.
  void Bind(const VkCommandBuffer& command_buffer) const;

 private:
  Pipeline(SharedContext context, absl::string_view name,
           VkPipelineBindPoint binding_point)
      : renderer::Pipeline{name}, context_{std::move(FATAL_IF_NULL(context))},
        binding_point_{binding_point} {}

  // Pointer to context.
  const SharedContext context_;

  // Pipeline binding point, either graphics or compute.
  const VkPipelineBindPoint binding_point_;

  // Opaque pipeline layout object.
  VkPipelineLayout layout_;

  // Opaque pipeline object.
  VkPipeline pipeline_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_PIPELINE_H */
