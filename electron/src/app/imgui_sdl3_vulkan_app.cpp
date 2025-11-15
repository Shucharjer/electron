#define ELECTRON_USES_SDL3_VULKAN
#ifdef ELECTRON_USES_SDL3_VULKAN

    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <vector>
    #include <SDL3/SDL.h>
    #include <SDL3/SDL_error.h>
    #include <SDL3/SDL_events.h>
    #include <SDL3/SDL_init.h>
    #include <SDL3/SDL_video.h>
    #include <SDL3/SDL_vulkan.h>
    #include <imgui.h>
    #include <imgui_impl_sdl3.h>
    #include <imgui_impl_vulkan.h>
    #include <vulkan/vulkan_core.h>
    #include <neutron/neutron.hpp>
    #include <neutron/print.hpp>
    #include "electron/app/app.hpp"
    #include "electron/app/config.hpp"

using namespace electron;
using neutron::println;

// NOLINTBEGIN

inline static VkPhysicalDevice g_physical_device = VK_NULL_HANDLE;
inline static VkInstance g_instance              = VK_NULL_HANDLE;
inline static VkAllocationCallbacks* g_allocator = nullptr;
inline static uint32_t g_queue_family            = 0;
inline static VkDevice g_device                  = VK_NULL_HANDLE;
inline static VkQueue g_queue                    = VK_NULL_HANDLE;
inline static VkDescriptorPool g_descriptor_pool = VK_NULL_HANDLE;
inline static uint32_t g_min_image_count         = 2;
inline static VkPipelineCache g_pipeline_cache   = VK_NULL_HANDLE;
inline static bool g_swapchain_rebuild           = false;

// NOLINTEND

static bool setup_vulkan(std::vector<const char*> instance_extensions);
static bool setup_vulkan_window(
    ImGui_ImplVulkanH_Window* imgui_vk_wnd, VkSurfaceKHR surface, int width,
    int height);
static void cleanup_vulkan_window(ImGui_ImplVulkanH_Window* imgui_vk_wnd);
static void cleanup_vulkan();
static void
    render(ImGui_ImplVulkanH_Window* imgui_vk_wnd, ImDrawData* draw_data);
static void present(ImGui_ImplVulkanH_Window* imgui_vk_wnd);

class app::impl {
public:
    bool init(const wnd_config& config) {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            println("Error: SDL_Init(): {}", SDL_GetError());
            return false;
        }

        using enum window_flags;
        SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;
        window_flags |= (config.flags & fullscreen) ? SDL_WINDOW_FULLSCREEN : 0;
        window_flags |= (config.flags & resizable) ? SDL_WINDOW_RESIZABLE : 0;
        window_flags |= (config.flags & borderless) ? SDL_WINDOW_BORDERLESS : 0;
        window_flags |= (config.flags & maximized) ? SDL_WINDOW_MAXIMIZED : 0;
        sdl_window_ = SDL_CreateWindow(
            config.name.c_str(), config.width, config.height, window_flags);
        if (sdl_window_ == nullptr) {
            println("Error: SDL_CreateWindow(): {}", SDL_GetError());
            return false;
        }

        auto scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

        std::vector<const char*> extensions;
        uint32_t sdl_extension_count = 0;
        const char* const* sdl_extensions =
            SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
        for (uint32_t i = 0; i < sdl_extension_count; ++i) {
            extensions.emplace_back(sdl_extensions[i]); // NOLINT
        }
        setup_vulkan(extensions);

        VkSurfaceKHR surface{};
        VkResult result{};
        if (!SDL_Vulkan_CreateSurface(
                sdl_window_, g_instance, g_allocator, &surface)) {
            println("Error: SDL_Vulkan_CreateSurface(): {}", SDL_GetError());
            return false;
        }

        ImGui_ImplVulkanH_Window* imgui_vulkan_window = &imgui_window_data_;
        setup_vulkan_window(
            imgui_vulkan_window, surface, config.width, config.height);
        SDL_ShowWindow(sdl_window_);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // dpi

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(scale);
        // dpi

        ImGui_ImplSDL3_InitForVulkan(sdl_window_);
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance        = g_instance;
        init_info.PhysicalDevice  = g_physical_device;
        init_info.Device          = g_device;
        init_info.Queue           = g_queue;
        init_info.PipelineCache   = g_pipeline_cache;
        init_info.DescriptorPool  = g_descriptor_pool;
        init_info.RenderPass      = imgui_vulkan_window->RenderPass;
        init_info.Subpass         = 0;
        init_info.MinImageCount   = g_min_image_count;
        init_info.ImageCount      = imgui_vulkan_window->ImageCount;
        init_info.MSAASamples     = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator       = g_allocator;
        init_info.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&init_info);

        // load fonts
        //

        return true;
    }

    void poll_events() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) [[unlikely]] {
                stopped_ = true;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                event.window.windowID == SDL_GetWindowID(sdl_window_)) {
                stopped_ = true;
            }
        }
    }

    NODISCARD bool is_stopped() const noexcept { return stopped_; }

    // NOLINTNEXTLINE
    void render_begin() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void render_end() {
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        render(&imgui_window_data_, draw_data);
        ::present(&imgui_window_data_);
    }

    void destroy() {
        vkDeviceWaitIdle(g_device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        cleanup_vulkan_window(&imgui_window_data_);
        cleanup_vulkan();
        SDL_DestroyWindow(sdl_window_);
        SDL_Quit();
    }

private:
    bool stopped_           = false;
    SDL_Window* sdl_window_ = nullptr;
    ImGui_ImplVulkanH_Window imgui_window_data_;
};

