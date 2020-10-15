//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_RENDERER_H
#define LIGHTER_RENDERER_RENDERER_H

#include <memory>

#include "lighter/renderer/buffer.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

class Renderer {
 public:
  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  ~Renderer() = default;

  /* Host buffer */

  virtual std::unique_ptr<HostBuffer> CreateHostBuffer(size_t size) const = 0;

  template <typename DataType>
  std::unique_ptr<HostBuffer> CreateHostBuffer(int num_chunks) const {
    return CreateHostBuffer(sizeof(DataType) * num_chunks);
  }

  /* Device buffer */

  virtual std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size) const = 0;

  template <typename DataType>
  std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, int num_chunks) const {
    return CreateDeviceBuffer(update_rate, sizeof(DataType) * num_chunks);
  }

  /* Buffer view */

  virtual VertexBufferView CreateVertexBufferView(
      VertexBufferView::InputRate input_rate, int buffer_binding, size_t stride,
      absl::Span<VertexBufferView::Attribute> attributes) const = 0;

  virtual UniformBufferView CreateUniformBufferView(size_t chunk_size,
                                                    int num_chunks) const = 0;

  template <typename DataType>
  UniformBufferView CreateUniformBufferView(int num_chunks) const {
    return CreateUniformBufferView(sizeof(DataType), num_chunks);
  }
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_RENDERER_H */
