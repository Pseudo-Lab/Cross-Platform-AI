#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cmath>
#include <limits>
#include <fstream>
#include <optional>
#include <utility>
#include <vector>

class ComputeApplication
{
public:
    bool run()
    {
        if (!initVulkan() || !recordCommandBuffer())
        {
            return false;
        }
        for (std::uint32_t round = 0; round < 2; ++round)
        {
            if (round != 0)
            {
                device.resetFences(*fence); // C API: vkResetFences
            }
            if (!writeInputs(round) || !submitAndWait() || !verifyOutput(round))
            {
                return false;
            }
        }
        return true;
    }

private:
    static constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";

    static_assert(sizeof(float) == 4);
    static constexpr std::uint32_t elementCount = 8;
    static constexpr std::uint32_t workgroupSize = 64;
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
    // TODO: uploadA, uploadB, readback 멤버를 추가합니다.
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    vk::raii::ShaderModule shaderModule = nullptr;
    vk::raii::PipelineLayout pipelineLayout = nullptr;
    vk::raii::Pipeline computePipeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    vk::raii::Fence fence = nullptr;

    bool initVulkan()
    {
        return createInstance()
            && setupDebugMessenger()
            && pickPhysicalDevice()
            && createLogicalDevice()
            && createStorageBuffers()
            && createDescriptorSetLayout()
            && createDescriptorPool()
            && createDescriptorSets()
            && createShaderModule()
            && createPipelineLayout()
            && createComputePipeline()
            && createCommandPool()
            && allocateCommandBuffer()
            && createFence();
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
        for (const auto& extension : extensions)
        {
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
        appInfo.pApplicationName = "12-staging-buffers";
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
        return true;
    }

    // TODO: 역할별 usage와 memory properties로 버퍼를 생성합니다.
    bool createStorageBuffers()
    {
        const std::array buffers = {&inputA, &inputB, &output};
        for (auto* buffer : buffers)
        {
            if (!createBuffer(bufferSize, *buffer))
            {
                return false;
            }
        }
        return true;
    }

    // TODO: usage, properties 인자를 추가하고 findMemoryType() 실패 메시지를 수정합니다.
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

    bool writeBuffer(BufferResource& resource, const std::array<float, elementCount>& values)
    {
        void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
        std::memcpy(mapped, values.data(), static_cast<std::size_t>(bufferSize));
        resource.memory.unmapMemory(); // C API: vkUnmapMemory
        return true;
    }
    bool createDescriptorSetLayout()
    {
        std::array<vk::DescriptorSetLayoutBinding, 3> bindings{};
        for (std::uint32_t i = 0; i < bindings.size(); ++i)
        {
            bindings[i].binding = i;
            bindings[i].descriptorType = vk::DescriptorType::eStorageBuffer;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = vk::ShaderStageFlagBits::eCompute;
        }

        vk::DescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo); // C API: vkCreateDescriptorSetLayout
        return true;
    }
    bool createDescriptorPool()
    {
        vk::DescriptorPoolSize poolSize{};
        poolSize.type = vk::DescriptorType::eStorageBuffer;
        poolSize.descriptorCount = 3;
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        descriptorPool = vk::raii::DescriptorPool(device, poolInfo); // C API: vkCreateDescriptorPool
        return true;
    }

