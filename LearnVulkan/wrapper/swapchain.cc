//
//  swapchain.cc
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "swapchain.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::max;
using std::min;
using std::vector;

VkSurfaceFormatKHR ChooseSurfaceFormat(
    const vector<VkSurfaceFormatKHR>& available) {
  // if surface has no preferred format, we can choose any format
  if (available.size() == 1 && available[0].format == VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for (auto candidate : available) {
    if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
        candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return candidate;
  }

  // if our preferred format is not supported, simply choose the first one
  return available[0];
}

VkPresentModeKHR ChoosePresentMode(const vector<VkPresentModeKHR>& available) {
  // FIFO mode is guaranteed to be available, but not properly supported by
  // some drivers. we will prefer MAILBOX and IMMEDIATE mode over it
  VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

  for (auto candidate : available) {
    switch (candidate) {
      case VK_PRESENT_MODE_MAILBOX_KHR:
        return candidate;
      case VK_PRESENT_MODE_IMMEDIATE_KHR:
        best_mode = candidate;
      default:
        break;
    }
  }

  return best_mode;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                        VkExtent2D current_extent) {
  // .currentExtent is the suggested resolution
  // if it is UINT32_MAX, window manager suggests us to be flexible
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    current_extent.width = max(capabilities.minImageExtent.width,
                               current_extent.width);
    current_extent.width = min(capabilities.maxImageExtent.width,
                               current_extent.width);
    current_extent.height = max(capabilities.minImageExtent.height,
                                current_extent.height);
    current_extent.height = min(capabilities.maxImageExtent.height,
                                current_extent.height);
    return current_extent;
  }
}

vector<VkImage> CreateImages(const VkSwapchainKHR& swapchain,
                             const VkDevice& device) {
  // image count might be different from the minimum we set
  return util::QueryAttribute<VkImage>(
      [&device, &swapchain](uint32_t *count, VkImage *images) {
        vkGetSwapchainImagesKHR(device, swapchain, count, images);
      }
  );
}

vector<VkImageView> CreateImageViews(const vector<VkImage>& images,
                                     const VkDevice& device,
                                     VkFormat image_format,
                                     const VkAllocationCallbacks* allocator) {
  // use image view to specify how will we use these images
  // (color, depth, stencil, etc)
  vector<VkImageView> image_views(images.size());
  for (size_t i = 0; i < images.size(); ++i) {
    VkImageViewCreateInfo image_view_info{
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/NULL_FLAG,
        images[i],
        /*viewType=*/VK_IMAGE_VIEW_TYPE_2D,  // 2D, 3D, cube maps
        image_format,
        // enable swizzling color channels around
        VkComponentMapping{
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
        },
        // specify image's purpose and which part to access
        VkImageSubresourceRange{
            /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
            /*baseMipLevel=*/0,
            /*levelCount=*/1,
            /*baseArrayLayer=*/0,
            /*layerCount=*/1,
        },
    };

    ASSERT_SUCCESS(
        vkCreateImageView(device, &image_view_info, allocator, &image_views[i]),
        "Failed to create image view");
  }
  return image_views;
}

} /* namespace */

bool Swapchain::HasSwapchainSupport(const VkSurfaceKHR& surface,
                                    const VkPhysicalDevice& physical_device) {
  try {
    std::cout << "Checking extension support required for swapchain..."
              << std::endl << std::endl;

    vector<std::string> required{
        kSwapChainExtensions.begin(),
        kSwapChainExtensions.end(),
    };
    auto extensions{util::QueryAttribute<VkExtensionProperties>(
        [&physical_device](uint32_t* count, VkExtensionProperties* properties) {
          return vkEnumerateDeviceExtensionProperties(
              physical_device, nullptr, count, properties);
        }
    )};
    auto get_name = [](const VkExtensionProperties& property) -> const char* {
      return property.extensionName;
    };
    util::CheckSupport<VkExtensionProperties>(required, extensions, get_name);
  } catch (const std::exception& e) {
    return false;
  }

  // physical device may support swapchain but maybe not compatible with
  // window system, so we need to query details
  uint32_t format_count, mode_count;
  vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, surface, &format_count, nullptr);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, surface, &mode_count, nullptr);
  return format_count && mode_count;
}

void Swapchain::Init(std::shared_ptr<Context> context) {
  context_ = context;
  const VkSurfaceKHR& surface = *context_->surface();
  const VkPhysicalDevice& physical_device = *context_->physical_device();
  const VkDevice& device = *context_->device();

  // surface capabilities
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      physical_device, surface, &surface_capabilities);
  VkExtent2D image_extent = ChooseExtent(
      surface_capabilities, context_->screen_size());

  // surface formats
  auto surface_formats{util::QueryAttribute<VkSurfaceFormatKHR>(
      [&surface, &physical_device]
      (uint32_t* count, VkSurfaceFormatKHR* formats) {
        return vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, count, formats);
      }
  )};
  VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(surface_formats);

  // present modes
  auto present_modes{util::QueryAttribute<VkPresentModeKHR>(
      [&surface, &physical_device](uint32_t* count, VkPresentModeKHR* modes) {
        return vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, count, modes);
      }
  )};
  VkPresentModeKHR present_mode = ChoosePresentMode(present_modes);

  // minimum amount of images we want to have in swapchain
  uint32_t min_image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0) // can be 0 if no maximum
    min_image_count = min(surface_capabilities.maxImageCount, min_image_count);

  VkSwapchainCreateInfoKHR swapchain_info{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      surface,
      min_image_count,
      surface_format.format,
      surface_format.colorSpace,
      image_extent,
      /*imageArrayLayers=*/1,
      // can be different for post-processing
      /*imageUsage=*/VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      /*imageSharingMode=*/VK_SHARING_MODE_EXCLUSIVE,
      /*queueFamilyIndexCount=*/0,
      /*pQueueFamilyIndices=*/nullptr,
      // may apply transformations
      /*preTransform=*/surface_capabilities.currentTransform,
      // may change alpha channel
      /*compositeAlpha=*/VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      present_mode,
      /*clipped=*/VK_TRUE,  // don't care about color of pixels obscured
      /*oldSwapchain=*/VK_NULL_HANDLE,
  };

  // graphics queue and present queue might be the same
  std::unordered_set<uint32_t> queue_families{
      context_->queues().graphics.family_index,
      context_->queues().present.family_index,
  };
  if (queue_families.size() > 1) {
    // specify which queue families will share access to images. we will draw on
    // images in swapchain from graphics queue and submit on presentation queue.
    // otherwise set to VK_SHARING_MODE_EXCLUSIVE
    vector<uint32_t> indices{queue_families.begin(), queue_families.end()};
    swapchain_info.queueFamilyIndexCount = CONTAINER_SIZE(indices);
    swapchain_info.pQueueFamilyIndices = indices.data();
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
  }

  ASSERT_SUCCESS(vkCreateSwapchainKHR(device, &swapchain_info,
                                      context_->allocator(), &swapchain_),
                 "Failed to create swapchain");

  image_format_ = surface_format.format;
  image_extent_ = image_extent;
  images_ = CreateImages(swapchain_, device);
  image_views_ = CreateImageViews(images_, device, image_format_,
                                  context_->allocator());
}

void Swapchain::Cleanup() {
  // images are implicitly cleaned up with swapchain
  for (auto& image_view : image_views_)
    vkDestroyImageView(*context_->device(), image_view, context_->allocator());
  vkDestroySwapchainKHR(*context_->device(), swapchain_, context_->allocator());
}

const vector<const char*> kSwapChainExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

} /* namespace vulkan */
} /* namespace wrapper */
