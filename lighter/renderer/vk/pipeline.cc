//
//  pipeline.cc
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

#include <vector>

#include "lighter/common/file.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

// Contains a loaded shader 'module' that will be used at 'stage'.
struct ShaderStage {
  VkShaderStageFlagBits stage;
  ShaderModule::RefCountedShaderModule module;
};

std::vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts() {
  FATAL("Not yet implemented");
  return {};
}

std::vector<VkPushConstantRange> CreatePushConstantRanges(
    const PipelineDescriptor::UniformDescriptor& descriptor) {
  std::vector<VkPushConstantRange> ranges;
  ranges.reserve(descriptor.push_constant_ranges.size());
  for (const auto& range : descriptor.push_constant_ranges) {
    ranges.push_back({
        type::ConvertShaderStages(range.shader_stages),
        CAST_TO_UINT(range.offset),
        CAST_TO_UINT(range.size),
    });
  }
  return ranges;
}

VkPipelineLayoutCreateInfo CreatePipelineLayoutInfo(
    const std::vector<VkDescriptorSetLayout>* descriptor_set_layouts,
    const std::vector<VkPushConstantRange>* push_constant_ranges) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(*descriptor_set_layouts),
      descriptor_set_layouts->data(),
      CONTAINER_SIZE(*push_constant_ranges),
      push_constant_ranges->data(),
  };
}

// Loads shaders in 'shader_file_path_map'.
std::vector<ShaderStage> CreateShaderStages(
    const SharedContext& context,
    const GraphicsPipelineDescriptor::ShaderPathMap& shader_path_map) {
  std::vector<ShaderStage> shader_stages;
  shader_stages.reserve(shader_path_map.size());
  for (const auto& pair : shader_path_map) {
    const auto& file_path = pair.second;
    shader_stages.push_back({
        type::ConvertShaderStage(pair.first),
        ShaderModule::RefCountedShaderModule::Get(
            /*identifier=*/file_path, context, file_path),
    });
  }
  return shader_stages;
}

// Extracts shader stage infos, assuming the entry point of each shader is a
// main() function.
std::vector<VkPipelineShaderStageCreateInfo> CreateShaderStageInfos(
    const std::vector<ShaderStage>* shader_stages) {
  static constexpr char kShaderEntryPoint[] = "main";
  std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;
  shader_stage_infos.reserve(shader_stages->size());
  for (const auto& stage : *shader_stages) {
    shader_stage_infos.push_back({
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = nullflag,
        stage.stage,
        **stage.module,
        .pName = kShaderEntryPoint,
        // May use 'pSpecializationInfo' to specify shader constants.
        .pSpecializationInfo = nullptr,
    });
  }
  return shader_stage_infos;
}

std::vector<VkVertexInputBindingDescription>
CreateVertexInputBindingDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  std::vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(descriptor.vertex_buffer_views.size());
  for (const auto& view : descriptor.vertex_buffer_views) {
    descriptions.push_back({
        CAST_TO_UINT(view.binding_point),
        CAST_TO_UINT(view.stride),
        type::ConvertVertexInputRate(view.input_rate),
    });
  }
  return descriptions;
}

std::vector<VkVertexInputAttributeDescription>
CreateVertexInputAttributeDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  const auto num_attributes = common::util::Reduce<int>(
      descriptor.vertex_buffer_views,
      [](const renderer::VertexBufferView& view) {
        return view.attributes.size();
      });

  std::vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attributes);
  for (const auto& view : descriptor.vertex_buffer_views) {
    for (const auto& attrib : view.attributes) {
      descriptions.push_back({
          CAST_TO_UINT(attrib.location),
          CAST_TO_UINT(view.binding_point),
          type::ConvertDataFormat(attrib.format),
          CAST_TO_UINT(attrib.offset),
      });
    }
  }
  return descriptions;
}

// Creates a vertex input state.
VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(
    const std::vector<VkVertexInputBindingDescription>* binding_descriptions,
    const std::vector<VkVertexInputAttributeDescription>*
        attribute_descriptions) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(*binding_descriptions),
      binding_descriptions->data(),
      CONTAINER_SIZE(*attribute_descriptions),
      attribute_descriptions->data(),
  };
}

VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      type::ConvertPrimitiveTopology(descriptor.primitive_topology),
      // 'primitiveRestartEnable' matters for drawing line/triangle strips.
      .primitiveRestartEnable = VK_FALSE,
  };
}

