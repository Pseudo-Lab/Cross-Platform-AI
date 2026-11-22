#include "eva-runtime.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>

using namespace eva;

class ComputeApplication
{
public:
    bool run()
    {
        createBuffers();
        createPipeline();
        if (!recordCommandBuffer())
        {
            return false;
        }

        auto fence = device.createFence();
        auto queue = device.queue(QueueType::queue_compute);
        for (std::uint32_t round = 0; round < 2; ++round)
        {
            writeInputs(round);
            queue << commandBuffer << fence;
            if (fence.wait(true) != Result::SUCCESS)
            {
                queue.waitIdle();
                std::cerr << "Fence wait did not complete successfully.\n";
                return false;
            }
            if (!verifyOutput(round))
            {
                return false;
            }
        }
        return true;
    }

private:
    static_assert(sizeof(float) == 4);
    static constexpr std::uint32_t elementCount = 8;
    static constexpr std::uint32_t workgroupSize = 64;
    static constexpr std::uint64_t bufferSize = sizeof(float) * elementCount;
    using Values = std::array<float, elementCount>;

    Device device = Runtime::get().device({.enableComputeQueues = true});
    Buffer inputA, inputB, intermediate, output;
    Buffer uploadA, uploadB, readback;
    ComputePipeline pipeline;
    DescriptorSet firstSet, secondSet;
    CommandBuffer commandBuffer;

    void createBuffers()
    {
        const auto inputUsage = BUFFER_USAGE::STORAGE_BUFFER | BUFFER_USAGE::TRANSFER_DST;
        const auto outputUsage = BUFFER_USAGE::STORAGE_BUFFER | BUFFER_USAGE::TRANSFER_SRC;
        const auto hostMemory = MEMORY_PROPERTY::HOST_VISIBLE | MEMORY_PROPERTY::HOST_COHERENT;

        inputA = device.createBuffer({bufferSize, inputUsage, MEMORY_PROPERTY::DEVICE_LOCAL});
        inputB = device.createBuffer({bufferSize, inputUsage, MEMORY_PROPERTY::DEVICE_LOCAL});
        intermediate = device.createBuffer({bufferSize, BUFFER_USAGE::STORAGE_BUFFER,
                                            MEMORY_PROPERTY::DEVICE_LOCAL});
        output = device.createBuffer({bufferSize, outputUsage, MEMORY_PROPERTY::DEVICE_LOCAL});
        uploadA = device.createBuffer({bufferSize, BUFFER_USAGE::TRANSFER_SRC, hostMemory});
        uploadB = device.createBuffer({bufferSize, BUFFER_USAGE::TRANSFER_SRC, hostMemory});
        readback = device.createBuffer({bufferSize, BUFFER_USAGE::TRANSFER_DST, hostMemory});
    }

    void createPipeline()
    {
        pipeline = device.createComputePipeline({.csStage = SpvBlob::readFrom(ADD_SHADER_PATH)});
        auto pool = device.createDescriptorPool({
            .maxTypes = {{DESCRIPTOR_TYPE::STORAGE_BUFFER, 6}},
            .maxSets = 2,
        });
        firstSet = pool(pipeline.descSetLayout());
        secondSet = pool(pipeline.descSetLayout());
        firstSet.write({inputA, inputB, intermediate});
        secondSet.write({intermediate, inputB, output});
    }

    bool recordCommandBuffer()
    {
        const std::uint32_t groupCount = (elementCount + workgroupSize - 1) / workgroupSize;
        if (groupCount > device.maxComputeWorkGroupCount()[0])
        {
            std::cerr << "Input exceeds device limits.\n";
            return false;
        }

        commandBuffer = device.newCommandBuffer(QueueType::queue_compute);
        commandBuffer.begin()
            .copyBuffer(uploadA, inputA)
            .copyBuffer(uploadB, inputB)
            .barrier({
                SYNC_SCOPE::TRANSFER_DST / inputA / SYNC_SCOPE::COMPUTE_READ,
                SYNC_SCOPE::TRANSFER_DST / inputB / SYNC_SCOPE::COMPUTE_READ,
            })
            .bindPipeline(pipeline)
            .setPushConstants(0, sizeof(elementCount), &elementCount)
            .bindDescSets({firstSet})
            .dispatch(groupCount)
            .barrier(SYNC_SCOPE::COMPUTE_WRITE / intermediate / SYNC_SCOPE::COMPUTE_READ)
            .bindDescSets({secondSet})
            .dispatch(groupCount)
            .barrier(SYNC_SCOPE::COMPUTE_WRITE / output / SYNC_SCOPE::TRANSFER_SRC)
            .copyBuffer(output, readback)
            .barrier(SYNC_SCOPE::TRANSFER_DST / readback / (PIPELINE_STAGE::HOST, ACCESS::HOST_READ))
            .end();

        std::cout << "Command buffer recorded: " << groupCount
                  << " workgroups per dispatch, 2 dispatches.\n";
        return true;
    }

    static void writeBuffer(Buffer buffer, const Values& values)
    {
        std::memcpy(buffer.map(), values.data(), bufferSize);
        buffer.unmap();
    }

    void writeInputs(std::uint32_t round)
    {
        Values valuesA{}, valuesB{}, initialOutput{};
        for (std::uint32_t i = 0; i < elementCount; ++i)
        {
            valuesA[i] = static_cast<float>(i + round);
            valuesB[i] = 10.0f + static_cast<float>(i + round);
        }
        initialOutput.fill(std::numeric_limits<float>::quiet_NaN());
        writeBuffer(uploadA, valuesA);
        writeBuffer(uploadB, valuesB);
        writeBuffer(readback, initialOutput);
    }

    bool verifyOutput(std::uint32_t round)
    {
        Values results{};
        std::memcpy(results.data(), readback.map(), bufferSize);
        readback.unmap();
        for (std::uint32_t i = 0; i < elementCount; ++i)
        {
            const float a = static_cast<float>(i + round);
            const float b = 10.0f + static_cast<float>(i + round);
            const float expected = (a + b) + b;
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
    if (!std::filesystem::is_regular_file(ADD_SHADER_PATH))
    {
        std::cerr << "Cannot open shader: " << ADD_SHADER_PATH << '\n';
        return 1;
    }
    ComputeApplication app;
    return app.run() ? 0 : 1;
}
