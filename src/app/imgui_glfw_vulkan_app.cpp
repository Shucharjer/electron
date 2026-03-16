#if defined(ELECTRON_GLFW_VULKAN_APP)

    #include <array>
    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <iostream>
    #include <vector>
    #include <imgui.h>
    #include <imgui_impl_glfw.h>
    #include <imgui_impl_vulkan.h>
    #include <vulkan/vulkan_core.h>
    #include <neutron/print.hpp>
    #include <neutron/utility.hpp>
    #include <vulkan/vulkan.hpp>
    #include <GLFW/glfw3.h>
    #include "electron/app/app.hpp"
    #include "electron/app/config.hpp"
    #include "electron/resources/VulkanContext.hpp"

using namespace electron;
using neutron::println;

static bool SetupVulkan(
    VulkanContext&, const std::string& appName,
    std::vector<const char*>& instanceExtensions);
static bool SetupVulkanWindow(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext&, VkSurfaceKHR surface,
    int width, int height, uint8_t framebufferCount);
static void
    CleanupVulkanWindows(ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext&);
static void CleanupVulkan(VulkanContext&);
static void ImGuiCheckVkResult(VkResult);
static void Render(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext&, ImDrawData* drawData);
static void Present(ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext&);
static void GlfwErrorCallback(int error, const char* description);
static bool IsExtensionAvailable(
    const std::vector<vk::ExtensionProperties>& properties,
    const char* extension);
static bool IsExtensionAvailable(
    const ImVector<VkExtensionProperties>& properties, const char* extension);
static bool IsExtensionEnabled(
    const std::vector<const char*>& enabledExtensions, const char* extension);

class App::Impl {
public:
    VulkanContext* init(const wnd_config& config) {
        framebufferCount_ = config.framebufferCount < 2 ? 2 : config.framebufferCount;

        glfwSetErrorCallback(&GlfwErrorCallback);
        if (glfwInit() == GLFW_FALSE) {
            println("Error: glfwInit() failed");
            return nullptr;
        }
        glfwInitialized_ = true;

        if (glfwVulkanSupported() == GLFW_FALSE) {
            println("Error: glfwVulkanSupported() == GLFW_FALSE");
            return nullptr;
        }

        using enum window_flags;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(
            GLFW_RESIZABLE,
            (config.flags & resizable) ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(
            GLFW_DECORATED,
            (config.flags & borderless) ? GLFW_FALSE : GLFW_TRUE);
        glfwWindowHint(
            GLFW_MAXIMIZED,
            (config.flags & maximized) ? GLFW_TRUE : GLFW_FALSE);

        GLFWmonitor* monitor =
            (config.flags & fullscreen) ? glfwGetPrimaryMonitor() : nullptr;
        window_ = glfwCreateWindow(
            config.width, config.height, config.name.c_str(), monitor, nullptr);
        if (window_ == nullptr) {
            println("Error: glfwCreateWindow() failed");
            return nullptr;
        }

        uint32_t extensionCount = 0;
        const char** glfwExtensions =
            glfwGetRequiredInstanceExtensions(&extensionCount);
        if (glfwExtensions == nullptr || extensionCount == 0) {
            println("Error: glfwGetRequiredInstanceExtensions() failed");
            return nullptr;
        }

        std::vector<const char*> extensions(
            glfwExtensions, glfwExtensions + extensionCount);

        if (!SetupVulkan(vkContext_, config.name, extensions)) {
            return nullptr;
        }

        VkResult result = glfwCreateWindowSurface(
            vkContext_.instance, window_, vkContext_.allocator, &surface_);
        if (result != VK_SUCCESS) {
            ImGuiCheckVkResult(result);
            return nullptr;
        }

        int framebufferWidth{};
        int framebufferHeight{};
        glfwGetFramebufferSize(window_, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            framebufferWidth  = config.width;
            framebufferHeight = config.height;
        }

        ImGui_ImplVulkanH_Window* imgui_vulkan_window = &imguiWindowData_;
        if (!SetupVulkanWindow(
                imgui_vulkan_window, vkContext_, surface_, framebufferWidth,
                framebufferHeight, framebufferCount_)) {
            return nullptr;
        }
        vulkanWindowInitialized_ = true;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imguiContextCreated_ = true;
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // dpi

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(window_, true)) {
            println("Error: ImGui_ImplGlfw_InitForVulkan() failed");
            return nullptr;
        }
        imguiGlfwInitialized_ = true;

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion     = VK_API_VERSION_1_0;
        init_info.Instance       = vkContext_.instance;
        init_info.PhysicalDevice = vkContext_.physicalDevice;
        init_info.Device         = vkContext_.device;
        init_info.QueueFamily    = vkContext_.queueFamilyIndex;
        init_info.Queue          = vkContext_.queue;
        init_info.PipelineCache   = nullptr;
        init_info.DescriptorPool  = vkContext_.descriptorPool;
        init_info.MinImageCount   = framebufferCount_;
        init_info.ImageCount      = imgui_vulkan_window->ImageCount;
        init_info.PipelineInfoMain.RenderPass   = imgui_vulkan_window->RenderPass;
        init_info.PipelineInfoMain.Subpass      = 0;
        init_info.PipelineInfoMain.MSAASamples  = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator       = vkContext_.allocator;
        init_info.CheckVkResultFn = &ImGuiCheckVkResult;
        if (!ImGui_ImplVulkan_Init(&init_info)) {
            println("Error: ImGui_ImplVulkan_Init() failed");
            return nullptr;
        }
        imguiVulkanInitialized_ = true;

        // load fonts
        //

        return &vkContext_;
    }

