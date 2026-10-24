# 10. 커맨드 버퍼

[이전: 컴퓨트 파이프라인](../09-compute-pipeline/README.md) · [다음: 디스패치와 동기화](../11-dispatch-synchronization/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

09장에서 연산에 사용할 컴퓨트 파이프라인을 만들었습니다. 이 장에서는 파이프라인과 디스크립터 셋을 바인딩하고, 원소 수를 전달한 뒤 디스패치(Dispatch) 명령을 커맨드 버퍼(Command buffer)에 기록합니다.

명령을 먼저 기록해 묶어서 제출하면 CPU가 GPU 작업마다 완료를 기다릴 필요가 없습니다. 연산 순서와 리소스가 같다면 기록도 재사용할 수 있어, 다음 장에서는 입력 데이터만 바꿔 같은 커맨드 버퍼를 다시 제출합니다.

[main.cpp](main.cpp)에 아래 순서대로 코드를 작성합니다. 여기서는 기록까지만 마치며, 큐에 제출하고 계산 결과를 확인하는 과정은 다음 장에서 작성합니다. 완성된 코드는 공개 후 `examples/99-completed/10-command-buffers.cpp`에서 확인할 수 있습니다.

## 풀과 커맨드 버퍼 보관하기

커맨드 풀(Command pool)은 명령 기록에 필요한 메모리를 모아 관리하고, 커맨드 버퍼를 할당하는 객체입니다. 같은 풀의 기록용 메모리를 재사용하거나 함께 초기화할 수 있도록 할당 단위를 분리한 것입니다. `private` 영역의 `computePipeline` 뒤에 다음 멤버를 추가합니다.

```cpp
vk::raii::CommandPool commandPool = nullptr;
std::vector<vk::raii::CommandBuffer> commandBuffers;
```

커맨드 버퍼 하나를 `commandBuffers[0]`에 보관합니다. 멤버는 선언의 역순으로 소멸하므로 커맨드 버퍼가 먼저 해제되고, 그 버퍼를 할당한 풀이 나중에 해제됩니다.

클래스의 `private` 영역 끝에 풀 생성 함수의 틀을 추가합니다.

```cpp
bool createCommandPool()
{
}
```

함수 안에서 사용할 큐 패밀리를 지정하고 풀을 만듭니다.

```cpp
vk::CommandPoolCreateInfo poolInfo{};
poolInfo.queueFamilyIndex = computeQueueFamilyIndex;
commandPool = vk::raii::CommandPool(device, poolInfo); // C API: vkCreateCommandPool
return true;
```

이 풀에서 할당한 커맨드 버퍼는 지정한 패밀리의 큐에 제출해야 합니다. 03장에서 선택하고 04장에서 큐를 가져온 `computeQueueFamilyIndex`를 그대로 사용합니다.

`flags`에는 별도 값을 넣지 않습니다. 명령을 한 번 기록해 두고 재제출할 것이므로, 커맨드 버퍼를 개별적으로 초기화해 다시 기록하는 기능은 필요하지 않습니다.

## 프라이머리 커맨드 버퍼 할당하기

풀 생성 함수 다음에 할당 함수의 틀을 추가합니다.

```cpp
bool allocateCommandBuffer()
{
}
```

함수 안에서 풀, 커맨드 버퍼의 종류, 개수를 지정합니다.

```cpp
vk::CommandBufferAllocateInfo allocInfo{};
allocInfo.commandPool = *commandPool;
allocInfo.level = vk::CommandBufferLevel::ePrimary;
allocInfo.commandBufferCount = 1;
```

`ePrimary`는 큐에 직접 제출할 수 있는 커맨드 버퍼입니다. 다른 커맨드 버퍼에서 실행하는 `eSecondary`도 있지만 이 예제에서는 사용하지 않습니다.

이어서 할당받은 RAII 객체를 멤버에 보관합니다.

```cpp
commandBuffers = device.allocateCommandBuffers(allocInfo); // C API: vkAllocateCommandBuffers
return true;
```

할당 직후의 커맨드 버퍼는 `initial` 상태입니다. `begin()`으로 기록을 시작하고 `end()`로 끝내면 큐에 제출할 수 있는 `executable` 상태가 됩니다. [커맨드 버퍼 상태 규칙](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-lifecycle)

## 기록 시작하기

`private`의 `elementCount` 바로 아래에 셰이더의 워크그룹 크기를 추가합니다.

```cpp
static constexpr uint32_t workgroupSize = 64;
```

이 값은 [add.comp](add.comp)의 `local_size_x = 64`와 같아야 합니다. CPU 쪽 상수만 바꿔도 셰이더의 크기가 바뀌지는 않습니다.

할당 함수 다음에 기록 함수의 틀을 만듭니다.

```cpp
bool recordCommandBuffer()
{
}
```

함수 안에서 X 방향 워크그룹 수를 먼저 계산합니다. 한 그룹이 64개의 인보케이션을 실행하므로, 원소 수를 그룹 크기로 나누어 올림합니다.

```cpp
const uint32_t groupCount = (elementCount + workgroupSize - 1) / workgroupSize;
```

그리고 03장에서 조회한 장치 한계와 비교합니다.

```cpp
const auto limits = physicalDevice.getProperties().limits; // C API: vkGetPhysicalDeviceProperties
if (groupCount > limits.maxComputeWorkGroupCount[0])
{
    std::cerr << "Input exceeds device limits.\n";
    return false;
}
```

X 방향 그룹 수가 장치에서 지원하는 최대값을 넘으면 기록하지 않습니다. 이 실습은 작은 고정 크기 입력을 사용합니다.

> [!NOTE]
> **생각해볼 점**
>
> 서브그룹(subgroup)은 워크그룹과 어떤 관계이며 CUDA의 warp와는 어떻게 비교할 수 있나요? 워크그룹 크기와 서브그룹 크기는 각각 누가 정하나요?

그 아래에서 첫 커맨드 버퍼를 참조하고 기록을 시작합니다.

```cpp
auto& cmd = commandBuffers[0];
vk::CommandBufferBeginInfo beginInfo{};
cmd.begin(beginInfo); // C API: vkBeginCommandBuffer
```

이제 `vkCmd*`에 대응하는 함수를 호출하면 커맨드 버퍼에 명령이 쌓입니다. 이 호출들은 GPU 연산을 즉시 실행하지 않습니다.

`beginInfo.flags`도 비워 둡니다. `eOneTimeSubmit`을 지정하면 한 번 제출한 기록을 완료 후 그대로 다시 제출할 수 없는데, 다음 장에서는 입력 데이터만 바꿔 같은 기록을 두 번 실행할 것입니다.

## 파이프라인과 디스크립터 셋 바인딩하기

기록 시작 코드 아래에 컴퓨트 파이프라인을 바인딩합니다.

```cpp
cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline); // C API: vkCmdBindPipeline
```

`eCompute`는 컴퓨트 명령이 사용할 파이프라인을 지정한다는 뜻입니다. 뒤에 기록할 디스패치는 이 파이프라인의 셰이더를 사용합니다. [파이프라인 바인딩](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindPipeline.html)

이어서 07장에서 입력과 출력 버퍼를 연결한 디스크립터 셋을 바인딩합니다.

```cpp
cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout,
                       0, *descriptorSets[0], {}); // C API: vkCmdBindDescriptorSets
```

`pipelineLayout`은 09장에서 만든 인터페이스이고, 그 다음의 `0`은 첫 번째로 바인딩할 **셋 번호**입니다. 셰이더의 `set = 0`에 해당하며, 셋 안의 `binding = 0, 1, 2`와 구분해야 합니다. 한 셋 안에 `inputA`, `inputB`, `output`이 모두 들어 있습니다.

마지막 `{}`은 동적 오프셋 목록입니다. 이 예제는 일반 스토리지 버퍼 디스크립터를 사용하므로 전달할 동적 오프셋이 없습니다. 바인딩은 버퍼 데이터를 복사하지 않고, 디스패치가 사용할 셋을 선택합니다. [디스크립터 셋 바인딩](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindDescriptorSets.html)

## 원소 수 전달하기

디스크립터 셋을 바인딩한 다음 푸시 상수 값을 기록합니다.

```cpp
const uint32_t count = elementCount;
cmd.pushConstants<uint32_t>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute,
                                0, count); // C API: vkCmdPushConstants
```

09장에서 레이아웃에 선언한 범위에 실제 값을 넣는 단계입니다. 오프셋 `0`부터 `uint32_t` 크기인 4바이트를 전달하며, 셰이더는 이 값을 읽어 유효한 원소 인덱스인지 검사합니다.

푸시 상수의 값은 이 호출에서 커맨드 버퍼에 기록됩니다. 이후 CPU 변수만 바꿔도 기록된 값이 바뀌지는 않습니다. 원소 수를 바꾸려면 해당 값을 포함한 명령을 다시 기록해야 합니다. [푸시 상수 기록](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPushConstants.html)

## 디스패치 기록하기

함수 시작에서 계산한 워크그룹 수를 사용해 디스패치를 기록합니다.

```cpp
cmd.dispatch(groupCount, 1, 1); // C API: vkCmdDispatch
```

`dispatch()`의 인수는 인보케이션 수가 아니라 **워크그룹 수**입니다. Y와 Z 방향은 각각 한 그룹만 사용합니다. 원소가 65개라면 X 방향으로 두 그룹, 총 128개의 인보케이션이 필요하며, 65 이상인 인덱스는 셰이더의 범위 검사에서 돌아갑니다. [디스패치의 그룹 수](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdDispatch.html)

마지막으로 기록을 끝냅니다.

```cpp
cmd.end(); // C API: vkEndCommandBuffer
std::cout << "Command buffer recorded: " << groupCount << " workgroups.\n";
return true;
```

`end()`가 성공하면 제출 가능한 기록이 만들어집니다. 아직 큐 제출을 하지 않았으므로 출력 버퍼에는 GPU의 계산 결과가 들어 있지 않습니다.

> [!NOTE]
> **생각해볼 점**
>
> - 원소 수가 130이고 워크그룹 크기가 64라면 워크그룹과 인보케이션은 각각 몇 개가 필요할까요? 실제 덧셈을 수행하지 않는 인보케이션은 어떻게 처리되나요?
> - `dispatch()`를 호출한 시점에 GPU 계산도 시작될까요? CUDA와 어떤 차이가 있나요?

## 초기화와 실행 순서에 연결하기

`initVulkan()` 끝의 `&& createComputePipeline();`을 다음과 같이 바꿉니다.

```cpp
    && createComputePipeline()
    && createCommandPool()
    && allocateCommandBuffer();
```

그리고 `run()`의 초기화 성공 뒤 `return true;`를 다음 줄로 바꿉니다.

```cpp
return recordCommandBuffer();
```

프로그램은 버퍼·셋·파이프라인을 준비하고, 커맨드 버퍼를 할당한 뒤 명령을 기록합니다. 다음 장에서는 기록에 결과를 CPU가 읽기 위한 메모리 배리어를 추가하고 큐에 제출합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤 저장소 루트에서 이동합니다. CMake는 08장과 같은 방식으로 셰이더를 SPIR-V로 컴파일합니다.

```sh
cd examples/10-command-buffers
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target command_buffers_example
.\build\Debug\main.exe
```

완성본이 있다면 같은 구성에서 다음과 같이 실행합니다.

```powershell
cmake --build build --config Debug --target command_buffers_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target command_buffers_example
./build/main
```

완성본이 있다면 같은 구성에서 다음과 같이 실행합니다.

```sh
cmake --build build --target command_buffers_completed
./build/completed/main
```

### 완료 기준

기본 원소 수 8에서는 다음 메시지가 출력됩니다.

```text
Command buffer recorded: 1 workgroups.
Resources destroyed.
```

- 검증 오류 없이 커맨드 버퍼 기록이 끝납니다.
- 출력된 워크그룹 수가 `elementCount`를 `workgroupSize`로 나누어 올림한 값과 같습니다.
- `bindPipeline → bindDescriptorSets → pushConstants → dispatch` 순서와, 기록·제출의 차이를 코드에서 확인할 수 있습니다.

실행 결과가 보이도록 터미널을 캡처해 제출합니다.

## 참고 자료

이 장은 저장소의 벡터 덧셈 예제를 위해 작성했습니다. 풀·할당·기록 규칙은 [Vulkan Specification: Command Buffers](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)를 참고합니다.
