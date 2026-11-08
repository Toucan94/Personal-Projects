#if defined (__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cassert>
#include <fstream>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescription() {
        return {{
            {.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos)},
            {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)}
        }};
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

const std::vector<char const*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    //std::cout << buffer.size() << std::endl;

    return buffer;
}

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow *window                                  = nullptr;
    
    vk::raii::Context context;

    vk::raii::Instance instance                         = nullptr;

    vk::raii::DebugUtilsMessengerEXT debugMessenger     = nullptr;
    
    vk::raii::SurfaceKHR surface                        = nullptr;
    
    std::vector<const char*> requiredDeviceExtension    = { vk::KHRSwapchainExtensionName };
    vk::raii::PhysicalDevice physicalDevice             = nullptr;
    vk::raii::Device device                             = nullptr;
    
    uint32_t queueIndex                                 = ~0;
    vk::raii::Queue queue                               = nullptr;
    
    vk::Extent2D swapChainExtent;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::raii::SwapchainKHR swapChain                    = nullptr;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::raii::ImageView> swapChainImageViews;
    
    vk::raii::PipelineLayout pipelineLayout             = nullptr;
    vk::raii::Pipeline graphicsPipeline                 = nullptr;
    
    vk::raii::CommandPool commandPool                   = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    uint32_t frameIndex                                 = 0;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;

    bool frameBufferResized                             = false;

    vk::raii::Buffer vertexBuffer                       = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory           = nullptr;
    vk::raii::Buffer indexBuffer                        = nullptr;
    vk::raii::DeviceMemory indexBufferMemory            = nullptr;

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT  * pCallbackData,
                                                          void *                                        pUserData)
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
        }

        return vk::False;
    }

    void initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->frameBufferResized = true;
    }

    std::vector<const char*> getRequiredInstanceExtensions() {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers) {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        return extensions;
    }

    void createInstance(const bool listExtensions = false) {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName     = "Hello Triangle",
            .applicationVersion   = VK_MAKE_VERSION( 1, 0, 0 ),
            .pEngineName          = "No Engine",
            .engineVersion        = VK_MAKE_VERSION( 1, 0, 0 ),
            .apiVersion           = vk::ApiVersion14
        };

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
                                                        [&layerProperties](auto const &requiredLayer) {
                                                            return std::ranges::none_of(layerProperties,
                                                                [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
                                                        });

        if (unsupportedLayerIt != requiredLayers.end()) {
            throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredInstanceExtensions();

        // Check if the required GLFW extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions,
                                        [&extensionProperties](auto const *requiredExtension) {
                                            return std::ranges::none_of(extensionProperties,
                                                                            [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
                                        });
        if (unsupportedPropertyIt != requiredExtensions.end()) {
            throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames     = requiredLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data()
        };

        instance = vk::raii::Instance(context, createInfo);

        if (listExtensions) {
            auto extensions = context.enumerateInstanceExtensionProperties();
            std::cout << "available extensions:" << std::endl;

            for (const auto& extension : extensions) {
                std::cout << "\t" << extension.extensionName << '\n';
            }
        }
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity    = severityFlags,
            .messageType        = messageTypeFlags,
            .pfnUserCallback    = &debugCallback
        };

        debugMessenger = instance.createDebugUtilsMessengerEXT( debugUtilsMessengerCreateInfoEXT );
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice const & physicalDevice) {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures = physicalDevice.getFeatures();

        // Required API version check
        bool supportsVulkan1_3 = deviceProperties.apiVersion >= vk::ApiVersion13;

        // Required queue family support check
        auto queueFamilies = physicalDevice.getQueueFamilyProperties();
        bool supportsGraphics =
            std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

        // Required extension check
        auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        bool supportsAllRequiredExtensions =
            std::ranges::all_of(requiredDeviceExtension,
                                [&availableDeviceExtensions](auto const & requiredDeviceExtension)
                                {
                                    return std::ranges::any_of(availableDeviceExtensions,
                                                                [requiredDeviceExtension](auto const & availableDeviceExtension)
                                                                { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
                                });

        // Required feature check
        auto features = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                             vk::PhysicalDeviceVulkan11Features,
                                                             vk::PhysicalDeviceVulkan13Features,
                                                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                                        features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                        features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

        return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
    }

    void createSurface() {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    void pickPhysicalDevice() {
        std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
        auto const devIter = std::ranges::find_if(physicalDevices, [&](auto const & physicalDevice) { return isDeviceSuitable(physicalDevice); });

        if (devIter == physicalDevices.end()) {
            throw std::runtime_error("Failed to find a suitable GPU.");
        }
        physicalDevice = *devIter;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const &availableFormats) {
        const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; }
        );
        return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes) {
        assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
        return std::ranges::any_of(availablePresentModes,
                                    [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
                    vk::PresentModeKHR::eMailbox :
                    vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) {
        // currentExtent is only set to the special "undefined" described above
        // when the window manager lets us choose the extent ourselves; any other value
        // means the surface already dectates a fixed extent that we must use as-is.
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    void createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // Get the first index into queueFamilyProperties which supports both graphics and present
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++) {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface)) {
                // found a queue family that supports both graphics and present
                queueIndex = qfpIndex;
                break;
            }
        }

        if (queueIndex == ~0) {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // queue for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},
                {.shaderDrawParameters = true},
                {.synchronization2 = true, .dynamicRendering = true},
                {.extendedDynamicState = true}
            };
        
        // create a device
        float queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &deviceQueueCreateInfo,
            .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data()
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities) {
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount)) {
            minImageCount = surfaceCapabilities.maxImageCount;
        }
        return minImageCount;
    }

    void createSwapChain() {
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
        swapChainExtent                                = chooseSwapExtent(surfaceCapabilities);
        uint32_t minImageCount                         = chooseSwapMinImageCount(surfaceCapabilities);

        std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
        swapChainSurfaceFormat                             = chooseSwapSurfaceFormat(availableFormats);

        std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR( *surface );

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = *surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainSurfaceFormat.format,
            .imageColorSpace = swapChainSurfaceFormat.colorSpace,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(availablePresentModes),
            .clipped = true,
            .oldSwapchain = nullptr
        };

        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    void createImageViews() {
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{
            .viewType         = vk::ImageViewType::e2D,
            .format           = swapChainSurfaceFormat.format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
        };

        for (auto &image : swapChainImages) {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back( device, imageViewCreateInfo );
        }
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const {
        vk::ShaderModuleCreateInfo createInfo{
            .codeSize = code.size() * sizeof(char),
            .pCode = reinterpret_cast<const uint32_t*>(code.data())
        };

        vk::raii::ShaderModule shaderModule{ device, createInfo };

        return shaderModule;
    }

    void createGraphicsPipeline() {
        vk::raii::ShaderModule shaderModule = createShaderModule(readFile(SHADER_DIR "slang.spv"));

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = shaderModule,
            .pName  = "vertMain"
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = shaderModule,
            .pName  = "fragMain"
        };

        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescription();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data()
        };

        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList
        };

        // vk::Viewport viewport{
        //     0.0f,
        //     0.0f,
        //     static_cast<float>(swapChainExtent.width),
        //     static_cast<float>(swapChainExtent.height),
        //     0.0f,
        //     1.0f
        // };

        // vk::Rect2D scissor{
        //     vk::Offset2D{ 0, 0 },
        //     swapChainExtent
        // };

        vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

        vk::PipelineRasterizationStateCreateInfo rasterizer{
            .depthClampEnable        = vk::False,
            .rasterizerDiscardEnable = vk::False,
            .polygonMode             = vk::PolygonMode::eFill,
            .cullMode                = vk::CullModeFlagBits::eBack,
            .frontFace               = vk::FrontFace::eClockwise,
            .depthBiasEnable         = vk::False,
            .lineWidth               = 1.0f
        };

        vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable         = vk::True,
            .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
            .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
            .colorBlendOp        = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp        = vk::BlendOp::eAdd,
            .colorWriteMask      = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable   = vk::False,
            .logicOp         = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment
        };

        std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()
        };

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
            .setLayoutCount = 0,
            .pushConstantRangeCount = 0
        };

        pipelineLayout = vk::raii::PipelineLayout{device, pipelineLayoutInfo};

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
            {.stageCount          = 2,
             .pStages             = shaderStages,
             .pVertexInputState   = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState      = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState   = &multisampling,
             .pColorBlendState    = &colorBlending,
             .pDynamicState       = &dynamicState,
             .layout              = pipelineLayout,
             .renderPass          = nullptr},
            {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}
        };

        graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

    void createCommandPool() {
        vk::CommandPoolCreateInfo poolInfo{
            .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueIndex
        };

        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandBuffers() {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool        = commandPool,
            .level              = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = MAX_FRAMES_IN_FLIGHT
        };

        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex) {
        auto &commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});
        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput
        );

        vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView   = swapChainImageViews[imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = clearColor
        };

        vk::RenderingInfo renderingInfo = {
            .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo
        };

        commandBuffer.beginRendering(renderingInfo);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        
        commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
        commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);

        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        commandBuffer.endRendering();

        transition_image_layout(
            imageIndex,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eBottomOfPipe
        );

        commandBuffer.end();
    }

    void transition_image_layout(
        uint32_t imageIndex,
        vk::ImageLayout         old_layout,
        vk::ImageLayout         new_layout,
        vk::AccessFlags2        src_access_mask,
        vk::AccessFlags2        dst_access_mask,
        vk::PipelineStageFlags2 src_stage_mask,
        vk::PipelineStageFlags2 dst_stage_mask)
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask        = src_stage_mask,
            .srcAccessMask       = src_access_mask,
            .dstStageMask        = dst_stage_mask,
            .dstAccessMask       = dst_access_mask,
            .oldLayout           = old_layout,
            .newLayout           = new_layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = swapChainImages[imageIndex],
            .subresourceRange    = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1}
        };

        vk::DependencyInfo dependency_info = {
            .dependencyFlags         = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier
        };

        commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
    }

    void createSyncObjects() {
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
        vk::BufferCreateInfo bufferInfo{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive
        };

        vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);

        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

        vk::MemoryAllocateInfo memoryAllocateInfo{
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
        };

        vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, memoryAllocateInfo);

        buffer.bindMemory(*bufferMemory, 0);
        return {std::move(buffer), std::move(bufferMemory)};
    }

    void copyBuffer(vk::raii::Buffer & srcBuffer, vk::raii::Buffer & dstBuffer, vk::DeviceSize size) {
        vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };

        vk::raii::CommandBuffer commandCopyBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

        commandCopyBuffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy(0, 0, size));

        commandCopyBuffer.end();

        queue.submit(vk::SubmitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandCopyBuffer}, nullptr);
        queue.waitIdle();
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        auto [stagingBuffer, stagingBufferMemory] = 
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, vertices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(vertexBuffer, vertexBufferMemory) =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        auto [stagingBuffer, stagingBufferMemory] =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, indices.data(), (size_t) bufferSize);
        stagingBufferMemory.unmapMemory();

        std::tie(indexBuffer, indexBufferMemory) = 
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame();
        }

        device.waitIdle();
    }

    void cleanupSwapChain() {
        swapChainImageViews.clear();
        swapChain = nullptr;
    }
    
    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while ((width == 0 || height == 0) && !glfwWindowShouldClose(window)) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        if (glfwWindowShouldClose(window)) {
            return;
        }

        device.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
    }

    void drawFrame() {
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess) {
            throw std::runtime_error("failed to wait for fence!");
        }

        auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);
        
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        }

        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        device.resetFences(*inFlightFences[frameIndex]);

        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo submitInfo{
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
            .pWaitDstStageMask    = &waitDestinationStageMask,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &*commandBuffers[frameIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]
        };

        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
            .swapchainCount     = 1,
            .pSwapchains        = &*swapChain,
            .pImageIndices      = &imageIndex
        };

        result = queue.presentKHR(presentInfoKHR);

        if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR || frameBufferResized)) {
            frameBufferResized = false;
            recreateSwapChain();
        }
        else {
            // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
            assert(result == vk::Result::eSuccess);
        }

        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup() {
        cleanupSwapChain();

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    try {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
