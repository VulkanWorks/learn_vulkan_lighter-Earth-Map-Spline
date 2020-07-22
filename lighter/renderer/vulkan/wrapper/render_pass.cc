//
//  render_pass.cc
//
//  Created by Pujun Lun on 2/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/render_pass.h"

#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using common::util::SetElementWithResizing;

using ColorOps = RenderPassBuilder::Attachment::ColorOps;
using DepthStencilOps = RenderPassBuilder::Attachment::DepthStencilOps;

// Creates clear value for 'attachment'.
VkClearValue CreateClearColor(const RenderPassBuilder::Attachment& attachment) {
  VkClearValue clear_value{};
  if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
    clear_value.color = {{0, 0, 0, 0}};
  } else if (absl::holds_alternative<DepthStencilOps>(
      attachment.attachment_ops)) {
    clear_value.depthStencil = {/*depth=*/1.0f, /*stencil=*/0};
  } else {
    FATAL("Unrecognized variant type");
  }
  return clear_value;
}

// Creates description for 'attachment'. The image format will be
// VK_FORMAT_UNDEFINED, and the sample count will be VK_SAMPLE_COUNT_1_BIT.
// The caller may need to update these information.
VkAttachmentDescription CreateAttachmentDescription(
    const RenderPassBuilder::Attachment& attachment) {
  VkAttachmentDescription description{
      /*flags=*/nullflag,
      VK_FORMAT_UNDEFINED,  // To be updated.
      VK_SAMPLE_COUNT_1_BIT,  // To be updated.
      /*loadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // To be updated.
      /*storeOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // To be updated.
      /*stencilLoadOp=*/VK_ATTACHMENT_LOAD_OP_DONT_CARE,  // To be updated.
      /*stencilStoreOp=*/VK_ATTACHMENT_STORE_OP_DONT_CARE,  // To be updated.
      attachment.initial_layout,
      attachment.final_layout,
  };
  if (absl::holds_alternative<ColorOps>(attachment.attachment_ops)) {
    const auto& color_ops = absl::get<ColorOps>(attachment.attachment_ops);
    description.loadOp = color_ops.load_color_op;
    description.storeOp = color_ops.store_color_op;
  } else if (absl::holds_alternative<DepthStencilOps>(
      attachment.attachment_ops)) {
    const auto& depth_stencil_ops =
        absl::get<DepthStencilOps>(attachment.attachment_ops);
    description.loadOp = depth_stencil_ops.load_depth_op;
    description.storeOp = depth_stencil_ops.store_depth_op;
    description.stencilLoadOp = depth_stencil_ops.load_stencil_op;
    description.stencilStoreOp = depth_stencil_ops.store_stencil_op;
  } else {
    FATAL("Unrecognized variant type");
  }
  return description;
}

// Creates subpass description given 'subpass_attachments'.
std::vector<VkSubpassDescription> CreateSubpassDescriptions(
    const std::vector<RenderPassBuilder::SubpassAttachments>&
        subpass_attachments) {
  std::vector<VkSubpassDescription> descriptions;
  descriptions.reserve(subpass_attachments.size());
  for (const auto& attachments : subpass_attachments) {
    descriptions.push_back(VkSubpassDescription{
        /*flags=*/nullflag,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        /*inputAttachmentCount=*/0,
        /*pInputAttachments=*/nullptr,
        CONTAINER_SIZE(attachments.color_refs),
        attachments.color_refs.data(),
        attachments.multisampling_refs.has_value()
            ? attachments.multisampling_refs->data()
            : nullptr,
        // A render pass can only use one depth stencil attachment, so we do not
        // need to pass a count.
        attachments.depth_stencil_ref.has_value()
            ? &attachments.depth_stencil_ref.value()
            : nullptr,
        /*preserveAttachmentCount=*/0,
        /*pPreserveAttachments=*/nullptr,
    });
  }
  return descriptions;
}