    bool createDescriptorSets()
    {
        const vk::DescriptorSetLayout layout = *descriptorSetLayout;
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo.descriptorPool = *descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;
        descriptorSets = device.allocateDescriptorSets(allocInfo); // C API: vkAllocateDescriptorSets

        std::array<vk::DescriptorBufferInfo, 3> bufferInfos{};
        bufferInfos[0].buffer = *inputA.buffer;
        bufferInfos[1].buffer = *inputB.buffer;
        bufferInfos[2].buffer = *output.buffer;
        for (auto& bufferInfo : bufferInfos)
        {
            bufferInfo.offset = 0;
            bufferInfo.range = bufferSize;
        }
        std::array<vk::WriteDescriptorSet, 3> writes{};
        for (std::uint32_t i = 0; i < writes.size(); ++i)
        {
            writes[i].dstSet = *descriptorSets[0];
            writes[i].dstBinding = i;
            writes[i].descriptorType = vk::DescriptorType::eStorageBuffer;
            writes[i].descriptorCount = 1;
            writes[i].pBufferInfo = &bufferInfos[i];
        }
        device.updateDescriptorSets(writes, {}); // C API: vkUpdateDescriptorSets
        return true;
    }
    bool createShaderModule()
    {
        std::ifstream file(ADD_SHADER_PATH, std::ios::ate | std::ios::binary);
        if (!file)
        {
            std::cerr << "Cannot open shader: " << ADD_SHADER_PATH << '\n';
            return false;
        }
        const std::streamsize byteCount = file.tellg();
        if (byteCount <= 0 || byteCount % sizeof(std::uint32_t) != 0)
        {
            std::cerr << "Invalid SPIR-V byte count.\n";
            return false;
        }
        std::vector<std::uint32_t> code(static_cast<std::size_t>(byteCount) / sizeof(std::uint32_t));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(code.data()), byteCount))
        {
            std::cerr << "Cannot read SPIR-V file.\n";
            return false;
        }

        vk::ShaderModuleCreateInfo moduleInfo{};
        moduleInfo.codeSize = static_cast<std::size_t>(byteCount);
        moduleInfo.pCode = code.data();
        shaderModule = vk::raii::ShaderModule(device, moduleInfo); // C API: vkCreateShaderModule
        std::cout << "Shader module created: " << moduleInfo.codeSize << " bytes.\n";
        return true;
    }
    bool createPipelineLayout()
    {
        vk::PushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(std::uint32_t);

        const vk::DescriptorSetLayout layout = *descriptorSetLayout;
        vk::PipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &layout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        pipelineLayout = vk::raii::PipelineLayout(device, layoutInfo); // C API: vkCreatePipelineLayout
        return true;
    }

    bool createComputePipeline()
    {
        vk::PipelineShaderStageCreateInfo stageInfo{};
        stageInfo.stage = vk::ShaderStageFlagBits::eCompute;
        stageInfo.module = *shaderModule;
        stageInfo.pName = "main";

        vk::ComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = *pipelineLayout;
        computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo); // C API: vkCreateComputePipelines
        std::cout << "Compute pipeline created.\n";
        return true;
    }
    bool createCommandPool()
    {
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.queueFamilyIndex = computeQueueFamilyIndex;
        commandPool = vk::raii::CommandPool(device, poolInfo); // C API: vkCreateCommandPool
        return true;
    }

    bool allocateCommandBuffer()
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = *commandPool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;
        commandBuffers = device.allocateCommandBuffers(allocInfo); // C API: vkAllocateCommandBuffers
        return true;
    }

    bool recordCommandBuffer()
    {
        const std::uint32_t groupCount = (elementCount + workgroupSize - 1) / workgroupSize;
        const auto limits = physicalDevice.getProperties().limits; // C API: vkGetPhysicalDeviceProperties
        if (groupCount > limits.maxComputeWorkGroupCount[0])
        {
            std::cerr << "Input exceeds device limits.\n";
            return false;
        }

        auto& cmd = commandBuffers[0];
        vk::CommandBufferBeginInfo beginInfo{};
        cmd.begin(beginInfo); // C API: vkBeginCommandBuffer
        // TODO: 입력 복사와 transfer write → compute read 배리어를 기록합니다.
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline); // C API: vkCmdBindPipeline
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout,
                               0, *descriptorSets[0], {}); // C API: vkCmdBindDescriptorSets
        const std::uint32_t count = elementCount;
        cmd.pushConstants<std::uint32_t>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute,
                                        0, count); // C API: vkCmdPushConstants
        cmd.dispatch(groupCount, 1, 1); // C API: vkCmdDispatch

        // TODO: 이 배리어를 출력 복사와 두 단계 배리어로 교체합니다.
        vk::BufferMemoryBarrier hostBarrier{};
        hostBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        hostBarrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
        hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        hostBarrier.buffer = *output.buffer;
        hostBarrier.offset = 0;
        hostBarrier.size = bufferSize;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                            vk::PipelineStageFlagBits::eHost, {}, {}, hostBarrier, {}); // C API: vkCmdPipelineBarrier
        cmd.end(); // C API: vkEndCommandBuffer
        std::cout << "Command buffer recorded: " << groupCount << " workgroups.\n";
        return true;
    }
    bool createFence()
    {
        fence = vk::raii::Fence(device, vk::FenceCreateInfo{}); // C API: vkCreateFence
        return true;
    }

    // TODO: CPU 쓰기 대상을 uploadA, uploadB, readback으로 바꿉니다.
    bool writeInputs(std::uint32_t round)
    {
        std::array<float, elementCount> valuesA{};
        std::array<float, elementCount> valuesB{};
        std::array<float, elementCount> initialOutput{};
        initialOutput.fill(std::numeric_limits<float>::quiet_NaN());
        for (std::uint32_t i = 0; i < elementCount; ++i)
        {
            valuesA[i] = static_cast<float>(i + round);
            valuesB[i] = 10.0f + static_cast<float>(i + round);
        }
        return writeBuffer(inputA, valuesA)
            && writeBuffer(inputB, valuesB)
            && writeBuffer(output, initialOutput);
    }

    bool submitAndWait()
    {
        const vk::CommandBuffer commandBuffer = *commandBuffers[0];
        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        computeQueue.submit(submitInfo, *fence); // C API: vkQueueSubmit
        const auto result = device.waitForFences(*fence, VK_TRUE, UINT64_MAX); // C API: vkWaitForFences
        if (result != vk::Result::eSuccess)
        {
            device.waitIdle(); // C API: vkDeviceWaitIdle
            std::cerr << "Fence wait did not complete successfully.\n";
            return false;
        }
        return true;
    }

    // TODO: output 대신 readback 메모리를 읽습니다.
    bool verifyOutput(std::uint32_t round)
    {
        std::array<float, elementCount> results{};
        void* mapped = output.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
        std::memcpy(results.data(), mapped, static_cast<std::size_t>(bufferSize));
        output.memory.unmapMemory(); // C API: vkUnmapMemory
        for (std::uint32_t i = 0; i < elementCount; ++i)
        {
            const float a = static_cast<float>(i + round);
            const float b = 10.0f + static_cast<float>(i + round);
            const float expected = a + b;
            const float tolerance = 1e-5f * (1.0f + std::abs(expected));
            if (!std::isfinite(results[i]) || std::abs(results[i] - expected) > tolerance)
            {
                std::cerr << "Mismatch at " << i << ": expected " << expected
                          << ", got " << results[i] << '\n';
                return false;
            }
        }
        std::cout << "Round " << round + 1 << ": verified " << elementCount
                  << " elements; first=" << results.front() << ", last=" << results.back() << '\n';
        return true;
    }
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

    std::cout << "Resources destroyed.\n";
    return 0;
}
