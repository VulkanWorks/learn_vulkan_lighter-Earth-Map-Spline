//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_RENDERER_H
#define LIGHTER_RENDERER_RENDERER_H

#include <memory>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/buffer.h"
#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/pass.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {

class Renderer {
 public:
  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  virtual ~Renderer() = default;

  /* Host buffer */

  std::unique_ptr<HostBuffer> CreateHostBuffer(size_t size) const {
    return absl::make_unique<HostBuffer>(size);
  }

  template <typename DataType>
  std::unique_ptr<HostBuffer> CreateHostBuffer(int num_chunks) const {
    return CreateHostBuffer(sizeof(DataType) * num_chunks);
  }

  /* Device buffer */

  virtual std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const BufferUsage> usages) const = 0;

  template <typename DataType>
  std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, int num_chunks,
      absl::Span<const BufferUsage> usages) const {
    return CreateDeviceBuffer(update_rate, sizeof(DataType) * num_chunks,
                              usages);
  }

  /* Device image */

  virtual const DeviceImage& GetSwapchainImage(int window_index) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateColorImage(
      absl::string_view name, const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateColorImage(
      absl::string_view name, const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateDepthStencilImage(
      absl::string_view name, const glm::ivec2& extent,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const = 0;

  /* Pass */

  virtual std::unique_ptr<GraphicsPass> CreateGraphicsPass(
      const GraphicsPassDescriptor& descriptor) const = 0;

  virtual std::unique_ptr<ComputePass> CreateComputePass(
      const ComputePassDescriptor& descriptor) const = 0;

 protected:
  explicit Renderer(std::vector<const common::Window*>&& windows)
      : windows_{std::move(windows)} {
    for (int i = 0; i < num_windows(); ++i) {
      ASSERT_NON_NULL(windows_[i], absl::StrFormat("Window %d is nullptr", i));
    }
  }

  const std::vector<const common::Window*>& windows() const { return windows_; }
  int num_windows() const { return windows_.size(); }

 private:
  std::vector<const common::Window*> windows_;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_RENDERER_H */
