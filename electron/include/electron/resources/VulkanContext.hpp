#pragma once
#include <vma/vk_mem_alloc.h>
#include <proton/proton.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>


namespace electron {

struct VulkanContext {
    using resource_concept = proton::resource_t;

    vk::Instance instance;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue presentQueue;
    uint32_t graphicsFamilyIndex = 0;
    uint32_t presentFamilyIndex  = 0;
};

} // namespace electron