app::impl* app::_create_impl() { return new app::impl; }

bool app::_init_impl(app::impl* impl, const wnd_config& config) {
    return impl->init(config);
}

bool app::_is_stopped(app::impl* impl) { return impl->is_stopped(); }

void app::_poll_events(app::impl* impl) { impl->poll_events(); }

void app::_render_begin(app::impl* impl) { impl->render_begin(); }

void app::_render_end(app::impl* impl) { impl->render_end(); }

void app::_destroy_impl(app::impl* impl) {
    impl->destroy();
    delete impl;
}

    // NOLINTNEXTLINE
    #define CHECK_VK_RESULT(result, msg)                                       \
        if ((result) != VK_SUCCESS) {                                          \
            println(msg);                                                      \
            return false;                                                      \
        }                                                                      \
        //

bool setup_vulkan(std::vector<const char*> instance_extensions) {
    VkResult result{};

    // create instance
    {
        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        uint32_t properties_count{};
        vkEnumerateInstanceExtensionProperties(
            nullptr, &properties_count, nullptr);
        std::vector<VkExtensionProperties> properties(properties_count);
        result = vkEnumerateInstanceExtensionProperties(
            nullptr, &properties_count, properties.data());
        CHECK_VK_RESULT(
            result,
            "[vulkan] Failed at enumerate instance extension properties")

        auto check = [](const std::vector<VkExtensionProperties>& properties,
                        const char* extension) {
            for (const auto& property : properties) {
                if (std::strcmp(
                        static_cast<const char*>(property.extensionName),
                        extension) == 0) {
                    return true;
                }
            }
            return false;
        };
        if (check(
                properties,
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
            instance_extensions.emplace_back(
                VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

        create_info.enabledExtensionCount   = instance_extensions.size();
        create_info.ppEnabledExtensionNames = instance_extensions.data();
        result = vkCreateInstance(&create_info, g_allocator, &g_instance);
        CHECK_VK_RESULT(result, "[vulkan] Failed at creating instance");
    }

    // select physical device
    g_physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(g_instance);
    IM_ASSERT(g_physical_device != VK_NULL_HANDLE);

    // select graphics queue family
    g_queue_family =
        ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_physical_device);
    IM_ASSERT(g_queue_family != static_cast<uint32_t>(-1));

    // create logical deivec
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");
        uint32_t properties_count{};
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(
            g_physical_device, nullptr, &properties_count, nullptr);
        properties.resize(static_cast<int>(properties_count));
        vkEnumerateDeviceExtensionProperties(
            g_physical_device, nullptr, &properties_count, properties.Data);
    #ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(
                properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(
                VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    #endif

        auto queue_priority = make_array<float>(1.0F);
        auto queue_info =
            make_array<VkDeviceQueueCreateInfo>(VkDeviceQueueCreateInfo{});
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_queue_family;
        queue_info[0].queueCount       = 1;
        queue_info[0].pQueuePriorities = queue_priority.data();
        VkDeviceCreateInfo create_info = {};
        create_info.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount =
            sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos       = queue_info.data();
        create_info.enabledExtensionCount   = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        result                              = vkCreateDevice(
            g_physical_device, &create_info, g_allocator, &g_device);
        CHECK_VK_RESULT(result, "[vulkan] failed at creating device");
        vkGetDeviceQueue(g_device, g_queue_family, 0, &g_queue);
    }

    // create desc pool
    {
        using psize_t   = VkDescriptorPoolSize;
        auto pool_sizes = std::array{
            psize_t{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount =
                         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE }
        };
        VkDescriptorPoolCreateInfo create_info;
        create_info.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        create_info.flags   = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        create_info.maxSets = 0;
        for (auto& pool_size : pool_sizes) {
            create_info.maxSets += pool_size.descriptorCount;
        }
        create_info.poolSizeCount = pool_sizes.size();
        create_info.pPoolSizes    = pool_sizes.data();
        result                    = vkCreateDescriptorPool(
            g_device, &create_info, g_allocator, &g_descriptor_pool);
        CHECK_VK_RESULT(result, "[vulkan] Failed at create descriptor pool");
    }

    return true;
}

    #undef CHECK_VK_RESULT

bool setup_vulkan_window(
    ImGui_ImplVulkanH_Window* imgui_vk_wnd, VkSurfaceKHR surface, int width,
    int height) {
    imgui_vk_wnd->Surface = surface;

    VkBool32 result{};
    vkGetPhysicalDeviceSurfaceSupportKHR(
        g_physical_device, g_queue_family, surface, &result);
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
    imgui_vk_wnd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
        g_physical_device, surface, surface_formats.data(),
        surface_formats.size(), surface_color_space);

    auto present_modes = make_array<VkPresentModeKHR>(
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_FIFO_KHR);
    imgui_vk_wnd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        g_physical_device, surface, present_modes.data(), present_modes.size());

    // create swap chain
    IM_ASSERT(g_min_image_count);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        g_instance, g_physical_device, g_device, imgui_vk_wnd, g_queue_family,
        g_allocator, width, height, g_min_image_count);

    return true;
}
void cleanup_vulkan_window(ImGui_ImplVulkanH_Window* imgui_vk_wnd) {
    ImGui_ImplVulkanH_DestroyWindow(
        g_instance, g_device, imgui_vk_wnd, g_allocator);
}
void cleanup_vulkan() {
    vkDestroyDescriptorPool(g_device, g_descriptor_pool, g_allocator);
    vkDestroyDevice(g_device, g_allocator);
    vkDestroyInstance(g_instance, g_allocator);
}