VkViewport CreateViewport(const GraphicsPipelineDescriptor& descriptor) {
  const auto& viewport_info = descriptor.viewport_config.viewport;
  VkViewport viewport{
      viewport_info.origin.x,
      viewport_info.origin.y,
      viewport_info.extent.x,
      viewport_info.extent.y,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  if (descriptor.viewport_config.flip_y) {
    viewport.y += viewport.height;
    viewport.height *= -1;
  }
  return viewport;
}

VkRect2D CreateScissor(const GraphicsPipelineDescriptor& descriptor) {
  const auto& scissor_info = descriptor.viewport_config.scissor;
  return {util::CreateOffset(scissor_info.origin),
          util::CreateExtent(scissor_info.extent)};
}

VkPipelineViewportStateCreateInfo CreateViewportInfo(const VkViewport* viewport,
                                                     const VkRect2D* scissor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      .viewportCount = 1,
      viewport,
      .scissorCount = 1,
      scissor,
  };
}

VkPipelineRasterizationStateCreateInfo CreateRasterizationInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      // If false, fragments beyond clip space will be discarded, not clamped.
      .depthClampEnable = VK_FALSE,
      // If true, disable outputs to the framebuffer.
      .rasterizerDiscardEnable = VK_FALSE,
      // Fill polygons with fragments.
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_BACK_BIT,
      descriptor.viewport_config.flip_y ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                                        : VK_FRONT_FACE_CLOCKWISE,
      // Whether to let the rasterizer alter depth values.
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };
}

VkPipelineMultisampleStateCreateInfo CreateMultisampleInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  // Since all color and depth stencil attachments must have the same sample
  // count, we only need to look at one of them.
  const DeviceImage* attachment =
      descriptor.depth_stencil_attachment == nullptr
          ? descriptor.depth_stencil_attachment
          : descriptor.color_attachment_info_map.begin()->first;
  return {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      type::ConvertSampleCount(attachment->sample_count()),
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 0.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };
}

VkStencilOpState CreateStencilOp(
    const GraphicsPipelineDescriptor::StencilTestOneFace& test) {
  return {
      type::ConvertStencilOp(test.stencil_fail_op),
      type::ConvertStencilOp(test.stencil_and_depth_pass_op),
      type::ConvertStencilOp(test.stencil_pass_depth_fail_op),
      type::ConvertCompareOp(test.compare_op),
      CAST_TO_UINT(test.compare_mask),
      CAST_TO_UINT(test.write_mask),
      CAST_TO_UINT(test.reference),
  };
}

VkPipelineDepthStencilStateCreateInfo CreateDepthStencilInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  using StencilTest = GraphicsPipelineDescriptor::StencilTest;
  const auto& depth_test = descriptor.depth_test;
  const auto& stencil_test = descriptor.stencil_test;
  return {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      util::ToVkBool(depth_test.enable_test),
      util::ToVkBool(depth_test.enable_write),
      type::ConvertCompareOp(depth_test.compare_op),
      // We may only keep fragments in a specific depth range.
      .depthBoundsTestEnable = VK_FALSE,
      util::ToVkBool(stencil_test.enable_test),
      CreateStencilOp(stencil_test.tests[StencilTest::kFrontFaceIndex]),
      CreateStencilOp(stencil_test.tests[StencilTest::kBackFaceIndex]),
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };
}

std::vector<VkPipelineColorBlendAttachmentState> CreateColorBlendStates(
    const GraphicsPipelineDescriptor& descriptor,
    absl::Span<const DeviceImage* const> subpass_attachments) {
  const VkPipelineColorBlendAttachmentState
      disabled_state{.blendEnable = VK_FALSE};
  std::vector<VkPipelineColorBlendAttachmentState> color_blend_states(
      subpass_attachments.size(), disabled_state);
  for (int i = 0; i < subpass_attachments.size(); ++i) {
    const DeviceImage* attachment = subpass_attachments[i];
    const auto iter = descriptor.color_attachment_info_map.find(attachment);
    if (iter == descriptor.color_attachment_info_map.end() ||
        !iter->second.color_blend.has_value()) {
      continue;
    }

    const auto& color_blend = iter->second.color_blend.value();
    color_blend_states[i] = {
        .blendEnable = VK_TRUE,
        type::ConvertBlendFactor(color_blend.src_color_blend_factor),
        type::ConvertBlendFactor(color_blend.dst_color_blend_factor),
        type::ConvertBlendOp(color_blend.color_blend_op),
        type::ConvertBlendFactor(color_blend.src_alpha_blend_factor),
        type::ConvertBlendFactor(color_blend.dst_alpha_blend_factor),
        type::ConvertBlendOp(color_blend.alpha_blend_op),
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                              | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT
                              | VK_COLOR_COMPONENT_A_BIT,
    };
  }
  return color_blend_states;
}

VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(
    const std::vector<VkPipelineColorBlendAttachmentState>* states) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      .logicOpEnable = VK_FALSE,
      VK_LOGIC_OP_CLEAR,
      CONTAINER_SIZE(*states),
      states->data(),
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
  };
}

VkPipelineDynamicStateCreateInfo CreateDynamicStateInfo() {
  return  {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      .dynamicStateCount = 0,
      .pDynamicStates = nullptr,
  };
}

}  // namespace