    bool pollEvents() {
        glfwPollEvents();

        if (glfwWindowShouldClose(window_) != 0) [[unlikely]] {
            stopped_ = true;
            return false;
        }

        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            return false;
        }

        int width{};
        int height{};
        glfwGetFramebufferSize(window_, &width, &height);
        const bool framebufferResized =
            imguiWindowData_.Width != width || imguiWindowData_.Height != height;
        if (width > 0 && height > 0 &&
            (vkContext_.rebuildSwapchain || framebufferResized)) {
            ImGui_ImplVulkan_SetMinImageCount(framebufferCount_);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                vkContext_.instance, vkContext_.physicalDevice,
                vkContext_.device, &imguiWindowData_,
                vkContext_.queueFamilyIndex, vkContext_.allocator, width,
                height, framebufferCount_, 0);
            imguiWindowData_.FrameIndex = 0;
            vkContext_.rebuildSwapchain = false;
        }

        return true;
    }

    ATOM_NODISCARD bool is_stopped() const noexcept { return stopped_; }

    // NOLINTNEXTLINE
    void renderBegin() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void renderEnd() {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const auto minimized =
            (draw_data->DisplaySize.x <= 0.0F ||
             draw_data->DisplaySize.y <= 0.0F);
        if (!minimized) {
            Render(&imguiWindowData_, vkContext_, draw_data);
        }
        if (!minimized) {
            ::Present(&imguiWindowData_, vkContext_);
        }
    }

    void destroy() {
        if (vkContext_.device) {
            vkContext_.device.waitIdle();
        }
        if (imguiVulkanInitialized_) {
            ImGui_ImplVulkan_Shutdown();
        }
        if (imguiGlfwInitialized_) {
            ImGui_ImplGlfw_Shutdown();
        }
        if (imguiContextCreated_) {
            ImGui::DestroyContext();
        }
        if (vulkanWindowInitialized_) {
            CleanupVulkanWindows(&imguiWindowData_, vkContext_);
        }
        if (surface_ != VK_NULL_HANDLE && vkContext_.instance) {
            vkDestroySurfaceKHR(vkContext_.instance, surface_, vkContext_.allocator);
            surface_ = VK_NULL_HANDLE;
        }
        CleanupVulkan(vkContext_);
        if (window_ != nullptr) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        if (glfwInitialized_) {
            glfwTerminate();
            glfwInitialized_ = false;
        }
    }

private:
    uint8_t framebufferCount_ = 2;
    bool stopped_             = false;
    bool glfwInitialized_     = false;
    bool imguiContextCreated_ = false;
    bool imguiGlfwInitialized_ = false;
    bool imguiVulkanInitialized_ = false;
    bool vulkanWindowInitialized_ = false;
    GLFWwindow* window_       = nullptr;
    VkSurfaceKHR surface_     = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Window imguiWindowData_;
    VulkanContext vkContext_;
};

App::Impl* App::_create_impl() { return new App::Impl; }

VulkanContext* App::_init_impl(App::Impl* impl, const wnd_config& config) {
    return impl->init(config);
}

bool App::_is_stopped(App::Impl* impl) { return impl->is_stopped(); }

bool App::_poll_events(App::Impl* impl) { return impl->pollEvents(); }

void App::_render_begin(App::Impl* impl) { impl->renderBegin(); }

void App::_render_end(App::Impl* impl) { impl->renderEnd(); }

void App::_destroy_impl(App::Impl* impl) {
    impl->destroy();
    delete impl;
}