// Returns the number of color attachments in each subpass.
std::vector<int> GetNumberColorAttachmentsInSubpasses(
    const std::vector<RenderPassBuilder::SubpassAttachments>&
        subpass_attachments) {
  std::vector<int> num_color_attachments;
  num_color_attachments.reserve(subpass_attachments.size());
  for (const auto& attachments : subpass_attachments) {
    num_color_attachments.push_back(attachments.color_refs.size());
  }
  return num_color_attachments;
}

// Converts RenderPassBuilder::SubpassDependency to VkSubpassDependency.
VkSubpassDependency CreateSubpassDependency(
    const RenderPassBuilder::SubpassDependency& dependency) {
  return VkSubpassDependency{
      dependency.prev_subpass.index,
      dependency.next_subpass.index,
      dependency.prev_subpass.stage_flags,
      dependency.next_subpass.stage_flags,
      dependency.prev_subpass.access_flags,
      dependency.next_subpass.access_flags,
      dependency.dependency_flags,
  };
}

// Creates framebuffers.
std::vector<VkFramebuffer> CreateFramebuffers(
    const BasicContext& context,
    const VkRenderPass& render_pass,
    const std::vector<RenderPassBuilder::GetImage>& get_images,
    int num_framebuffers, const VkExtent2D& framebuffer_size) {
  std::vector<VkImageView> image_views(get_images.size());
  VkFramebufferCreateInfo framebuffer_info{
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      render_pass,
      CONTAINER_SIZE(image_views),
      image_views.data(),
      framebuffer_size.width,
      framebuffer_size.height,
      kSingleImageLayer,
  };

  std::vector<VkFramebuffer> framebuffers(num_framebuffers);
  for (int i = 0; i < framebuffers.size(); ++i) {
    for (int image_index = 0; image_index < get_images.size(); ++image_index) {
      image_views[image_index] =
          get_images[image_index](/*framebuffer_index=*/i).image_view();
    }
    ASSERT_SUCCESS(vkCreateFramebuffer(*context.device(), &framebuffer_info,
                                       *context.allocator(), &framebuffers[i]),
                   "Failed to create framebuffer");
  }
  return framebuffers;
}

} /* namespace */

