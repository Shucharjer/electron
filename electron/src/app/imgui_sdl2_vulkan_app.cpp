#ifdef ELECTRON_USES_SDL2_VULKAN

    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <vector>
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_error.h>
    #include <SDL2/SDL_events.h>
    #include <SDL2/SDL_video.h>
    #include <SDL2/SDL_vulkan.h>
    #include <imgui.h>
    #include <imgui_impl_sdl2.h>
    #include <imgui_impl_vulkan.h>
    #include <vulkan/vulkan_core.h>
    #include <neutron/print.hpp>
    #include <neutron/utility.hpp>
    #include <vulkan/vulkan.hpp>
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

class App::Impl {
public:
    VulkanContext* init(const wnd_config& config) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            println("Error: SDL_Init(): {}", SDL_GetError());
            return nullptr;
        }

        using enum window_flags;
        auto sdlwnd_flags = static_cast<int>(SDL_WINDOW_VULKAN);
        sdlwnd_flags |= (config.flags & fullscreen) ? SDL_WINDOW_FULLSCREEN : 0;
        sdlwnd_flags |= (config.flags & resizable) ? SDL_WINDOW_RESIZABLE : 0;
        sdlwnd_flags |= (config.flags & borderless) ? SDL_WINDOW_BORDERLESS : 0;
        sdlwnd_flags |= (config.flags & maximized) ? SDL_WINDOW_MAXIMIZED : 0;
        auto window_flags = static_cast<SDL_WindowFlags>(sdlwnd_flags);

        sdlWindow_ = SDL_CreateWindow(
            config.name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            config.width, config.height, window_flags);
        if (sdlWindow_ == nullptr) {
            println("Error: SDL_CreateWindow(): {}", SDL_GetError());
            return nullptr;
        }

        std::vector<const char*> extensions;
        uint32_t sdl_extension_count = 0;
        SDL_Vulkan_GetInstanceExtensions(
            sdlWindow_, &sdl_extension_count, nullptr);
        extensions.resize(sdl_extension_count);
        SDL_Vulkan_GetInstanceExtensions(
            sdlWindow_, &sdl_extension_count, extensions.data());

        SetupVulkan(vkContext_, config.name, extensions);

        VkSurfaceKHR surface{};
        VkResult result{};
        if (SDL_Vulkan_CreateSurface(
                sdlWindow_, vkContext_.instance, &surface) == 0) {
            println("Error: SDL_Vulkan_CreateSurface(): {}", SDL_GetError());
            return nullptr;
        }

        ImGui_ImplVulkanH_Window* imgui_vulkan_window = &imguiWindowData_;
        SetupVulkanWindow(
            imgui_vulkan_window, vkContext_, surface, config.width,
            config.height,framebufferCount_);
        SDL_ShowWindow(sdlWindow_);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        // dpi

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        // dpi

        ImGui_ImplSDL2_InitForVulkan(sdlWindow_);
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance       = vkContext_.instance;
        init_info.PhysicalDevice = vkContext_.physicalDevice;
        init_info.Device         = vkContext_.device;
        init_info.Queue          = vkContext_.queue;
        // init_info.PipelineCache   = g_pipeline_cache;
        init_info.PipelineCache   = nullptr;
        init_info.DescriptorPool  = vkContext_.descriptorPool;
        init_info.RenderPass      = imgui_vulkan_window->RenderPass;
        init_info.Subpass         = 0;
        init_info.MinImageCount   = 2;
        init_info.ImageCount      = imgui_vulkan_window->ImageCount;
        init_info.MSAASamples     = VK_SAMPLE_COUNT_4_BIT;
        init_info.Allocator       = vkContext_.allocator;
        init_info.CheckVkResultFn = &ImGuiCheckVkResult;
        ImGui_ImplVulkan_Init(&init_info);

        // load fonts
        //

        return &vkContext_;
    }

    bool pollEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) [[unlikely]] {
                stopped_ = true;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(sdlWindow_)) {
                stopped_ = true;
            }
        }

        if ((SDL_GetWindowFlags(sdlWindow_) & SDL_WINDOW_MINIMIZED) != 0) {
            return false;
        }

        int width{};
        int height{};
        SDL_GetWindowSize(sdlWindow_, &width, &height);
        if (width > 0 && height > 0 && (vkContext_.rebuildSwapchain)) {
            ImGui_ImplVulkan_SetMinImageCount(framebufferCount_);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                vkContext_.instance, vkContext_.physicalDevice,
                vkContext_.device, &imguiWindowData_,
                vkContext_.queueFamilyIndex, vkContext_.allocator, width,
                height, framebufferCount_);
            imguiWindowData_.FrameIndex = 0;
            vkContext_.rebuildSwapchain = false;
        }

        return true;
    }

    ATOM_NODISCARD bool is_stopped() const noexcept { return stopped_; }

    // NOLINTNEXTLINE
    void renderBegin() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
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
        auto& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        if (!minimized) {
            ::Present(&imguiWindowData_, vkContext_);
        }
    }

    void destroy() {
        vkContext_.device.waitIdle();
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        CleanupVulkanWindows(&imguiWindowData_, vkContext_);
        CleanupVulkan(vkContext_);
        SDL_DestroyWindow(sdlWindow_);
        SDL_Quit();
    }

private:
    uint8_t framebufferCount_;
    bool stopped_           = false;
    SDL_Window* sdlWindow_ = nullptr;
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
        const auto extensionCount = extensionProperties.size();
        static std::vector<const char*> extensionNames(extensionCount, nullptr);
        for (auto i = 0; i < extensionCount; ++i) {
            extensionNames[i] = extensionProperties[i].extensionName;
        }

        vk::ApplicationInfo appInfo{ appName.c_str(), 1, "atom", 1,
                                     VK_API_VERSION_1_0 };

        vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);
        instanceCreateInfo.setEnabledExtensionCount(extensionProperties.size())
            .setPpEnabledExtensionNames(extensionNames.data());

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
        imguiVkWnd, vkContext.queueFamilyIndex, {}, width, height,
        framebufferCount);

    return true;
}

void CleanupVulkanWindows(
    ImGui_ImplVulkanH_Window* imguiVkWnd, VulkanContext& vkContext) {
    ImGui_ImplVulkanH_DestroyWindow(
        vkContext.instance, vkContext.device, imguiVkWnd, vkContext.allocator);
}

void CleanupVulkan(VulkanContext& vkContext) {
    vkContext.device.destroyDescriptorPool(vkContext.descriptorPool);
    vkContext.device.destroy();
    vkContext.instance.destroy();
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
