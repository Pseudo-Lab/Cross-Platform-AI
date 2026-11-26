#pragma once

#include <cstdint>
#include <stdexcept>
#include "vai/nodes/common.h"

class NaiveMatmul : public vai::Node
{
public:
    NaiveMatmul()
    {
    }

    void prepare() override
    {
    }

    void run(eva::CommandBuffer cmd) override
    {
    }
};