std::vector<VkAttachmentReference>
RenderPassBuilder::CreateMultisamplingReferences(
    int num_color_refs, absl::Span<const MultisamplingPair> pairs) {
  ASSERT_NON_EMPTY(pairs, "No multisampling pairs provided");
  std::vector<VkAttachmentReference> references(
      num_color_refs,
      VkAttachmentReference{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED}
  );
  for (const auto& pair : pairs) {
    references[pair.multisample_reference] =
        VkAttachmentReference{
            pair.target_attachment,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
  }
  return references;
}

RenderPassBuilder& RenderPassBuilder::SetNumFramebuffers(int count) {
  num_framebuffers_ = count;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetAttachment(
    int index, const Attachment& attachment) {
  SetElementWithResizing(CreateClearColor(attachment), index, &clear_values_);
  SetElementWithResizing(CreateAttachmentDescription(attachment), index,
                         &attachment_descriptions_);
  if (attachment_descriptions_.size() > get_attachment_images_.size()) {
    get_attachment_images_.resize(attachment_descriptions_.size());
  }
  return *this;
}

RenderPassBuilder& RenderPassBuilder::UpdateAttachmentImage(
    int index, GetImage&& get_image) {
  ASSERT_NON_NULL(get_image, "'get_image' cannot be nullptr");
  const Image& sample_image = get_image(/*framebuffer_index=*/0);
  attachment_descriptions_[index].format = sample_image.format();
  attachment_descriptions_[index].samples = sample_image.sample_count();
  get_attachment_images_.at(index) = std::move(get_image);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetSubpass(
    int index, std::vector<VkAttachmentReference>&& color_refs,
    const absl::optional<VkAttachmentReference>& depth_stencil_ref) {
  SubpassAttachments attachments{
      std::move(color_refs),
      /*multisampling_refs=*/absl::nullopt,  // To be updated.
      depth_stencil_ref,
  };
  SetElementWithResizing(std::move(attachments), index, &subpass_attachments_);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetMultisampling(
    int subpass_index,
    std::vector<VkAttachmentReference>&& multisampling_refs) {
  ASSERT_TRUE(subpass_index < subpass_attachments_.size(),
              absl::StrFormat("Attachments not set for subpass %d",
                              subpass_index));
  const int num_color_attachments =
      subpass_attachments_[subpass_index].color_refs.size();
  ASSERT_TRUE(multisampling_refs.size() == num_color_attachments,
              absl::StrFormat("Number of multisampling attachment (%d) must be "
                              "equal to the number of color attachments (%d) "
                              "for subpass %d",
                              multisampling_refs.size(), num_color_attachments,
                              subpass_index));
  subpass_attachments_[subpass_index].multisampling_refs =
      std::move(multisampling_refs);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSubpassDependency(
    const SubpassDependency& dependency) {
  subpass_dependencies_.push_back(CreateSubpassDependency(dependency));
  return *this;
}

std::unique_ptr<RenderPass> RenderPassBuilder::Build() const {
  ASSERT_HAS_VALUE(num_framebuffers_, "Number of framebuffers is not set");
  for (int i = 0; i < get_attachment_images_.size(); ++i) {
    ASSERT_NON_NULL(
        get_attachment_images_[i],
        absl::StrFormat("Attachment image at index %d is not set", i));
  }

  const auto subpass_descriptions =
      CreateSubpassDescriptions(subpass_attachments_);
  const VkRenderPassCreateInfo render_pass_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(attachment_descriptions_),
      attachment_descriptions_.data(),
      CONTAINER_SIZE(subpass_descriptions),
      subpass_descriptions.data(),
      CONTAINER_SIZE(subpass_dependencies_),
      subpass_dependencies_.data(),
  };

  VkRenderPass render_pass;
  ASSERT_SUCCESS(vkCreateRenderPass(*context_->device(), &render_pass_info,
                                    *context_->allocator(), &render_pass),
                 "Failed to create render pass");

  const auto framebuffer_size =
      get_attachment_images_[0](/*framebuffer_index=*/0).extent();
  return std::unique_ptr<RenderPass>{new RenderPass{
      context_, /*num_subpasses=*/static_cast<int>(subpass_descriptions.size()),
      render_pass, clear_values_, framebuffer_size,
      CreateFramebuffers(*context_, render_pass, get_attachment_images_,
                         num_framebuffers_.value(), framebuffer_size),
      GetNumberColorAttachmentsInSubpasses(subpass_attachments_)}};
}

void RenderPass::Run(const VkCommandBuffer& command_buffer,
                     int framebuffer_index,
                     absl::Span<const RenderOp> render_ops) const {
  ASSERT_TRUE(render_ops.size() == num_subpasses_,
              absl::StrFormat("Render pass contains %d subpasses, but %d "
                              "rendering operations are provided",
                              num_subpasses_, render_ops.size()));

  const VkRenderPassBeginInfo begin_info{
      VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      /*pNext=*/nullptr,
      render_pass_,
      framebuffers_[framebuffer_index],
      /*renderArea=*/{
          /*offset=*/{0, 0},
          framebuffer_size_,
      },
      CONTAINER_SIZE(clear_values_),
      clear_values_.data(),
  };

  vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
  for (int i = 0; i < render_ops.size(); ++i) {
    if (i != 0) {
      vkCmdNextSubpass(command_buffer, VK_SUBPASS_CONTENTS_INLINE);
    }
    render_ops[i](command_buffer);
  }
  vkCmdEndRenderPass(command_buffer);
}

RenderPass::~RenderPass() {
  for (const auto& framebuffer : framebuffers_) {
    vkDestroyFramebuffer(*context_->device(), framebuffer,
                         *context_->allocator());
  }
  vkDestroyRenderPass(*context_->device(), render_pass_,
                      *context_->allocator());
#ifndef NDEBUG
  LOG_INFO << "Render pass destructed";
#endif  /* !NDEBUG */
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */