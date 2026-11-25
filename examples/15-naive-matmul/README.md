# 15. Naive matmul 실습

[이전 장](../14-eva-multiple-dispatches/README.md) | [전체 튜토리얼](../../README.md#튜토리얼)

13장에서는 첫 디스패치가 쓴 중간 버퍼를 두 번째 디스패치가 읽도록 연결했습니다. 14장에서는 같은 작업을 eva의 커맨드 버퍼와 배리어로 작성했습니다. 이번에는 vAi에서 행렬곱과 ReLU를 같은 방식으로 이어 보겠습니다.

## Vulkan 연산을 vAi 그래프로 연결하기

vAi에서는 모델을 만드는 사람이 연산 순서를 정하고, 노드를 만드는 사람이 각 연산의 GPU 명령을 작성합니다. 예를 들어 vAi의 `Activation`은 입력과 출력 슬롯을 가진 노드입니다. 이 노드를 `relu`라는 멤버로 두면 모델 생성자에서 `input("in0") - relu - output("out0");` 한 줄로 입력에서 ReLU를 거쳐 출력으로 가는 경로를 만듭니다. `Activation`은 이 경로를 실행할 때 Vulkan 래퍼인 eva로 파이프라인을 바인딩하고 디스패치를 기록합니다. [vAi 모델 작성법](https://github.com/vaistrata/vAi/blob/cf5e8658496dac426e3803dd963e8a8355f52aa1/docs/en/contributing/models.md), [Activation 구현](https://github.com/vaistrata/vAi/blob/cf5e8658496dac426e3803dd963e8a8355f52aa1/vai/nodes/activation.h)

행렬곱은 직접 노드를 만들어 보겠습니다. 모델의 입력은 행렬 `X`이고, 행렬곱에 쓸 가중치 `W`는 그 노드에 보관합니다. 행렬곱 결과 `Z`는 GPU에서 곧바로 ReLU의 입력이 됩니다.

```text
                        W [K,N]
                           ↓
입력 X [M,K] ──────→ NaiveMatmul ── Z [M,N] ──→ Activation(ReLU) ──→ 출력 Y [M,N]
```

| 코드 | 이 그래프에서 맡는 일 |
|---|---|
| `Net` (`vai::NeuralNet` 상속) | 입력, 행렬곱, ReLU, 출력을 연결합니다. 입력 텐서와 출력을 받을 위치를 넘겨 Net을 호출하면 엔진이 연결 순서에 맞춰 GPU 명령을 기록하고 제출합니다. |
| `vai::Tensor` | 행렬의 차원 크기(shape), 원소 타입(dtype), 데이터를 함께 다룹니다. 연결된 슬롯은 같은 텐서의 GPU 버퍼를 참조하므로 중간 행렬을 CPU로 복사할 필요가 없습니다. |
| `NaiveMatmul` (`vai::Node` 상속) | `prepare()`에서 입력을 보고 출력의 크기를 정합니다. 엔진이 출력 버퍼를 연결하면 `run(cmd)`에서 eva로 파이프라인과 버퍼를 바인딩하고 디스패치와 배리어를 기록합니다. |
| GLSL 컴퓨트 셰이더 | 바인딩된 버퍼에서 값을 읽고 각 스레드가 맡은 출력 원소를 계산합니다. |

12장에서 직접 작성한 입력 업로드는 Net을 호출할 때 엔진이 처리합니다. 마지막에는 출력 텐서의 `getData<float>()`로 결과를 CPU에서 읽습니다. 우리가 직접 작성할 GPU 명령은 두 과정 사이에서 행렬곱을 수행하고 그 결과를 ReLU에 넘기는 부분입니다. [vAi 실행 과정](https://github.com/vaistrata/vAi/blob/cf5e8658496dac426e3803dd963e8a8355f52aa1/docs/en/03-usage/execution-pipeline.md), [커스텀 노드 작성법](https://github.com/vaistrata/vAi/blob/cf5e8658496dac426e3803dd963e8a8355f52aa1/docs/en/contributing/nodes.md)

아래 세 파일을 순서대로 작성하겠습니다.

1. [shaders/naive-matmul.comp](shaders/naive-matmul.comp)에 행렬곱 커널을 작성합니다.
2. [naive-matmul.h](naive-matmul.h)의 `NaiveMatmul`에 커널을 실행할 멤버와 함수를 채웁니다.
3. [main.cpp](main.cpp)의 `Net`에서 두 노드를 연결하고 `main()`에서 입력 한 쌍으로 실행합니다.

## 커널에서 행과 열 곱하기

`shaders/naive-matmul.comp`의 힌트 아래에 GLSL 버전과 워크그룹 크기를 선언합니다. 출력은 2차원 행렬이고, 각 원소는 다른 출력 원소를 참조하지 않습니다. 한 스레드에 원소 하나를 맡기고, 가로 8개와 세로 8개의 스레드를 한 워크그룹으로 묶겠습니다.

```glsl
#version 450

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
```

그 아래에 출력 `Z`, 입력 `X`, 가중치 `W`를 담을 스토리지 버퍼를 선언합니다. 세 버퍼 모두 FP32 값을 행 우선(row-major)으로 저장합니다. `set = 0` 안에서 커널은 0번 바인딩에 쓰고, 1번과 2번에서 읽습니다. 잠시 뒤 C++에서 이 순서로 버퍼를 연결합니다.

```glsl
layout(set = 0, binding = 0, std430) writeonly buffer Output { float values[]; } Z;
layout(set = 0, binding = 1, std430) readonly buffer Input   { float values[]; } X;
layout(set = 0, binding = 2, std430) readonly buffer Weight  { float values[]; } W;
```

셰이더에는 연속된 값만 보이므로 한 행이 어디서 끝나는지 알려 줘야 합니다. 버퍼 선언 아래에 푸시 상수를 추가합니다. `M`은 출력의 행 수, `N`은 출력의 열 수, `K`는 원소 하나를 계산할 때 곱해서 더할 항의 수입니다. C++에서 이 순서대로 값을 보냅니다.

```glsl
layout(push_constant) uniform Parameters
{
    uint M;
    uint N;
    uint K;
} p;
```

이제 커널의 `main()`을 작성합니다. x축은 열, y축은 행에 대응시킵니다. 출력이 2×2여도 워크그룹 하나를 실행하면 스레드 64개가 시작됩니다. 행과 열이 모두 출력 범위 안에 있는 네 스레드만 버퍼에 접근합니다.

```glsl
void main()
{
    uint col = gl_GlobalInvocationID.x;
    uint row = gl_GlobalInvocationID.y;
    if (row >= p.M || col >= p.N)
        return;
}
```

같은 함수의 경계 검사 다음, 닫는 중괄호 앞에 내적을 추가합니다. `X`의 한 행은 `K`개씩, `W`의 한 행은 `N`개씩 저장되어 있습니다. 따라서 `k`를 하나 늘리면 `X`에서는 오른쪽으로 한 칸, `W`에서는 같은 열의 다음 행으로 이동합니다.

```glsl
float sum = 0.0;
for (uint k = 0; k < p.K; ++k)
    sum += X.values[row * p.K + k] * W.values[k * p.N + col];

Z.values[row * p.N + col] = sum;
```

이렇게 하면 가중치를 전치하거나 bias를 더하지 않고 `Z = X @ W`를 얻습니다. 음수도 그대로 기록해야 다음 노드에서 ReLU를 적용할 수 있습니다.

## 노드에 실행 상태와 슬롯 추가하기

`naive-matmul.h`를 엽니다. 시작 코드가 포함한 `vai/nodes/common.h`에는 `Node`와 `Tensor`, 그래프 연결 연산자가 들어 있습니다. `NaiveMatmul`의 `public:` 바로 위에 파이프라인과 디스크립터 셋 핸들을 선언합니다. 이어서 푸시 상수를 담을 멤버를 추가합니다. GLSL의 `uint` 하나는 4바이트이므로 `Parameters`도 같은 순서의 12바이트여야 합니다.

```cpp
eva::ComputePipeline pipe;
eva::DescriptorSet descSet;

struct Parameters
{
    uint32_t M;
    uint32_t N;
    uint32_t K;
} params{};
static_assert(sizeof(Parameters) == 12);
```

생성자 `NaiveMatmul()`의 본문에서 텐서가 들어갈 자리를 등록합니다. `in0`는 앞 노드에서 받을 입력이고, `out0`는 다음 노드에 넘길 출력입니다. `weight`는 모델에 한 번 설정해 두고 쓰는 가중치이므로 엔진이 실행 사이에도 보관하는 `persistent` 슬롯에 둡니다.

```cpp
addSlot("in0", vai::NodeSlot::input);
addSlot("weight", vai::NodeSlot::persistent);
addSlot("out0", vai::NodeSlot::output);
```

## 입력에서 출력의 크기 정하기

`prepare()`의 첫 줄부터 두 슬롯의 텐서를 가져옵니다. 이 커널은 각 축의 크기가 양수인 2차원 FP32 행렬을 받습니다. 행렬곱을 하려면 `X`의 열 수와 `W`의 행 수도 같아야 합니다.

```cpp
const auto& input = (*this)["in0"];
const auto& weight = (*this)["weight"];
if (!input.validShape() || !weight.validShape()
    || input.shape().size() != 2 || weight.shape().size() != 2
    || input.dtype() != vai::Dtype::Fp32 || weight.dtype() != vai::Dtype::Fp32)
    throw std::runtime_error("NaiveMatmul requires two nonempty FP32 matrices.");
if (input.shape()[1] != weight.shape()[0])
    throw std::runtime_error("Input columns must match weight rows.");
```

그 아래에서 실제 크기를 읽습니다. 입력 `[2,3]`에서 `input.shape()[0]`은 행 수 2이므로 `params.M`에, `input.shape()[1]`은 내적의 길이 3이므로 `params.K`에 넣습니다. 가중치 `[3,2]`의 열 수 2는 `weight.shape()[1]`에서 읽어 `params.N`에 넣습니다. 그러면 이 입력의 푸시 상수는 메모리에 `M=2, N=2, K=3` 순서로 놓입니다.

```cpp
params.M = input.shape()[0];
params.N = weight.shape()[1];
params.K = input.shape()[1];
(*this)["out0"] = vai::Tensor({params.M, params.N}, vai::Dtype::Fp32);
```

마지막 줄은 출력 슬롯에 `[M,N]` 모양의 FP32 텐서를 설정합니다. 엔진은 `prepare()`가 끝난 뒤 이 정보로 GPU 버퍼를 연결하고, 같은 텐서를 다음 노드의 입력 슬롯에 전달합니다.

## 파이프라인을 준비하고 행렬곱 기록하기

`prepare()` 끝에 다음 블록을 추가합니다. `MATMUL_SHADER_PATH`는 CMake가 컴파일한 SPIR-V 파일의 경로입니다. eva는 SPIR-V에서 바인딩과 푸시 상수 선언을 읽어 파이프라인 레이아웃을 구성합니다. `pipe.descSetLayout(0)`은 그중 set 0의 레이아웃이며, 이 레이아웃으로 vAi의 디스크립터 풀에서 셋 하나를 할당합니다.

```cpp
if (!pipe)
{
    auto spv = eva::SpvBlob::readFrom(MATMUL_SHADER_PATH);
    pipe = device().createComputePipeline({.csStage = spv});
    descSet = vai::gVaiDestSetPool(pipe.descSetLayout(0));
}
```

`!pipe`는 eva 핸들의 `operator bool()`을 통해 파이프라인이 비어 있는지 검사합니다. 행렬의 크기가 바뀌어도 셰이더와 버퍼 레이아웃은 같으므로, 명령을 다시 기록할 때도 이 파이프라인과 셋을 쓸 수 있습니다.

이제 `run(eva::CommandBuffer cmd)`의 본문을 작성합니다. 각 텐서의 `buffer()`로 GPU 버퍼 범위를 가져와 GLSL의 바인딩 순서대로 셋에 씁니다. 슬롯을 등록한 순서와 관계없이 여기서 `write()`에 넘긴 첫 항목이 0번 출력, 다음이 1번 입력, 마지막이 2번 가중치입니다. 파이프라인과 셋을 커맨드 버퍼에 바인딩합니다.

```cpp
auto outputBuffer = (*this)["out0"].buffer();
auto inputBuffer = (*this)["in0"].buffer();
auto weightBuffer = (*this)["weight"].buffer();
descSet.write({outputBuffer, inputBuffer, weightBuffer});
cmd.bindPipeline(pipe).bindDescSets({descSet});
```

바로 아래에서 푸시 상수 영역의 오프셋 0부터 `params`의 12바이트를 기록합니다. 셰이더에서는 이 값을 `p.M`, `p.N`, `p.K`로 읽습니다. 출력 한 칸에 스레드 하나를 배정했으므로 가로로 `N`개, 세로로 `M`개의 스레드가 필요합니다.

```cpp
cmd.setPushConstants(0, sizeof(params), &params)
   .dispatch2(params.N, params.M);
```

`dispatch(x, y)`의 인자는 워크그룹 수입니다. `dispatch2(x, y)`의 인자는 필요한 스레드 수이며, eva가 각 축을 바인딩된 파이프라인의 워크그룹 크기로 나누고 올림합니다. 이 예에서 `dispatch2(2, 2)`는 8×8 워크그룹 하나를 실행합니다. 앞서 작성한 경계 검사 덕분에 나머지 60개 스레드는 행렬 밖에 쓰지 않습니다.

`run()` 마지막에 출력 버퍼의 메모리 배리어를 기록합니다. 행렬곱 셰이더가 쓴 `Z`를 다음 디스패치의 ReLU 셰이더가 읽습니다. 13장의 중간 버퍼와 같은 쓰기 → 읽기 의존성입니다.

```cpp
cmd.barrier(eva::SYNC_SCOPE::COMPUTE_WRITE / outputBuffer /
            eva::SYNC_SCOPE::COMPUTE_READ);
```

## Net에서 두 노드 연결하기

`main.cpp`의 빈 `Net` 구조체 안에 노드 두 개를 멤버로 선언합니다. 방금 작성한 `NaiveMatmul`과 vAi의 `Activation`을 사용하고, 활성화 함수로 ReLU를 선택합니다.

```cpp
NaiveMatmul matmul;
vai::node::Activation relu{{.actType = vai::ActType::Relu}};
```

두 멤버 아래에 생성자를 추가합니다. `-` 연산자는 왼쪽 노드의 `out0` 슬롯을 오른쪽 노드의 `in0` 슬롯에 연결합니다. 이 한 줄로 외부 입력에서 시작해 행렬곱, ReLU를 거쳐 외부 출력에 이르는 경로가 만들어집니다.

```cpp
explicit Net(const vai::NeuralNetConfig& config)
    : vai::NeuralNet(config)
{
    input("in0") - matmul - relu - output("out0");
}
```

## 입력 한 쌍으로 실행하기

이제 같은 파일의 `main()` 안에서 Net을 만듭니다. 가중치와 노드 출력의 기본 타입을 FP32로 지정하면 ReLU의 출력도 FP32입니다. `setFloats()`에는 각 행의 값을 왼쪽에서 오른쪽으로, 첫 행부터 차례로 넣습니다.

```cpp
Net net({.wDefaultDtype = vai::Dtype::Fp32, .oDefaultDtype = vai::Dtype::Fp32});

vai::Tensor input({2, 3}, vai::Dtype::Fp32);
input.setFloats({1, 2, 3,
                -1, 0, 2});
```

그 아래에 가중치를 작성해 행렬곱 노드의 `weight` 슬롯에 넣습니다. 첫 실행 때 엔진이 이 슬롯의 값을 GPU로 올립니다.

```cpp
vai::Tensor weight({3, 2}, vai::Dtype::Fp32);
weight.setFloats({ 1, -1,
                   2,  0,
                  -1,  1});
net.matmul["weight"] = weight;
```

`main()` 끝에서 Net을 한 번 호출합니다. 첫 인자는 외부 입력 이름과 텐서, 둘째 인자는 외부 출력 이름과 결과를 받을 텐서의 주소를 짝지은 것입니다. `output.getData<float>()`는 GPU에서 최종 결과를 가져와 CPU가 읽을 수 있는 포인터를 돌려줍니다. 2×2 출력의 첫 두 값이 첫 행입니다.

```cpp
vai::Tensor output;
net({{"in0", input}}, {{"out0", &output}});

const float* y = output.getData<float>();
std::cout << "ReLU(X @ W)\n"
          << y[0] << ' ' << y[1] << '\n'
          << y[2] << ' ' << y[3] << '\n';
```

## 빌드와 실행

Vulkan 1.3을 지원하는 장치, `glslc`가 포함된 Vulkan SDK 1.3 이상, CMake 3.24 이상과 C++23 컴파일러를 준비합니다. Linux에서 GCC를 쓸 때는 vAi가 요구하는 14 이상을 사용합니다. vAi를 클론할 때 하위 의존성인 eva도 함께 내려받겠습니다. 아래의 `/path/to/vAi`는 vAi를 둘 실제 절대 경로로 바꿉니다.

```sh
git clone --recurse-submodules https://github.com/vaistrata/vAi.git /path/to/vAi
```

`examples/15-naive-matmul` 폴더에서 구성하고 빌드합니다. `VAI_SOURCE_DIR`로 지정한 vAi와 그 안의 `external/eva`를 사용하며, 실습 커널은 빌드 중에 SPIR-V로 컴파일됩니다.

```sh
cmake -S . -B build -DVAI_SOURCE_DIR=/path/to/vAi -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target naive_matmul_example
```

Linux에서 Make 또는 Ninja로 빌드했다면 다음과 같이 실행합니다. Windows에서 Visual Studio 생성기를 사용했다면 PowerShell에서 `.\build\bin\Release\naive_matmul_example.exe`를 실행합니다.

```sh
./build/bin/naive_matmul_example
```

이 입력으로 실행하면 장치 초기화 로그 뒤에 다음 결과가 나와야 합니다.

```text
ReLU(X @ W)
2 2
0 3
```

행렬곱 직후의 두 행은 `[2, 2]`와 `[-3, 3]`입니다. 예를 들어 둘째 행의 첫 값은 `(-1)×1 + 0×2 + 2×(-1) = -3`입니다. 출력하는 행렬은 ReLU까지 거친 결과이므로 이 위치에 0이 표시됩니다.

완성본은 [커널](../99-completed/15-naive-matmul.comp), [노드](../99-completed/15-naive-matmul.h), [Net과 실행 코드](../99-completed/15-naive-matmul.cpp)에서 확인할 수 있습니다. 같은 빌드 폴더에서 아래 타깃을 선택하면 완성본의 커널과 C++ 소스를 함께 빌드하므로 실습 파일을 작성하는 도중에도 결과를 볼 수 있습니다.

```sh
cmake --build build --config Release --target naive_matmul_completed
```

Linux의 Make 또는 Ninja 빌드는 `./build/completed/naive_matmul_completed`, Visual Studio의 Release 빌드는 `.\build\completed\Release\naive_matmul_completed.exe`로 실행합니다.
