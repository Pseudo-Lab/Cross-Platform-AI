#include <iostream>
#include "15-naive-matmul.h"
#include "vai/nodes/activation.h"

struct Net : public vai::NeuralNet
{
    NaiveMatmul matmul;
    vai::node::Activation relu{{.actType = vai::ActType::Relu}};

    explicit Net(const vai::NeuralNetConfig& config)
        : vai::NeuralNet(config)
    {
        input("in0") - matmul - relu - output("out0");
    }
};

int main()
{
    Net net({.wDefaultDtype = vai::Dtype::Fp32, .oDefaultDtype = vai::Dtype::Fp32});

    vai::Tensor input({2, 3}, vai::Dtype::Fp32);
    input.setFloats({1, 2, 3,
                    -1, 0, 2});

    vai::Tensor weight({3, 2}, vai::Dtype::Fp32);
    weight.setFloats({ 1, -1,
                       2,  0,
                      -1,  1});
    net.matmul["weight"] = weight;

    vai::Tensor output;
    net({{"in0", input}}, {{"out0", &output}});

    const float* y = output.getData<float>();
    std::cout << "ReLU(X @ W)\n"
              << y[0] << ' ' << y[1] << '\n'
              << y[2] << ' ' << y[3] << '\n';
}
