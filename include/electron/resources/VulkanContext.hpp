#pragma once
#include <vulkan/vulkan_core.h>
#include <neutron/ecs.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace electron {

struct VulkanContext {
    using resource_concept = neutron::resource_t;

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