ShaderModule::ShaderModule(SharedContext context, std::string_view file_path)
    : context_{std::move(FATAL_IF_NULL(context))} {
  const auto raw_data = std::make_unique<common::RawData>(file_path);
  const VkShaderModuleCreateInfo module_info{
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      raw_data->size,
      reinterpret_cast<const uint32_t*>(raw_data->data),
  };
  ASSERT_SUCCESS(vkCreateShaderModule(*context_->device(), &module_info,
                                      *context_->host_allocator(),
                                      &shader_module_),
                 "Failed to create shader module");
}

Pipeline::Pipeline(SharedContext context,
                   const GraphicsPipelineDescriptor& descriptor,
                   const VkRenderPass& render_pass, int subpass_index,
                   absl::Span<const DeviceImage* const> subpass_attachments)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_GRAPHICS, descriptor.uniform_descriptor} {
  const auto shader_stages = CreateShaderStages(context_,
                                                descriptor.shader_path_map);
  const auto shader_stage_infos = CreateShaderStageInfos(&shader_stages);

  const auto vertex_input_bindings =
      CreateVertexInputBindingDescriptions(descriptor);
  const auto vertex_input_attributes =
      CreateVertexInputAttributeDescriptions(descriptor);
  const auto vertex_input_info = CreateVertexInputInfo(
      &vertex_input_bindings, &vertex_input_attributes);

  const auto viewport = CreateViewport(descriptor);
  const auto scissor = CreateScissor(descriptor);
  const auto viewport_info = CreateViewportInfo(&viewport, &scissor);

  const auto color_blend_states =
      CreateColorBlendStates(descriptor, subpass_attachments);
  const auto color_blend_info = CreateColorBlendInfo(&color_blend_states);

  const auto input_assembly_info = CreateInputAssemblyInfo(descriptor);
  const auto rasterization_info = CreateRasterizationInfo(descriptor);
  const auto multisample_info = CreateMultisampleInfo(descriptor);
  const auto depth_stencil_info = CreateDepthStencilInfo(descriptor);
  const auto dynamic_state_info = CreateDynamicStateInfo();

  const VkGraphicsPipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      CONTAINER_SIZE(shader_stage_infos),
      shader_stage_infos.data(),
      &vertex_input_info,
      &input_assembly_info,
      .pTessellationState = nullptr,
      &viewport_info,
      &rasterization_info,
      &multisample_info,
      &depth_stencil_info,
      &color_blend_info,
      &dynamic_state_info,
      layout_,
      render_pass,
      CAST_TO_UINT(subpass_index),
      // 'basePipelineHandle' and 'basePipelineIndex' can be used to copy
      // settings from another pipeline.
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = 0,
  };

  ASSERT_SUCCESS(
      vkCreateGraphicsPipelines(
          *context_->device(), /*pipelineCache=*/VK_NULL_HANDLE,
          /*createInfoCount=*/1, &pipeline_info, *context_->host_allocator(),
          &pipeline_),
      "Failed to create graphics pipeline");
}

Pipeline::Pipeline(SharedContext context,
                   const ComputePipelineDescriptor& descriptor)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_COMPUTE, descriptor.uniform_descriptor} {
  const auto shader_stages = CreateShaderStages(
      context_, {{shader_stage::COMPUTE, descriptor.shader_path}});
  const auto shader_stage_infos = CreateShaderStageInfos(&shader_stages);

  const VkComputePipelineCreateInfo pipeline_info{
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = nullflag,
      shader_stage_infos[0],
      layout_,
      // 'basePipelineHandle' and 'basePipelineIndex' can be used to copy
      // settings from another pipeline.
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = 0,
  };

  ASSERT_SUCCESS(
      vkCreateComputePipelines(
          *context_->device(), /*pipelineCache=*/VK_NULL_HANDLE,
          /*createInfoCount=*/1, &pipeline_info, *context_->host_allocator(),
          &pipeline_),
      "Failed to create compute pipeline");
}

Pipeline::Pipeline(
    SharedContext context, std::string_view name,
    VkPipelineBindPoint binding_point,
    const PipelineDescriptor::UniformDescriptor& uniform_descriptor)
    : context_{std::move(FATAL_IF_NULL(context))}, name_{name},
      binding_point_{binding_point} {
  const auto descriptor_set_layouts = CreateDescriptorSetLayouts();
  const auto push_constant_ranges =
      CreatePushConstantRanges(uniform_descriptor);
  const auto pipeline_layout_info = CreatePipelineLayoutInfo(
      &descriptor_set_layouts, &push_constant_ranges);

  ASSERT_SUCCESS(
      vkCreatePipelineLayout(*context_->device(), &pipeline_layout_info,
                             *context_->host_allocator(), &layout_),
      "Failed to create pipeline layout");
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, binding_point_, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_,
                    *context_->host_allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_,
                          *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Pipeline '%s' destructed", name_);
#endif  // !NDEBUG
}

}  // namespace vk::renderer::lighter