void check_vk_result(VkResult result) {
    if (result == VK_SUCCESS) [[likely]] {
        return;
    }
    std::cerr << "[vulkan] Error: VkResult = " << result << '\n';
    if (result < 0) {
        std::abort();
    }
}

void render(ImGui_ImplVulkanH_Window* imgui_vk_wnd, ImDrawData* draw_data) {
    VkSemaphore image_acquired_semaphore =
        imgui_vk_wnd->FrameSemaphores[imgui_vk_wnd->SemaphoreIndex]
            .ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore =
        imgui_vk_wnd->FrameSemaphores[imgui_vk_wnd->SemaphoreIndex]
            .RenderCompleteSemaphore;

    VkResult result = vkAcquireNextImageKHR(
        g_device, imgui_vk_wnd->Swapchain, UINT64_MAX, image_acquired_semaphore,
        VK_NULL_HANDLE, &imgui_vk_wnd->FrameIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        g_swapchain_rebuild = true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR) {
        check_vk_result(result);
    }

    ImGui_ImplVulkanH_Frame* frame =
        &imgui_vk_wnd->Frames[imgui_vk_wnd->FrameIndex];
    {
        result = vkWaitForFences(
            g_device, 1, &frame->Fence, VK_TRUE,
            UINT64_MAX); // wait indefinitely instead of periodically checking
        check_vk_result(result);

        result = vkResetFences(g_device, 1, &frame->Fence);
        check_vk_result(result);
    }
    {
        result = vkResetCommandPool(g_device, frame->CommandPool, 0);
        check_vk_result(result);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(frame->CommandBuffer, &info);
        check_vk_result(result);
    }
    {
        VkRenderPassBeginInfo info   = {};
        info.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass              = imgui_vk_wnd->RenderPass;
        info.framebuffer             = frame->Framebuffer;
        info.renderArea.extent.width = imgui_vk_wnd->Width;
        info.renderArea.extent.height = imgui_vk_wnd->Height;
        info.clearValueCount          = 1;
        info.pClearValues             = &imgui_vk_wnd->ClearValue;
        vkCmdBeginRenderPass(
            frame->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, frame->CommandBuffer);

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
        check_vk_result(result);
        result = vkQueueSubmit(g_queue, 1, &info, frame->Fence);
        check_vk_result(result);
    }
}

void present(ImGui_ImplVulkanH_Window* imgui_vk_wnd) {
    if (g_swapchain_rebuild) {
        return;
    }

    VkSemaphore render_complete_semaphore =
        imgui_vk_wnd->FrameSemaphores[imgui_vk_wnd->SemaphoreIndex]
            .RenderCompleteSemaphore;
    VkPresentInfoKHR info{};
    info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = &render_complete_semaphore;
    info.swapchainCount     = 1;
    info.pSwapchains        = &imgui_vk_wnd->Swapchain;
    info.pImageIndices      = &imgui_vk_wnd->FrameIndex;
    VkResult result         = vkQueuePresentKHR(g_queue, &info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        g_swapchain_rebuild = true;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return;
    }
    if (result != VK_SUBOPTIMAL_KHR) {
        check_vk_result(result);
    }
    imgui_vk_wnd->SemaphoreIndex =
        (imgui_vk_wnd->SemaphoreIndex + 1) % imgui_vk_wnd->SemaphoreCount;
}

#endif
