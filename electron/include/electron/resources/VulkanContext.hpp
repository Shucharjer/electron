#pragma once
#include <vulkan/vulkan_core.h>
#include <proton/proton.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace electron {

struct VulkanContext {
    using resource_concept = proton::resource_t;

    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue queue;
    VkAllocationCallbacks* allocator = nullptr;
    uint32_t queueFamilyIndex        = 0;
    vk::DescriptorPool descriptorPool;
    bool rebuildSwapchain = false;
};

} // namespace electron
