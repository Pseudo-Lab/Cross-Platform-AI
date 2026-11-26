# 15. Naive matmul 실습

[이전 장](../14-eva-multiple-dispatches/README.md) | [전체 튜토리얼](../../README.md#튜토리얼)

13장에서는 첫 디스패치가 쓴 중간 버퍼를 두 번째 디스패치가 읽도록 연결했습니다. 14장에서는 같은 작업을 eva의 커맨드 버퍼와 배리어로 작성했습니다. 이번에는 vAi에서 행렬곱과 ReLU를 같은 방식으로 이어 보겠습니다.

## Vulkan 연산을 vAi 그래프로 연결하기

vAi는 C++로 연산을 연결하고 Vulkan GPU에서 실행하는 추론 엔진입니다. 모델을 만드는 사람은 어떤 연산의 결과를 다음 연산에 전달할지 정합니다. vAi 엔진은 이 연결을 따라 버퍼를 준비하고 GPU 명령의 기록과 제출을 관리합니다. 각 연산의 `Node` 구현은 eva로 명령을 기록하고, 실제 숫자 계산은 GPU의 컴퓨트 셰이더가 수행합니다.

먼저 구성 요소의 역할을 구분하겠습니다. `Node`는 행렬곱, 활성화 함수, 형상 변경 등 하나의 연산을 표현합니다. `Tensor`는 연산에 쓰는 데이터의 형상과 자료형, 저장소를 다룹니다. `NodeGroup`은 여러 노드나 그룹을 묶어 재사용하는 부분 그래프이고, `NeuralNet`은 모델 전체의 입출력과 실행을 담당합니다. Node 하나가 여러 커널을 실행할 수도 있고, 버퍼를 공유하며 형상만 바꾸는 노드는 계산 커널이 필요하지 않을 수도 있습니다.

행렬곱은 직접 노드를 만들어 보겠습니다. 모델의 입력은 행렬 `X`이고, 행렬곱에 쓸 가중치 `W`는 그 노드에 보관합니다. 행렬곱 결과 `Z`는 GPU에서 곧바로 ReLU의 입력이 됩니다.

```text
                        W [K,N]
                           ↓
입력 X [M,K] ──────→ NaiveMatmul ── Z [M,N] ──→ Activation(ReLU) ──→ 출력 Y [M,N]
```

이 실습에서는 `NaiveMatmul` 노드를 직접 구현하고, ReLU에는 vAi가 제공하는 `Activation` 노드를 사용합니다. 두 노드를 `Net`에 연결하겠습니다. 여기서 가중치의 형상은 `[K,N]`입니다. 기본 제공 `Linear`가 사용하는 `[N,K]` 가중치와 방향이 다르므로, 이 실습에서는 위 그림과 셰이더의 인덱스를 기준으로 작성합니다.

12장에서 직접 작성한 입력 업로드는 Net을 호출할 때 엔진이 처리합니다. 마지막에는 출력 텐서의 `getData<float>()`로 결과를 CPU에서 읽습니다. 우리가 직접 작성할 GPU 명령은 두 과정 사이에서 행렬곱을 수행하고 그 결과를 ReLU에 넘기는 부분입니다.
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

이제 커널의 `main()`을 작성합니다. x축은 열, y축은 행에 대응시킵니다. 출력이 2×2여도 워크그룹 하나를 실행하면 스레드 64개가 시작됩니다. 행과 열이 모두 출력 범위 안에 있는 네 스레드만 버퍼에 접근하도록, 아래 주석을 보고 직접 구현합니다.

```glsl
void main()
{
    uint col = gl_GlobalInvocationID.x;
    uint row = gl_GlobalInvocationID.y;
    // TODO: row 또는 col이 출력 행렬의 범위를 벗어나면 버퍼에 접근하지 않고 종료합니다.
    // TODO: 누적값을 0으로 시작해 X의 row행과 W의 col열에서 K쌍의 원소를 곱해 더합니다.
    //       행 우선 저장이므로 X의 한 행은 K개, W와 Z의 한 행은 N개 원소입니다.
    // TODO: 누적값을 Z의 해당 행과 열에 저장합니다. 음수도 그대로 저장합니다.
}
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

생성자 `NaiveMatmul()`의 본문에서 Tensor를 담을 자리를 등록합니다. 이 자리를 슬롯(slot)이라고 합니다. 슬롯은 노드의 연결 위치이고, 그 안의 Tensor는 형상(shape), 자료형(dtype)과 실제 데이터 저장소에 대한 참조를 담습니다. 예를 들어 `[2,3]`은 행과 열의 크기이고, FP32는 원소의 자료형입니다. 여섯 개의 숫자 자체는 CPU 메모리나 GPU 버퍼에 저장합니다. Tensor의 형상을 정하는 것과 GPU 버퍼를 확보하는 것은 별도 단계입니다.

`in0`는 앞 노드에서 받을 입력이고, `out0`는 다음 노드에 넘길 출력입니다. `weight`는 실행 사이에도 보존할 가중치이므로 `persistent` 슬롯에 둡니다.

```cpp
addSlot("in0", vai::NodeSlot::input);
addSlot("weight", vai::NodeSlot::persistent);
addSlot("out0", vai::NodeSlot::output);
```

`addSlot("out0", ...)`는 이름과 역할을 등록할 뿐 출력 크기나 값을 정하지 않습니다. `in0`와 `out0`도 노드 구현자가 직접 등록해야 합니다. 나중에 `a - b`라고 연결하면 `a.out0`에서 `b.in0`으로 잇지만, 이름을 생략했다고 슬롯이 자동으로 생기지는 않습니다.

슬롯의 역할은 다음과 같습니다.

| 종류 | 누가 무엇을 넣는가 |
|---|---|
| `input` | 엔진이 외부 입력이나 앞 노드의 출력 Tensor를 전달합니다. |
| `output` | 노드 구현자가 `prepare()`에서 결과의 형상과 자료형을 정하고, GPU가 기록된 명령에 따라 결과 값을 씁니다. |
| `persistent` | 호출 코드나 가중치 로더가 가중치를 넣거나, 노드가 실행 사이에 유지할 상태를 준비합니다. KV 캐시처럼 갱신되는 값도 여기에 둡니다. 보존한다는 뜻이지 불변이라는 뜻은 아닙니다. |
| `temporary` | 노드 구현자가 내부 계산에 필요한 작업 공간을 준비합니다. 다른 노드에 결과를 전달하는 슬롯은 아닙니다. |

예를 들어 한 노드의 커널 A가 부분합을 temporary 버퍼에 쓰고, 배리어 뒤에 커널 B가 이를 읽어 최종 결과를 만들 수 있습니다. 두 커널을 하나로 합치는 커널 퓨전과는 별개입니다. CPU의 `run()` 호출이 끝났다고 이 GPU 메모리가 즉시 사라지는 것은 아닙니다. 엔진은 명령 기록 후 임시 슬롯의 Tensor 참조를 정리하고 버퍼 풀에서 저장 공간을 재사용합니다. 이 실습의 `NaiveMatmul`은 커널 하나로 결과를 쓰므로 temporary 슬롯을 사용하지 않습니다.

## 입력에서 출력의 크기 정하기

이제 노드 구현자인 우리가 `prepare()`를 작성합니다. 이 함수는 입력과 가중치의 형상을 검사하고, 출력과 필요한 임시 Tensor의 형상 및 자료형을 정합니다. 엔진이 행렬곱 공식을 알아서 추측하는 것이 아니라, 우리가 작성한 함수를 명령 기록 과정에서 호출합니다.

함수를 호출하기 전에 입력 슬롯부터 채워져 있어야 합니다. 뒤에서 작성할 `main()`이 입력 Tensor를 `net(...)`에 전달하면 엔진이 이를 입력 경계 노드에서 `matmul["in0"]`으로 전달합니다. 가중치는 호출 전에 `net.matmul["weight"] = weight;`로 직접 설정합니다. ReLU 같은 중간 노드의 `in0`에는 엔진이 앞 노드의 `out0`에 설정된 Tensor를 전달합니다.

`prepare()`의 첫 줄부터 두 슬롯의 Tensor를 가져옵니다. `(*this)["in0"]`는 이 노드의 `in0` 슬롯에 들어 있는 Tensor를 읽는 표현입니다. 이 커널은 각 축의 크기가 양수인 2차원 FP32 행렬을 받습니다. 행렬곱을 하려면 `X`의 열 수와 `W`의 행 수도 같아야 합니다.

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

마지막 줄은 출력 슬롯에 `[M,N]` 모양의 FP32 Tensor를 설정합니다. 결과 숫자를 넣거나 GPU 버퍼를 할당하는 줄은 아닙니다. 노드를 사용하는 사람은 입력과 가중치를 제공하고, 노드를 구현하는 우리가 그 형상에서 출력 크기를 계산합니다. 따라서 호출자가 출력 크기까지 따로 입력할 필요는 없습니다.

엔진은 이 노드의 `prepare()`가 끝난 뒤 `run()`을 부르기 전에 필요한 GPU 버퍼를 확보하거나 재사용해 Tensor에 연결합니다. 이어서 출력 Tensor를 다음 노드의 입력 슬롯에 전달합니다. 보통 이 전달은 GPU 데이터를 다른 버퍼로 복사하는 것이 아니라 같은 저장소를 참조하게 하는 대입입니다. 그래서 ReLU는 행렬곱의 결과 버퍼를 그대로 읽을 수 있습니다.

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

`!pipe`는 eva 핸들의 `operator bool()`로 파이프라인이 비어 있는지 검사합니다. 이 노드는 `createComputePipeline()`을 직접 호출하고, 만든 파이프라인을 멤버 `pipe`에 보관합니다. 행렬 크기 `M`, `N`, `K`는 푸시 상수로 전달하므로 크기가 바뀌어도 같은 파이프라인을 사용할 수 있습니다. 버퍼 연결이나 디스패치 범위가 바뀌면 명령은 다시 기록해야 합니다.

파이프라인과 셰이더도 항상 일대일은 아닙니다. 같은 셰이더라도 파이프라인 생성 시 고정하는 특수화 상수 값에 따라 여러 파이프라인을 만들 수 있습니다. 예를 들어 기본 `Activation` 노드는 자료형과 활성화 함수 종류 등을 지정해 `pipelinePool().request(...)`로 파이프라인을 요청합니다. `PipelinePool`은 같은 설정의 파이프라인을 찾아 재사용하거나 새로 만듭니다. 이 실습의 `NaiveMatmul`은 그 풀을 거치지 않고 직접 생성하는 방식입니다. 위 코드의 `gVaiDestSetPool`은 디스크립터 셋을 할당하는 풀이므로 파이프라인 풀과 구분합니다.

이제 `run(eva::CommandBuffer cmd)`의 본문을 작성합니다. 엔진이 전달한 `cmd`에 실행할 파이프라인, 사용할 버퍼와 상수, 디스패치와 배리어를 기록합니다. `run()`을 호출하는 순간 GPU가 행렬곱을 계산하는 것은 아닙니다. 엔진이 기록을 마친 커맨드 버퍼를 큐에 제출하면 GPU가 그 명령을 실행합니다.

각 Tensor의 `buffer()`로 GPU 버퍼 범위를 가져와 GLSL의 바인딩 순서대로 셋에 씁니다. 슬롯 이름과 등록 순서는 셰이더의 binding 번호와 별개입니다. 이 셰이더의 0, 1, 2번 바인딩에는 `write()`에 넘긴 순서대로 출력, 입력, 가중치 버퍼가 들어갑니다.

```cpp
auto outputBuffer = (*this)["out0"].buffer();
auto inputBuffer = (*this)["in0"].buffer();
auto weightBuffer = (*this)["weight"].buffer();
descSet.write({outputBuffer, inputBuffer, weightBuffer});
```

바로 아래에서 파이프라인 바인딩부터 배리어까지 메서드 체인으로 연결합니다. 세미콜론은 마지막 `barrier()` 뒤에만 붙입니다.

```cpp
cmd.bindPipeline(pipe)
   .bindDescSets({descSet})
   .setPushConstants(0, sizeof(params), &params)
   .dispatch2(params.N, params.M)
   .barrier(eva::SYNC_SCOPE::COMPUTE_WRITE / outputBuffer /
            eva::SYNC_SCOPE::COMPUTE_READ);
```

`setPushConstants()`는 푸시 상수 영역의 오프셋 0부터 `params`의 12바이트를 기록합니다. 셰이더에서는 이 값을 `p.M`, `p.N`, `p.K`로 읽습니다. 출력 한 칸에 스레드 하나를 배정했으므로 가로로 `N`개, 세로로 `M`개의 스레드가 필요합니다.

`dispatch(x, y)`의 인자는 워크그룹 수입니다. `dispatch2(x, y)`의 인자는 필요한 스레드 수이며, eva가 각 축을 바인딩된 파이프라인의 워크그룹 크기로 나누고 올림합니다. 이 예에서 `dispatch2(2, 2)`는 8×8 워크그룹 하나를 실행합니다. 앞서 작성한 경계 검사 덕분에 나머지 60개 스레드는 행렬 밖에 쓰지 않습니다.

마지막 `barrier()`는 출력 버퍼의 메모리 배리어를 기록합니다. 행렬곱 셰이더가 쓴 `Z`를 다음 디스패치의 ReLU 셰이더가 읽습니다. 13장의 중간 버퍼와 같은 쓰기 → 읽기 의존성입니다.

여기까지 작성한 `prepare()`와 `run()`의 역할을 비교하면 다음과 같습니다.

| 구분 | `prepare()` | `run(cmd)` |
|---|---|---|
| 역할 | 입력을 검사하고 출력 및 임시 Tensor의 형상과 자료형을 정합니다. | 준비된 버퍼와 파이프라인으로 GPU 명령을 기록합니다. |
| 호출 시점 | 입력과 가중치 Tensor가 슬롯에 설정된 뒤입니다. | 엔진이 필요한 GPU 버퍼를 연결한 뒤입니다. |
| 이 실습에서 할 일 | `M`, `N`, `K`와 출력 `[M,N]`을 정하고 파이프라인을 준비합니다. | 버퍼 바인딩, 푸시 상수, 디스패치와 배리어를 기록합니다. |

두 함수 모두 CPU에서 호출합니다. 실제 계산은 엔진이 명령을 제출한 뒤 GPU가 수행합니다.

## Net에서 두 노드 연결하기

`main.cpp`의 빈 `Net` 구조체 안에 노드 두 개를 멤버로 선언합니다. 방금 작성한 `NaiveMatmul`과 vAi의 `Activation`을 사용하고, 활성화 함수로 ReLU를 선택합니다.

```cpp
NaiveMatmul matmul;
vai::node::Activation relu{{.actType = vai::ActType::Relu}};
```

노드를 멤버로 두는 이유는 그래프가 실행될 때까지 객체를 유지하기 위해서입니다. 연결 정보는 슬롯의 주소를, 엔진의 실행 목록은 노드의 주소를 참조합니다. 생성자 안의 지역 변수로 노드를 만들어 연결하면 생성자가 끝날 때 객체가 사라져 연결이 유효하지 않게 됩니다. 연결 뒤에 노드를 옮겨 주소가 바뀌어도 같은 문제가 생기므로, 이 예제처럼 멤버를 유지하고 첫 실행 전에 연결을 마칩니다.

`NeuralNet`은 그래프와 호출 코드 사이에 입력 및 출력 경계를 만듭니다. 예를 들어 `input("features") - relu - output("prediction");`이라고 연결하면, `features`와 `prediction`은 호출할 때 사용할 외부 이름입니다. 이 경우 호출의 입력 키는 `features`, 출력 키는 `prediction`이 됩니다. 내부 슬롯 이름 `in0`, `out0`을 바꾸는 문법은 아닙니다.

`input(...)`이 돌려주는 `InputNode`는 외부 Tensor를 내부 `in0`으로 받아 `out0`으로 전달합니다. `output(...)`의 `OutputNode`는 최종 Tensor를 `in0`으로 받아 호출자에게 돌려줄 수 있도록 보관합니다. 그래프의 끝이어서 다음 노드로 넘길 `out0` 슬롯은 없습니다.

이 실습은 외부 이름도 `in0`, `out0`으로 정합니다. 두 멤버 아래에 다음 생성자를 추가합니다. `-`는 왼쪽 노드의 `out0` 슬롯을 오른쪽 노드의 `in0` 슬롯에 연결하므로, 마지막 연결은 ReLU의 `out0`에서 OutputNode의 `in0`으로 이어집니다.

```cpp
explicit Net(const vai::NeuralNetConfig& config)
    : vai::NeuralNet(config)
{
    input("in0") - matmul - relu - output("out0");
}
```

더 큰 모델에서 같은 구조를 반복한다면 `NodeGroup`으로 묶을 수 있습니다. 예를 들어 행렬곱과 ReLU를 그룹의 멤버로 두고 연결한 뒤, `defineSlot("in0", matmul.slot("in0"))`와 `defineSlot("out0", relu.slot("out0"))`로 양 끝을 노출합니다. 그룹 밖에서 `in0`에 연결하면 실제로는 내부 matmul의 `in0`에 연결됩니다. `[]`가 슬롯의 Tensor 값을 다룬다면 `slot()`은 이렇게 연결 위치를 지정할 때 사용합니다.

그룹에는 다른 그룹도 넣을 수 있지만, 그룹 자체의 `prepare()`나 `run()`으로 별도 계산을 하지는 않습니다. 실행되는 것은 내부 노드들이며 커널을 자동으로 합치지도 않습니다. 이 실습에서는 두 노드를 Net에 직접 연결한 코드를 그대로 사용합니다.

## 입력 한 쌍으로 실행하기

이제 같은 파일의 `main()` 안에서 Net을 만듭니다. 가중치와 노드 출력의 기본 타입을 FP32로 지정하면 ReLU의 출력도 FP32입니다. 이 설정이 기존 Tensor를 변환하는 것은 아니므로 입력과 가중치 Tensor에도 FP32를 명시합니다. `NaiveMatmul`의 출력은 앞서 `prepare()`에서 FP32로 지정했습니다.

`Tensor({2, 3}, ...)`는 입력의 형상과 자료형을 정하고, `setFloats()`가 실제 여섯 값을 CPU 메모리에 넣습니다. 각 행의 값을 왼쪽에서 오른쪽으로, 첫 행부터 차례로 넣습니다. 이 시점에는 GPU로 업로드하지 않습니다.

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

이 대입으로 `prepare()`가 읽을 가중치 Tensor가 정해집니다. 학습된 모델에서는 호출 코드가 숫자를 직접 나열하는 대신 가중치 로더가 파일을 읽어 같은 슬롯에 Tensor를 넣을 수 있습니다. 어느 경우든 필요한 가중치는 첫 실행 전에 설정합니다.

`main()` 끝에서 Net을 한 번 호출합니다. 첫 인자는 외부 입력 이름과 Tensor, 둘째 인자는 외부 출력 이름과 결과를 받을 Tensor의 주소를 짝지은 것입니다. 생성자에서 등록한 외부 이름이 `in0`, `out0`이므로 호출에도 같은 이름을 씁니다. `vai::Tensor output;`은 형상도 없는 빈 Tensor이고, 엔진이 이 변수에 최종 결과의 Tensor를 대입합니다.

```cpp
vai::Tensor output;
net({{"in0", input}}, {{"out0", &output}});

const float* y = output.getData<float>();
std::cout << "ReLU(X @ W)\n"
          << y[0] << ' ' << y[1] << '\n'
          << y[2] << ' ' << y[3] << '\n';
```

첫 `net(...)` 호출에서 엔진은 입력과 가중치를 GPU에서 쓸 수 있게 준비합니다. InputNode에서 받은 Tensor로 `NaiveMatmul::prepare()`가 출력 정보를 정하면, 엔진이 출력 버퍼를 연결하고 `run()`으로 행렬곱 명령을 기록합니다. 이어서 ReLU도 같은 순서로 준비하고 명령을 기록합니다. OutputNode가 최종 Tensor를 보관하면 엔진은 기록을 마치고 GPU 큐에 제출합니다.

`net(...)`이 반환되었다고 GPU 계산까지 끝난 것은 아닙니다. `output.getData<float>()`는 필요한 GPU 완료 대기와 다운로드를 거쳐 CPU에서 결과를 읽을 포인터를 돌려줍니다. 2×2 출력의 첫 두 값이 첫 행입니다.

같은 모델을 다시 호출할 때는 이미 기록한 추론용 커맨드 버퍼를 재제출할 수 있습니다. 따라서 `prepare()`와 `run()`이 매 추론마다 반드시 호출되지는 않습니다. 재사용 여부는 입력 형상뿐 아니라 입력 버퍼와 오프셋, 활성 입력, 출력 대상의 저장소 및 명령 무효화 상태 등에 달려 있습니다. 노드 설정처럼 기록된 명령에 영향을 주는 값을 바꿀 때는 구현에서 필요에 따라 `invalidateCommandBuffer()`로 재기록을 요청해야 합니다. 입력 숫자만 바꾸고 나머지 조건이 유지되면 새 값을 업로드한 뒤 같은 연산 명령을 사용할 수 있습니다. 이는 이전 결과를 꺼내 쓰는 결과 캐시가 아니라 명령 재사용이며, GPU는 제출할 때마다 다시 계산합니다.

## 빌드와 실행

Vulkan 1.3을 지원하는 장치, `glslc`가 포함된 Vulkan SDK 1.3 이상, CMake 3.24 이상과 C++23 컴파일러를 준비합니다. Linux에서 GCC를 쓸 때는 vAi가 요구하는 14 이상을 사용합니다.

vAi를 아직 클론하지 않았다면 아래 명령을 실행합니다. vAi를 클론할 때 하위 의존성인 eva도 함께 내려받겠습니다. 아래의 `/path/to/vAi`는 vAi를 둘 실제 절대 경로로 바꿉니다.

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
