#pragma once

#include <cstdint>
#include <stdexcept>
#include "vai/nodes/common.h"

class NaiveMatmul : public vai::Node
{
    eva::ComputePipeline pipe;
    eva::DescriptorSet descSet;

    struct Parameters
    {
        uint32_t M;
        uint32_t N;
        uint32_t K;
    } params{};
    static_assert(sizeof(Parameters) == 12);

public:
    NaiveMatmul()
    {
        addSlot("in0", vai::NodeSlot::input);
        addSlot("weight", vai::NodeSlot::persistent);
        addSlot("out0", vai::NodeSlot::output);
    }

    void prepare() override
    {
        const auto& input = (*this)["in0"];
        const auto& weight = (*this)["weight"];
        if (!input.validShape() || !weight.validShape()
            || input.shape().size() != 2 || weight.shape().size() != 2
            || input.dtype() != vai::Dtype::Fp32 || weight.dtype() != vai::Dtype::Fp32)
            throw std::runtime_error("NaiveMatmul requires two nonempty FP32 matrices.");
        if (input.shape()[1] != weight.shape()[0])
            throw std::runtime_error("Input columns must match weight rows.");

        params.M = input.shape()[0];
        params.N = weight.shape()[1];
        params.K = input.shape()[1];
        (*this)["out0"] = vai::Tensor({params.M, params.N}, vai::Dtype::Fp32);

        if (!pipe)
        {
            auto spv = eva::SpvBlob::readFrom(MATMUL_SHADER_PATH);
            pipe = device().createComputePipeline({.csStage = spv});
            descSet = vai::gVaiDestSetPool(pipe.descSetLayout(0));
        }
    }

    void run(eva::CommandBuffer cmd) override
    {
        auto outputBuffer = (*this)["out0"].buffer();
        auto inputBuffer = (*this)["in0"].buffer();
        auto weightBuffer = (*this)["weight"].buffer();
        descSet.write({outputBuffer, inputBuffer, weightBuffer});
        cmd.bindPipeline(pipe).bindDescSets({descSet});

        cmd.setPushConstants(0, sizeof(params), &params)
           .dispatch2(params.N, params.M);

        cmd.barrier(eva::SYNC_SCOPE::COMPUTE_WRITE / outputBuffer /
                    eva::SYNC_SCOPE::COMPUTE_READ);
    }
};
