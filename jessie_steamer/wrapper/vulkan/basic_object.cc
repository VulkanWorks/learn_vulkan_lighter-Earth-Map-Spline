//
//  basic_object.cc
//
//  Created by Pujun Lun on 2/10/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/basic_object.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"
#include "jessie_steamer/wrapper/vulkan/swapchain.h"
#include "jessie_steamer/wrapper/vulkan/validation.h"
#include "third_party/glfw/glfw3.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

namespace util = common::util;

struct QueueIndices {
  bool IsValid() const {
    return graphics != util::kInvalidIndex && present != util::kInvalidIndex;
  }

  int graphics, present;
};

absl::optional<QueueIndices> FindDeviceQueues(
    SharedContext context, const VkPhysicalDevice& physical_device) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  std::cout << "Found device: " << properties.deviceName
            << std::endl << std::endl;

  // require swapchain support
  if (!Swapchain::HasSwapchainSupport(context, physical_device)) {
    return absl::nullopt;
  }

  // require anisotropy filtering support
  VkPhysicalDeviceFeatures feature_support;
  vkGetPhysicalDeviceFeatures(physical_device, &feature_support);
  if (!feature_support.samplerAnisotropy) {
    return absl::nullopt;
  }

  // find queue family that holds graphics queue
  auto graphics_support = [](const VkQueueFamilyProperties& family) {
    return family.queueCount && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT);
  };

  // find queue family that holds present queue
  uint32_t index = 0;
  auto present_support = [&physical_device, &context, index]
      (const VkQueueFamilyProperties& family) mutable {
      VkBool32 support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(
          physical_device, index++, *context->surface(), &support);
      return support;
  };

  auto families{util::QueryAttribute<VkQueueFamilyProperties>(
      [&physical_device](uint32_t* count, VkQueueFamilyProperties* properties) {
        return vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, count, properties);
      }
  )};

  QueueIndices indices{
      util::FindFirst<VkQueueFamilyProperties>(families, graphics_support),
      util::FindFirst<VkQueueFamilyProperties>(families, present_support),
  };
  return indices.IsValid() ? absl::make_optional<>(indices) : absl::nullopt;
}

} /* namespace */

void Instance::Init(SharedContext context) {
  context_ = std::move(context);

  if (glfwVulkanSupported() == GL_FALSE) {
    throw std::runtime_error{"Vulkan not supported"};
  }

  uint32_t glfw_extension_count;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);

#ifndef NDEBUG
  std::vector<const char*> required_extensions{
      glfw_extensions,
      glfw_extensions + glfw_extension_count,
  };
  // one extra extension to enable debug report
  required_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  validation::EnsureInstanceExtensionSupport({
      required_extensions.begin(),
      required_extensions.end()
  });
  validation::EnsureValidationLayerSupport({
      validation::layers().begin(),
      validation::layers().end()
  });
#endif /* !NDEBUG */

  // [optional]
  // might be useful for the driver to optimize for some graphics engine
  VkApplicationInfo app_info{
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      /*pNext=*/nullptr,
      /*pApplicationName=*/"Vulkan Application",
      /*applicationVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*pEngineName=*/"No Engine",
      /*engineVersion=*/VK_MAKE_VERSION(1, 0, 0),
      /*apiVersion=*/VK_API_VERSION_1_0,
  };

  // [required]
  // tell the driver which global extensions and validation layers to use
  VkInstanceCreateInfo instance_info{
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      &app_info,
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
      // enabled extensions
      glfw_extension_count,
      glfw_extensions,
#else  /* !NDEBUG */
      // enabled layers
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
      // enabled extensions
      CONTAINER_SIZE(required_extensions),
      required_extensions.data(),
#endif /* NDEBUG */
  };

  ASSERT_SUCCESS(
      vkCreateInstance(&instance_info, context_->allocator(), &instance_),
      "Failed to create instance");
}

Instance::~Instance() {
  vkDestroyInstance(instance_, context_->allocator());
}

void Surface::Init(SharedContext context) {
  context_ = std::move(context);
  surface_ = context_->window().CreateSurface(*context_->instance(),
                                              context_->allocator());
}

Surface::~Surface() {
  vkDestroySurfaceKHR(*context_->instance(), surface_, context_->allocator());
}

void PhysicalDevice::Init(SharedContext context) {
  context_ = std::move(context);

  auto devices{util::QueryAttribute<VkPhysicalDevice>(
      [this](uint32_t* count, VkPhysicalDevice* physical_device) {
        return vkEnumeratePhysicalDevices(
            *context_->instance(), count, physical_device);
      }
  )};

  for (const auto& candidate : devices) {
    auto indices = FindDeviceQueues(context_, candidate);
    if (indices.has_value()) {
      physical_device_ = candidate;
      context_->set_queue_family_indices(
          static_cast<uint32_t>(indices.value().graphics),
          static_cast<uint32_t>(indices.value().present)
      );

      // query device limits
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(physical_device_, &properties);
      limits_ = properties.limits;

      return;
    }
  }
  throw std::runtime_error{"Failed to find suitable GPU"};
}

void Device::Init(SharedContext context) {
  context_ = std::move(context);

  // request anisotropy filtering support
  VkPhysicalDeviceFeatures enabled_features{};
  enabled_features.samplerAnisotropy = VK_TRUE;

  // request negative-height viewport support
  auto enabled_extensions = Swapchain::extensions();
  enabled_extensions.emplace_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

  // graphics queue and present queue might be the same
  const Queues& queues = context_->queues();
  absl::flat_hash_set<uint32_t> queue_families{
      queues.graphics.family_index,
      queues.present.family_index,
  };
  float priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_infos;
  for (uint32_t queue_family : queue_families) {
    VkDeviceQueueCreateInfo queue_info{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        /*pNext=*/nullptr,
        /*flags=*/nullflag,
        queue_family,
        /*queueCount=*/1,
        &priority,  // always required even if only one queue
    };
    queue_infos.emplace_back(queue_info);
  }

  VkDeviceCreateInfo device_info{
      VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // queue create infos
      CONTAINER_SIZE(queue_infos),
      queue_infos.data(),
#ifdef NDEBUG
      /*enabledLayerCount=*/0,
      /*ppEnabledLayerNames=*/nullptr,
#else  /* !NDEBUG */
      // enabled layers
      CONTAINER_SIZE(validation::layers()),
      validation::layers().data(),
#endif /* NDEBUG */
      CONTAINER_SIZE(enabled_extensions),
      enabled_extensions.data(),
      &enabled_features,
  };

  ASSERT_SUCCESS(vkCreateDevice(*context_->physical_device(), &device_info,
                                context_->allocator(), &device_),
                 "Failed to create logical device");

  // retrieve queue handles for each queue family
  VkQueue graphics_queue, present_queue;
  vkGetDeviceQueue(device_, queues.graphics.family_index, 0, &graphics_queue);
  vkGetDeviceQueue(device_, queues.present.family_index, 0, &present_queue);
  context_->set_queues(graphics_queue, present_queue);
}

Device::~Device() {
  vkDestroyDevice(device_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
