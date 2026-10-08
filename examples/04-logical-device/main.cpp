#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <utility>

class ComputeApplication
{
public:
    bool run()
    {
        if (!initVulkan())
        {
            return false;
        }
        return true;
    }

private:
    static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    std::uint32_t computeQueueFamilyIndex = 0;
    // TODO: device와 computeQueue 멤버를 추가합니다.

    bool initVulkan()
    {
        // TODO: 마지막에 createLogicalDevice() 호출을 연결합니다.
        return createInstance() && setupDebugMessenger() && pickPhysicalDevice();
    }

    bool checkValidationLayerSupport()
    {
        const auto layers = context.enumerateInstanceLayerProperties(); // C API: vkEnumerateInstanceLayerProperties
        for (const auto& layer : layers)
        {
            if (std::strcmp(layer.layerName, validationLayer) == 0)
            {
                return true;
            }
        }
        std::cerr << "Required layer not found: " << validationLayer << '\n';
        return false;
    }

    bool createInstance()
    {
        if (!checkValidationLayerSupport())
        {
            return false;
        }

        const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
        bool debugUtilsAvailable = false;
        std::cout << "Available instance extensions:\n";
        for (const auto& extension : extensions)
        {
            std::cout << "  " << extension.extensionName << '\n';
            if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            {
                debugUtilsAvailable = true;
            }
        }
        if (!debugUtilsAvailable)
        {
            std::cerr << "Required instance extension not found: VK_EXT_debug_utils\n";
            return false;
        }

        vk::ApplicationInfo appInfo{};
        appInfo.pApplicationName = "04-logical-device";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        const char* requiredExtensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
        auto debugCreateInfo = makeDebugMessengerCreateInfo();
        vk::InstanceCreateInfo createInfo{};
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
        createInfo.enabledExtensionCount = 1;
        createInfo.ppEnabledExtensionNames = requiredExtensions;
        createInfo.pNext = &debugCreateInfo;

        instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
        std::cout << "Instance created.\n"
                  << "Validation layer enabled: " << validationLayer << '\n';
        return true;
    }

    static vk::DebugUtilsMessengerCreateInfoEXT makeDebugMessengerCreateInfo()
    {
        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                   | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        createInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                               | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                               | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
        createInfo.setPfnUserCallback(debugCallback);
        return createInfo;
    }

    bool setupDebugMessenger()
    {
        debugMessenger = instance.createDebugUtilsMessengerEXT(makeDebugMessengerCreateInfo()); // C API: vkCreateDebugUtilsMessengerEXT
        std::cout << "Debug messenger created.\n";
        return true;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void*)
    {
        std::cerr << "[debug "
                  << vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(severity))
                  << ' ' << vk::to_string(vk::DebugUtilsMessageTypeFlagsEXT(type)) << "] "
                  << (callbackData->pMessageIdName ? callbackData->pMessageIdName : "unnamed")
                  << ": " << callbackData->pMessage << '\n';
        return VK_FALSE;
    }

    bool pickPhysicalDevice()
    {
        auto physicalDevices = instance.enumeratePhysicalDevices(); // C API: vkEnumeratePhysicalDevices
        if (physicalDevices.empty())
        {
            std::cerr << "No Vulkan physical devices found.\n";
            return false;
        }

        for (auto& candidate : physicalDevices)
        {
            const auto properties = candidate.getProperties(); // C API: vkGetPhysicalDeviceProperties
            if (properties.apiVersion < VK_API_VERSION_1_3)
            {
                continue;
            }
            const auto queueFamily = findComputeQueueFamily(candidate);
            if (!queueFamily)
            {
                continue;
            }

            physicalDevice = std::move(candidate);
            computeQueueFamilyIndex = *queueFamily;
            printDeviceInfo();
            return true;
        }
        std::cerr << "No Vulkan 1.3 physical device with a compute queue family found.\n";
        return false;
    }

    static std::optional<std::uint32_t> findComputeQueueFamily(
        const vk::raii::PhysicalDevice& candidate)
    {
        const auto queueFamilies = candidate.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
        for (std::uint32_t index = 0; index < queueFamilies.size(); ++index)
        {
            const auto& family = queueFamilies[index];
            if (family.queueCount > 0 && (family.queueFlags & vk::QueueFlagBits::eCompute))
            {
                return index;
            }
        }
        return std::nullopt;
    }

    void printDeviceInfo()
    {
        const auto properties = physicalDevice.getProperties(); // C API: vkGetPhysicalDeviceProperties
        const auto features = physicalDevice.getFeatures(); // C API: vkGetPhysicalDeviceFeatures
        const auto queueFamilies = physicalDevice.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
        const auto& limits = properties.limits;

        std::cout << "Selected physical device: " << properties.deviceName << '\n'
                  << "Device type: " << vk::to_string(properties.deviceType) << '\n';
        std::cout << "Device API version: "
                  << VK_API_VERSION_MAJOR(properties.apiVersion) << '.'
                  << VK_API_VERSION_MINOR(properties.apiVersion) << '.'
                  << VK_API_VERSION_PATCH(properties.apiVersion) << '\n';
        std::cout << "Compute queue family index: " << computeQueueFamilyIndex << '\n'
                  << "Queues in selected family: " << queueFamilies[computeQueueFamilyIndex].queueCount << '\n'
                  << "shaderFloat64 supported (not enabled): " << (features.shaderFloat64 ? "yes" : "no") << '\n'
                  << "maxComputeWorkGroupCount: " << limits.maxComputeWorkGroupCount[0] << ", "
                  << limits.maxComputeWorkGroupCount[1] << ", " << limits.maxComputeWorkGroupCount[2] << '\n'
                  << "maxComputeWorkGroupSize: " << limits.maxComputeWorkGroupSize[0] << ", "
                  << limits.maxComputeWorkGroupSize[1] << ", " << limits.maxComputeWorkGroupSize[2] << '\n'
                  << "maxComputeWorkGroupInvocations: " << limits.maxComputeWorkGroupInvocations << '\n'
                  << "maxStorageBufferRange: " << limits.maxStorageBufferRange << " bytes\n";
    }
    // TODO: createLogicalDevice()를 추가합니다.
};

int main()
{
    {
        ComputeApplication app;
        if (!app.run())
        {
            return 1;
        }
    }

    // TODO: 논리적 디바이스가 해제되었음을 출력하도록 메시지를 바꿉니다.
    std::cout << "Debug messenger and instance destroyed.\n";
    return 0;
}
