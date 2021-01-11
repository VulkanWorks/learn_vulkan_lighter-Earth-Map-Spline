//
//  renderer.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDERER_H
#define LIGHTER_RENDERER_VK_RENDERER_H

#include <memory>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/renderer.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/pipeline.h"
#include "lighter/renderer/vk/swapchain.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

class Renderer : public renderer::Renderer {
 public:
  Renderer(absl::string_view application_name,
           const absl::optional<debug_message::Config>& debug_message_config,
           std::vector<const common::Window*>&& window_ptrs);

  Renderer(absl::string_view application_name,
           const absl::optional<debug_message::Config>& debug_message_config,
           absl::Span<const common::Window* const> windows)
      : Renderer{application_name, debug_message_config,
                 {windows.begin(), windows.end()}} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void RecreateSwapchain(int window_index);

  /* Buffer */

  std::unique_ptr<renderer::DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const BufferUsage> usages) const override {
    return absl::make_unique<DeviceBuffer>(context_, update_rate, initial_size,
                                           usages);
  }

  /* Image */

  const DeviceImage& GetSwapchainImage(int window_index) const override {
    return swapchains_.at(window_index)->image();
  }

  std::unique_ptr<renderer::DeviceImage> CreateColorImage(
      absl::string_view name, const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ImageUsage> usages) const override {
    return GeneralDeviceImage::CreateColorImage(
        context_, name, dimension, multisampling_mode, high_precision, usages);
  }

  std::unique_ptr<renderer::DeviceImage> CreateColorImage(
      absl::string_view name, const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const override {
    return GeneralDeviceImage::CreateColorImage(context_, name, image,
                                                generate_mipmaps, usages);
  }

  std::unique_ptr<renderer::DeviceImage> CreateDepthStencilImage(
      absl::string_view name, const glm::ivec2& extent,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const override {
    return GeneralDeviceImage::CreateDepthStencilImage(
        context_, name, util::CreateExtent(extent), multisampling_mode, usages);
  }

  /* Pass */

  std::unique_ptr<GraphicsPass> CreateGraphicsPass(
      const GraphicsPassDescriptor& descriptor) const override {
    FATAL("Not implemented yet");
  }

  std::unique_ptr<ComputePass> CreateComputePass(
      const ComputePassDescriptor& descriptor) const override {
    FATAL("Not implemented yet");
  }

 private:
  const SharedContext context_;

  std::vector<std::unique_ptr<Swapchain>> swapchains_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_RENDERER_H */