bool SetupVulkan(
    VulkanContext& vkContext, const std::string& appName,
    std::vector<const char*>& instanceExtensions) {

    // create instance
    {
        auto extensionProperties  = vk::enumerateInstanceExtensionProperties();
        if (IsExtensionAvailable(
                extensionProperties,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) &&
            !IsExtensionEnabled(
                instanceExtensions,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            instanceExtensions.push_back(
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

    #ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        bool enablePortabilityEnumeration = false;
        if (IsExtensionAvailable(
                extensionProperties,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) &&
            !IsExtensionEnabled(
                instanceExtensions,
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            instanceExtensions.push_back(
                VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            enablePortabilityEnumeration = true;
        }
    #endif

        vk::ApplicationInfo appInfo{ appName.c_str(), 1, "atom", 1,
                                     VK_API_VERSION_1_0 };

        vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);
        instanceCreateInfo
            .setEnabledExtensionCount(instanceExtensions.size())
            .setPpEnabledExtensionNames(instanceExtensions.data());

    #ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (enablePortabilityEnumeration) {
            instanceCreateInfo.flags |=
                vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
    #endif

        vkContext.instance = vk::createInstance(instanceCreateInfo);
    }

    // select physical device
    vkContext.physicalDevice =
        ImGui_ImplVulkanH_SelectPhysicalDevice(vkContext.instance);
    // g_physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(g_instance);
    IM_ASSERT(vkContext.physicalDevice != VK_NULL_HANDLE);

    // select graphics queue family
    vkContext.queueFamilyIndex =
        ImGui_ImplVulkanH_SelectQueueFamilyIndex(vkContext.physicalDevice);
    IM_ASSERT(vkContext.queueFamilyIndex != static_cast<uint32_t>(-1));

    // create logical device
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");
        uint32_t properties_count{};
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(
            vkContext.physicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(static_cast<int>(properties_count));
        vkEnumerateDeviceExtensionProperties(
            vkContext.physicalDevice, nullptr, &properties_count,
            properties.Data);
    #ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(
                properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(
                VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    #endif

        float queuePriority = 1.0F;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(), vkContext.queueFamilyIndex, 1,
            &queuePriority);
        vk::DeviceCreateInfo deviceCreateInfo{ {}, deviceQueueCreateInfo };
        deviceCreateInfo.setEnabledExtensionCount(device_extensions.size());
        deviceCreateInfo.setPpEnabledExtensionNames(device_extensions.Data);
        vkContext.device =
            vkContext.physicalDevice.createDevice(deviceCreateInfo);
        vkContext.queue =
            vkContext.device.getQueue(vkContext.queueFamilyIndex, 0);
    }

    // create desc pool
    {
        vk::DescriptorPoolSize poolSize(
            vk::DescriptorType::eCombinedImageSampler, 1);
        vk::DescriptorPoolCreateInfo descritporPoolCreateInfo = {
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, poolSize
        };
        vkContext.descriptorPool =
            vkContext.device.createDescriptorPool(descritporPoolCreateInfo);
    }

    return true;
}

bool SetupVulkanWindow(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext& vkContext,
    VkSurfaceKHR surface, int width, int height, uint8_t framebufferCount) {
    imguiVkWnd->Surface = surface;

    VkBool32 result{};
    vkGetPhysicalDeviceSurfaceSupportKHR(
        vkContext.physicalDevice, vkContext.queueFamilyIndex, surface, &result);
    if (result != VK_TRUE) {
        println("Error: No WSI support on physical device");
        return false;
    }

    constexpr std::array<VkFormat, 4> surface_formats = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM
    };
    const VkColorSpaceKHR surface_color_space =
        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    imguiVkWnd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        vkContext.physicalDevice, surface, surface_formats.data(),
        surface_formats.size(), surface_color_space);

    auto present_modes = make_array<VkPresentModeKHR>(
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR);
    imguiVkWnd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        vkContext.physicalDevice, surface, present_modes.data(),
        present_modes.size());

    // create swap chain
    IM_ASSERT(framebufferCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        vkContext.instance, vkContext.physicalDevice, vkContext.device,
        imguiVkWnd, vkContext.queueFamilyIndex, vkContext.allocator, width,
        height, framebufferCount, 0);

    return true;
}

void CleanupVulkanWindows(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext& vkContext) {
    ImGui_ImplVulkanH_DestroyWindow(
        vkContext.instance, vkContext.device, imguiVkWnd, vkContext.allocator);
}

void CleanupVulkan(VulkanContext& vkContext) {
    if (vkContext.device) {
        if (vkContext.descriptorPool) {
            vkContext.device.destroyDescriptorPool(vkContext.descriptorPool);
            vkContext.descriptorPool = nullptr;
        }
        vkContext.device.destroy();
        vkContext.device = nullptr;
    }
    if (vkContext.instance) {
        vkContext.instance.destroy();
        vkContext.instance = nullptr;
    }
}

void GlfwErrorCallback(int error, const char* description) {
    println(std::cerr, "GLFW Error {}: {}", error, description);
}

bool IsExtensionAvailable(
    const std::vector<vk::ExtensionProperties>& properties,
    const char* extension) {
    for (const auto& property : properties) {
        if (std::strcmp(property.extensionName, extension) == 0) {
            return true;
        }
    }
    return false;
}

bool IsExtensionAvailable(
    const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const auto& property : properties) {
        if (std::strcmp(property.extensionName, extension) == 0) {
            return true;
        }
    }
    return false;
}

