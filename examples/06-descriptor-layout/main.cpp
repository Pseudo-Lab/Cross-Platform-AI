#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstddef>
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
        submitDebugMessage();
        return true;
    }

private:
    static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";

    static_assert(sizeof(float) == 4);
    static constexpr std::uint32_t elementCount = 8;
    static constexpr vk::DeviceSize bufferSize = sizeof(float) * elementCount;

    struct BufferResource
    {
        vk::raii::DeviceMemory memory = nullptr;
        vk::raii::Buffer buffer = nullptr;
    };

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    std::uint32_t computeQueueFamilyIndex = 0;
    vk::raii::Device device = nullptr;
    vk::raii::Queue computeQueue = nullptr;
    BufferResource inputA;
    BufferResource inputB;
    BufferResource output;
    // TODO: descriptorSetLayout 멤버를 추가합니다.

    bool initVulkan()
    {
        // TODO: createStorageBuffers() 다음에 createDescriptorSetLayout()을 호출합니다.
        return createInstance()
            && setupDebugMessenger()
            && pickPhysicalDevice()
            && createLogicalDevice()
            && createStorageBuffers();
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
        appInfo.pApplicationName = "06-descriptor-layout";
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

    void submitDebugMessage()
    {
        vk::DebugUtilsMessengerCallbackDataEXT callbackData{};
        callbackData.pMessageIdName = "tutorial.callback-check";
        callbackData.pMessage = "Application-injected callback check; this is not a validation error.";
        instance.submitDebugUtilsMessageEXT( // C API: vkSubmitDebugUtilsMessageEXT
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
            callbackData);
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

    bool createLogicalDevice()
    {
        const float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        vk::PhysicalDeviceFeatures enabledFeatures{};
        vk::DeviceCreateInfo createInfo{};
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.pEnabledFeatures = &enabledFeatures;

        device = vk::raii::Device(physicalDevice, createInfo); // C API: vkCreateDevice
        computeQueue = vk::raii::Queue(device, computeQueueFamilyIndex, 0); // C API: vkGetDeviceQueue
        std::cout << "Logical device created.\n"
                  << "Compute queue ready: family " << computeQueueFamilyIndex << ", queue 0.\n";
        return true;
    }

    bool createStorageBuffers()
    {
        if (!createBuffer(bufferSize, inputA)
            || !createBuffer(bufferSize, inputB)
            || !createBuffer(bufferSize, output))
        {
            return false;
        }

        std::array<float, elementCount> valuesA{};
        std::array<float, elementCount> valuesB{};
        const std::array<float, elementCount> initialOutput{};
        for (std::uint32_t i = 0; i < elementCount; ++i)
        {
            valuesA[i] = static_cast<float>(i);
            valuesB[i] = 10.0f + static_cast<float>(i);
        }

        if (!writeAndVerifyBuffer(inputA, valuesA, "Input A")
            || !writeAndVerifyBuffer(inputB, valuesB, "Input B")
            || !writeAndVerifyBuffer(output, initialOutput, "Output (initial)"))
        {
            return false;
        }

        std::cout << "Host-visible storage buffers ready: 3 x " << bufferSize << " bytes.\n"
                  << "CPU write/readback verified. No GPU dispatch yet.\n";
        return true;
    }

    bool createBuffer(vk::DeviceSize size, BufferResource& resource)
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.size = size;
        bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
        bufferInfo.sharingMode = vk::SharingMode::eExclusive;
        resource.buffer = vk::raii::Buffer(device, bufferInfo); // C API: vkCreateBuffer

        const auto requirements = resource.buffer.getMemoryRequirements(); // C API: vkGetBufferMemoryRequirements
        const auto properties = vk::MemoryPropertyFlagBits::eHostVisible
            | vk::MemoryPropertyFlagBits::eHostCoherent;
        std::uint32_t memoryTypeIndex = 0;
        if (!findMemoryType(requirements.memoryTypeBits, properties, memoryTypeIndex))
        {
            return false;
        }

        vk::MemoryAllocateInfo allocationInfo{};
        allocationInfo.allocationSize = requirements.size;
        allocationInfo.memoryTypeIndex = memoryTypeIndex;
        resource.memory = vk::raii::DeviceMemory(device, allocationInfo); // C API: vkAllocateMemory
        resource.buffer.bindMemory(*resource.memory, 0); // C API: vkBindBufferMemory

        std::cout << "Buffer memory: size " << requirements.size
                  << ", alignment " << requirements.alignment
                  << ", type " << memoryTypeIndex << '\n';
        return true;
    }

    bool findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                        std::uint32_t& typeIndex)
    {
        const auto memoryProperties = physicalDevice.getMemoryProperties(); // C API: vkGetPhysicalDeviceMemoryProperties
        for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            const bool allowed = (typeFilter & (1u << i)) != 0;
            const bool hasProperties =
                (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (allowed && hasProperties)
            {
                typeIndex = i;
                return true;
            }
        }

        std::cerr << "No compatible HOST_VISIBLE | HOST_COHERENT memory type found.\n";
        return false;
    }

    bool writeAndVerifyBuffer(BufferResource& resource,
                              const std::array<float, elementCount>& values,
                              const char* label)
    {
        void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
        std::memcpy(mapped, values.data(), static_cast<std::size_t>(bufferSize));
        resource.memory.unmapMemory(); // C API: vkUnmapMemory

        std::array<float, elementCount> readback{};
        mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
        std::memcpy(readback.data(), mapped, static_cast<std::size_t>(bufferSize));
        resource.memory.unmapMemory(); // C API: vkUnmapMemory
        if (readback != values)
        {
            std::cerr << label << ": CPU data verification failed.\n";
            return false;
        }

        std::cout << label << ':';
        for (float value : readback)
        {
            std::cout << ' ' << value;
        }
        std::cout << '\n';
        return true;
    }
    // TODO: createDescriptorSetLayout() 함수를 추가합니다.
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

    std::cout << "Buffers, memory, device, debug messenger and instance destroyed.\n";
    return 0;
}
