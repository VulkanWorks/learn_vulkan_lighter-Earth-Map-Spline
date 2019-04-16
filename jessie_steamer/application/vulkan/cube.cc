//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "third_party/glm/glm.hpp"
// different from OpenGL, where depth values are in range [-1.0, 1.0]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "third_party/glm/gtc/matrix_transform.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace cube{
namespace {

namespace util = common::util;
using namespace wrapper::vulkan;
using std::vector;

constexpr size_t kNumFrameInFlight = 2;

class CubeApp {
 public:
  CubeApp() : context_{Context::CreateContext()} {
    context_->Init("Cube");
  };
  void MainLoop();

 private:
  bool is_first_time = true;
  size_t current_frame_ = 0;
  std::shared_ptr<Context> context_;
  Pipeline pipeline_;
  Command command_;
  Model model_;
  UniformBuffer uniform_buffer_;
  DepthStencilImage depth_stencil_;
  vector<Descriptor> descriptors_;

  void Init();
  void Cleanup();
};

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct Transformation {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

vector<Transformation> kTrans;

void UpdateTrans(size_t current_frame, float screen_aspect) {
  static auto start_time = util::Now();
  auto elapsed_time = util::TimeInterval(start_time, util::Now());
  Transformation& trans = kTrans[current_frame];
  trans = {
    glm::rotate(glm::mat4{1.0f}, elapsed_time * glm::radians(90.0f),
                glm::vec3{1.0f, 1.0f, 0.0f}),
    glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}),
    glm::perspective(glm::radians(45.0f), screen_aspect, 0.1f, 100.0f),
  };
  // No need to flip Y-axis as OpenGL
  trans.proj[1][1] *= -1;
}

} /* namespace */

void CubeApp::Init() {
  if (is_first_time) {
    // model (vertex buffer)
    model_.Init(context_->ptr(), /*obj_index_base=*/1,
                "jessie_steamer/resource/model/cube.obj",
                {{"jessie_steamer/resource/texture/statue.jpg"}});

    // uniform buffer
    kTrans.resize(context_->swapchain().size());
    UniformBuffer::Info chunk_info{
        kTrans.data(),
        sizeof(Transformation),
        CONTAINER_SIZE(kTrans),
    };
    uniform_buffer_.Init(context_->ptr(), chunk_info);

    // descriptor
    vector<Descriptor::Info> descriptor_infos{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
         VK_SHADER_STAGE_VERTEX_BIT,
         {{/*binding_point=*/0, /*array_length=*/1}}},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         VK_SHADER_STAGE_FRAGMENT_BIT,
         {{/*binding_point=*/1, /*array_length=*/1}}},
    };
    descriptors_.resize(context_->swapchain().size());
    for (auto& descriptor : descriptors_) {
      descriptor.Init(context_, descriptor_infos);
    }
    uniform_buffer_.UpdateDescriptors(descriptor_infos[0], &descriptors_);
    model_.UpdateDescriptors({descriptor_infos[1]}, &descriptors_);

    is_first_time = false;
  }

  depth_stencil_.Init(context_, context_->swapchain().extent());
  context_->render_pass().Config(depth_stencil_);
  pipeline_.Init(context_->ptr(),
                 {{"jessie_steamer/shader/compiled/simple.vert.spv",
                   VK_SHADER_STAGE_VERTEX_BIT},
                  {"jessie_steamer/shader/compiled/simple.frag.spv",
                   VK_SHADER_STAGE_FRAGMENT_BIT}},
                 descriptors_[0].layout(),
                 Model::binding_descs(), Model::attrib_descs());
  command_.Init(context_->ptr(), kNumFrameInFlight,
                [&](const VkCommandBuffer& command_buffer, size_t image_index) {
    // start render pass
    std::array<VkClearValue, 2> clear_values{};
    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil = {1.0f, 0};  // initial depth value set to 1.0

    VkRenderPassBeginInfo begin_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        /*pNext=*/nullptr,
        *context_->render_pass(),
        context_->render_pass().framebuffer(image_index),
        /*renderArea=*/{
            /*offset=*/{0, 0},
            context_->swapchain().extent(),
        },
        CONTAINER_SIZE(clear_values),
        clear_values.data(),  // used for _OP_CLEAR
    };

    // record commends. options:
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffer, &begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      *pipeline_);
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.layout(), 0, 1,
                            &descriptors_[image_index].set(), 0, nullptr);
    model_.Draw(command_buffer);

    vkCmdEndRenderPass(command_buffer);
  });
}

void CubeApp::Cleanup() {
  command_.Cleanup();
  pipeline_.Cleanup();
}

void CubeApp::MainLoop() {
  Init();
  auto& window = context_->window();
  while (!window.ShouldQuit()) {
    window.PollEvents();
    VkExtent2D extent = context_->swapchain().extent();
    auto update_func = [this, extent](size_t image_index) {
      UpdateTrans(image_index, (float)extent.width / extent.height);
      uniform_buffer_.UpdateData(image_index);
    };
    if (command_.DrawFrame(current_frame_, update_func) != VK_SUCCESS ||
        window.IsResized()) {
      context_->WaitIdle();
      Cleanup();
      context_->Recreate();
      Init();
    }
    current_frame_ = (current_frame_ + 1) % kNumFrameInFlight;
  }
  context_->WaitIdle(); // wait for all async operations finish
}

} /* namespace cube */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, const char* argv[]) {
#ifdef DEBUG
  INSERT_DEBUG_REQUIREMENT(/*overwrite=*/true);
  jessie_steamer::application::vulkan::cube::CubeApp app{};
  app.MainLoop();
#else
  try {
    jessie_steamer::application::vulkan::cube::CubeApp app{};
    app.MainLoop();
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* DEBUG */
  return EXIT_SUCCESS;
}