bool IsExtensionEnabled(
    const std::vector<const char*>& enabledExtensions, const char* extension) {
    for (const auto* enabledExtension : enabledExtensions) {
        if (std::strcmp(enabledExtension, extension) == 0) {
            return true;
        }
    }
    return false;
}

void ImGuiCheckVkResult(VkResult result) {
    if (result == VK_SUCCESS) [[likely]] {
        return;
    }
    println(std::cerr, "[vulkan] Error: VkResult={}", static_cast<int>(result));
    if (result < 0) {
        std::abort();
    }
}

static void CheckVkResult(vk::Result result) {
    if (result == vk::Result::eSuccess) [[likely]] {
        return;
    }
    auto res = static_cast<VkResult>(result);
    println(std::cerr, "[vulkan] Error: VkResult={}", static_cast<int>(res));
    if (res < 0) {
        std::abort();
    }
}

void Render(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext& vkContext,
    ImDrawData* drawData) {
    VkSemaphore image_acquired_semaphore =
        imguiVkWnd->FrameSemaphores[imguiVkWnd->SemaphoreIndex]
            .ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore =
        imguiVkWnd->FrameSemaphores[imguiVkWnd->SemaphoreIndex]
            .RenderCompleteSemaphore;

    VkResult result = vkAcquireNextImageKHR(
        vkContext.device, imguiVkWnd->Swapchain, UINT64_MAX,
        image_acquired_semaphore, VK_NULL_HANDLE, &imguiVkWnd->FrameIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        vkContext.rebuildSwapchain = true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR) {
        ImGuiCheckVkResult(result);
    }

    ImGui_ImplVulkanH_Frame* frame =
        &imguiVkWnd->Frames[imguiVkWnd->FrameIndex];
    {
        result = vkWaitForFences(
            vkContext.device, 1, &frame->Fence, VK_TRUE,
            UINT64_MAX); // wait indefinitely instead of periodically checking
        ImGuiCheckVkResult(result);

        result = vkResetFences(vkContext.device, 1, &frame->Fence);
        ImGuiCheckVkResult(result);
    }
    {
        result = vkResetCommandPool(vkContext.device, frame->CommandPool, 0);
        ImGuiCheckVkResult(result);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(frame->CommandBuffer, &info);
        ImGuiCheckVkResult(result);
    }
    {
        VkRenderPassBeginInfo info   = {};
        info.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass              = imguiVkWnd->RenderPass;
        info.framebuffer             = frame->Framebuffer;
        info.renderArea.extent.width = imguiVkWnd->Width;
        info.renderArea.extent.height = imguiVkWnd->Height;
        info.clearValueCount          = 1;
        info.pClearValues             = &imguiVkWnd->ClearValue;
        vkCmdBeginRenderPass(
            frame->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(drawData, frame->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(frame->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info         = {};
        info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount   = 1;
        info.pWaitSemaphores      = &image_acquired_semaphore;
        info.pWaitDstStageMask    = &wait_stage;
        info.commandBufferCount   = 1;
        info.pCommandBuffers      = &frame->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores    = &render_complete_semaphore;

        result = vkEndCommandBuffer(frame->CommandBuffer);
        ImGuiCheckVkResult(result);
        result = vkQueueSubmit(vkContext.queue, 1, &info, frame->Fence);
        ImGuiCheckVkResult(result);
    }
}

void Present(ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext& vkContext) {
    if (vkContext.rebuildSwapchain) {
        return;
    }

    VkSemaphore render_complete_semaphore =
        imguiVkWnd->FrameSemaphores[imguiVkWnd->SemaphoreIndex]
            .RenderCompleteSemaphore;
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &render_complete_semaphore;
    info.swapchainCount     = 1;
    info.pSwapchains        = &imguiVkWnd->Swapchain;
    info.pImageIndices      = &imguiVkWnd->FrameIndex;
    vk::Result result       = vkContext.queue.presentKHR(info);
    using enum vk::Result;
    if (result == eErrorOutOfDateKHR || result == eSuboptimalKHR) {
        vkContext.rebuildSwapchain = true;
    }
    if (result == eErrorOutOfDateKHR) {
        return;
    }
    if (result != eSuboptimalKHR) {
        CheckVkResult(result);
    }
    imguiVkWnd->SemaphoreIndex =
        (imguiVkWnd->SemaphoreIndex + 1) % imguiVkWnd->SemaphoreCount;
}

#endif
