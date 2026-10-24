# 11. 디스패치와 동기화

[이전: 커맨드 버퍼](../10-command-buffers/README.md) · [다음: 스테이징 버퍼](../12-staging-buffers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

10장에서 기록한 커맨드 버퍼를 컴퓨트 큐에 제출하고, GPU의 계산이 끝나면 출력 버퍼를 CPU에서 읽습니다. 결과를 `A[i] + B[i]`와 비교한 뒤 입력을 바꿔 같은 명령을 한 번 더 실행합니다.

[main.cpp](main.cpp)에 아래 순서대로 코드를 작성합니다. 완성된 코드는 공개 후 `examples/99-completed/11-dispatch-synchronization.cpp`에서 확인할 수 있습니다.

## 결과를 읽기 전에 필요한 것

`dispatch()`는 커맨드 버퍼에 명령을 기록합니다. `submit()`은 그 기록을 큐에 전달하지만 GPU의 연산 완료까지 기다리지 않습니다. CPU가 안전하게 결과를 읽으려면 다음 조건을 연결해야 합니다.

| 수단 | 이 예제에서 하는 일 |
|---|---|
| 메모리 배리어 | 컴퓨트 셰이더가 쓴 결과를 뒤의 호스트 읽기에 보이도록 지정 |
| 펜스 대기 | 해당 큐 제출이 완료될 때까지 CPU에서 대기 |
| `HOST_COHERENT` 메모리 | CPU 캐시에 대한 명시적인 flush·invalidate 호출 생략 |

GPU 작업의 완료와 그 쓰기 결과가 CPU에 보이는 것은 별도 조건입니다. 그래서 펜스로 완료를 확인하고, 메모리 배리어로 쓰기 결과를 호스트 읽기에 연결합니다.

04장에서 `synchronization2` 기능을 활성화하지 않았으므로 이 장은 `vkCmdPipelineBarrier`와 `vkQueueSubmit`에 대응하는 API를 사용합니다.

## 호스트 읽기를 위한 배리어 기록하기

`recordCommandBuffer()`에서 `dispatch()` 다음, `end()` 이전에 배리어를 추가합니다. 먼저 이전 접근과 이후 접근의 종류를 지정합니다.

```cpp
vk::BufferMemoryBarrier hostBarrier{};
hostBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
hostBarrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
```

> [!NOTE]
> **생각해볼 점**
>
> `dstAccessMask`에 `eHostRead`만 지정한 뒤 CPU가 쓰거나 셰이더가 읽으면 어떻게 될까요?

이전 접근은 셰이더가 `output`에 쓰는 작업이고, 이후 접근은 CPU가 그 결과를 읽는 작업입니다. 다음으로 적용할 버퍼 영역을 지정합니다.

```cpp
hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
hostBarrier.buffer = *output.buffer;
hostBarrier.offset = 0;
hostBarrier.size = bufferSize;
```

`output` 전체를 대상으로 합니다. 큐 패밀리 사이에서 버퍼 소유권을 넘기는 작업은 없으므로 두 패밀리 인덱스에 `VK_QUEUE_FAMILY_IGNORED`를 넣습니다.

이어서 배리어를 커맨드 버퍼에 기록합니다.

```cpp
cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eHost, {}, {}, hostBarrier, {}); // C API: vkCmdPipelineBarrier
```

첫 두 인수는 동기화할 **단계**이고, 앞서 설정한 접근 마스크는 그 단계에서의 **메모리 접근 종류**입니다. 여기서는 `ComputeShader / ShaderWrite → Host / HostRead`를 연결합니다. 빈 목록들은 의존성 플래그, 일반 메모리 배리어, 이미지 배리어를 사용하지 않는다는 뜻입니다.

배리어는 CPU를 기다리게 하는 호출이 아닙니다. 기록한 배리어까지 실행됐는지 CPU가 확인하는 부분은 펜스로 작성합니다. [버퍼 메모리 배리어의 접근 범위](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-buffer-memory-barrier)

> [!NOTE]
> **생각해볼 점**
>
> - 파이프라인 단계와 접근 마스크는 각각 무엇을 지정하나요?
> - 이 예제에서 펜스를 기다리는 것과 `ShaderWrite → HostRead` 배리어를 넣는 것은 각각 어떤 역할을 하나요?

## 펜스 만들기

펜스(Fence)는 큐 작업의 완료를 호스트가 확인하는 객체입니다. `device.waitIdle()`로 장치의 모든 큐를 기다리는 대신, 결과를 읽을 제출의 완료를 지정할 수 있어 사용합니다. `private`의 `commandBuffers` 뒤에 멤버를 추가합니다.

```cpp
vk::raii::Fence fence = nullptr;
```

클래스의 `private` 영역 끝에 생성 함수를 만듭니다.

```cpp
bool createFence()
{
    fence = vk::raii::Fence(device, vk::FenceCreateInfo{}); // C API: vkCreateFence
    return true;
}
```

`flags`를 비워 두면 `unsignaled` 상태로 만들어집니다. 큐에 제출할 때 이 펜스를 전달하고, 제출한 작업이 완료되면 `signaled` 상태가 됩니다. 이 예제는 첫 제출 전에 기다리지 않으므로, 생성 시 `eSignaled`를 지정할 필요가 없습니다. [펜스 생성 정보](https://docs.vulkan.org/refpages/latest/refpages/source/VkFenceCreateInfo.html)

`initVulkan()`의 마지막 `&& allocateCommandBuffer();`를 다음과 같이 바꿉니다.

```cpp
    && allocateCommandBuffer()
    && createFence();
```

## 버퍼 생성과 입력 기록 분리하기

입력을 두 번 바꿔 실행할 것이므로, `createStorageBuffers()`는 버퍼 할당만 담당하게 바꿉니다. 기존 함수의 값 배열 생성과 `writeAndVerifyBuffer()` 호출 부분을 제거하고 다음 형태로 남깁니다.

```cpp
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
```

버퍼의 메모리 타입은 이전 장과 같은 `HOST_VISIBLE | HOST_COHERENT`입니다. 버퍼와 디스크립터는 유지하고, 매 실행 전에 메모리 내용만 바꿉니다.

다음으로 기존 `writeAndVerifyBuffer()` 함수 전체를 아래의 `writeBuffer()`로 교체합니다. 이 함수는 데이터를 쓰는 역할만 맡고, GPU 연산 결과의 확인은 뒤에서 작성할 `verifyOutput()`이 맡습니다.

```cpp
bool writeBuffer(BufferResource& resource, const std::array<float, elementCount>& values)
{
    void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
    std::memcpy(mapped, values.data(), static_cast<size_t>(bufferSize));
    resource.memory.unmapMemory(); // C API: vkUnmapMemory
    return true;
}
```

헤더 목록에 계산 결과 검사와 초기값 생성에 필요한 헤더를 추가합니다.

```cpp
#include <cmath>
#include <limits>
```

클래스 끝에 `writeInputs()`를 추가합니다. 첫 실행은 `round = 0`, 두 번째 실행은 `round = 1`로 호출합니다.

```cpp
bool writeInputs(uint32_t round)
{
}
```

함수 안에서 입력 배열 두 개와 출력 초기값을 준비합니다.

```cpp
std::array<float, elementCount> valuesA{};
std::array<float, elementCount> valuesB{};
std::array<float, elementCount> initialOutput{};
initialOutput.fill(std::numeric_limits<float>::quiet_NaN());
for (uint32_t i = 0; i < elementCount; ++i)
{
    valuesA[i] = static_cast<float>(i + round);
    valuesB[i] = 10.0f + static_cast<float>(i + round);
}
```

출력을 NaN으로 채우면 쓰이지 않은 원소가 결과 검사에서 실패합니다. 이전 실행의 정답이 그대로 남아 재실행에 성공한 것처럼 보이는 것도 막습니다. NaN은 자기 자신과도 같지 않으므로 이전의 배열 동등 비교를 그대로 사용하지 않습니다.

이어서 세 배열을 각 버퍼의 메모리에 기록합니다.

```cpp
return writeBuffer(inputA, valuesA)
    && writeBuffer(inputB, valuesB)
    && writeBuffer(output, initialOutput);
```

호스트의 쓰기를 끝낸 뒤 큐에 제출하면, 이 쓰기가 뒤의 디바이스 접근에 전달되도록 큐 제출의 메모리 의존성이 적용됩니다. `HOST_COHERENT` 메모리를 사용하므로 `flushMappedMemoryRanges()`는 생략합니다. GPU가 앞 실행에서 같은 버퍼를 사용 중이면 그 완료를 먼저 기다려야 하며, 아래의 실행 순서에서 이를 보장합니다. [Host Write Ordering Guarantees](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-submission-host-writes)

## 제출하고 완료 기다리기

클래스 끝에 제출과 대기를 담당할 함수를 추가합니다.

```cpp
bool submitAndWait()
{
}
```

함수 안에서 커맨드 버퍼 핸들을 꺼내 제출 정보에 연결합니다.

```cpp
const vk::CommandBuffer commandBuffer = *commandBuffers[0];
vk::SubmitInfo submitInfo{};
submitInfo.commandBufferCount = 1;
submitInfo.pCommandBuffers = &commandBuffer;
computeQueue.submit(submitInfo, *fence); // C API: vkQueueSubmit
```

이 호출은 10장에서 기록한 명령을 실제 큐에 제출합니다. 하나의 큐만 사용하고 CPU가 완료를 기다리는 구조라서 큐 간 동기화를 위한 세마포어는 필요하지 않습니다. 제출에 사용한 펜스는 `unsignaled` 상태여야 합니다. [큐 제출 규칙](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueueSubmit.html)

바로 뒤에서 펜스가 신호 상태가 될 때까지 기다립니다.

```cpp
const auto result = device.waitForFences(*fence, VK_TRUE, UINT64_MAX); // C API: vkWaitForFences
if (result != vk::Result::eSuccess)
{
    device.waitIdle(); // C API: vkDeviceWaitIdle
    std::cerr << "Fence wait did not complete successfully.\n";
    return false;
}
return true;
```

`VK_TRUE`는 전달한 펜스가 모두 완료되기를 기다린다는 뜻이며 여기서는 하나만 전달합니다. `UINT64_MAX`로 무기한 대기를 요청합니다. 성공이 확인된 뒤에만 결과를 읽거나 버퍼를 재사용합니다. `waitIdle()`은 펜스 대기에 성공하지 못한 경로에서 미완료 리소스를 바로 해제하지 않도록 둔 처리이며, 정상 실행에서는 펜스로 해당 제출의 완료를 확인합니다. [펜스 대기 결과](https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForFences.html)

> [!NOTE]
> **생각해볼 점**
>
> 펜스, 파이프라인 배리어, 세마포어는 각각 누가 무엇을 기다리도록 하나요? 특히, 배리어와 세마포어의 차이는 무엇인가요?

## CPU 기준값과 비교하기

클래스 끝에 결과 확인 함수를 추가합니다.

```cpp
bool verifyOutput(uint32_t round)
{
}
```

함수의 첫 부분에서 출력 메모리를 읽어 CPU 배열로 복사합니다.

```cpp
std::array<float, elementCount> results{};
void* mapped = output.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(results.data(), mapped, static_cast<size_t>(bufferSize));
output.memory.unmapMemory(); // C API: vkUnmapMemory
```

이 함수는 반드시 `submitAndWait()` 성공 뒤에 호출합니다. `mapMemory()` 자체는 GPU 작업을 기다리지 않습니다. `HOST_COHERENT` 메모리이므로 읽기 전의 `invalidateMappedMemoryRanges()`를 생략할 수 있습니다.

이어서 각 원소를 CPU에서 계산한 기준값과 비교합니다.

```cpp
for (uint32_t i = 0; i < elementCount; ++i)
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
```

단순히 차이가 허용 오차보다 큰지만 검사하면 NaN을 놓칠 수 있어, `std::isfinite()`로 유한한 값인지 먼저 확인합니다. 검증 레이어는 우리가 의도한 수식까지 알지 못하므로, 잘못된 인덱스나 입력 연결로 오답이 나오는지는 CPU 기준값으로 확인합니다.

## 같은 기록으로 두 번 실행하기

`run()`을 다음과 같이 바꿉니다.

```cpp
bool run()
{
    if (!initVulkan() || !recordCommandBuffer())
    {
        return false;
    }
    for (uint32_t round = 0; round < 2; ++round)
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
```

첫 실행은 생성 직후의 펜스를 사용합니다. 두 번째 실행은 앞 반복에서 펜스 완료를 확인했으므로 이를 `unsignaled`로 초기화하고 다시 제출합니다. 완료되지 않은 제출과 연결된 펜스를 초기화하면 안 됩니다. [펜스 초기화 규칙](https://docs.vulkan.org/refpages/latest/refpages/source/vkResetFences.html)

커맨드 버퍼의 상태는 다음과 같이 바뀝니다.

```text
기록 완료: executable
    → submit: pending
    → 펜스 대기 성공: executable
    → 같은 기록을 다시 submit: pending
    → 펜스 대기 성공: executable
```

10장에서 `eOneTimeSubmit`을 사용하지 않았으므로 완료된 기록을 다시 제출할 수 있습니다. 여기서는 파이프라인, 디스크립터, 원소 수, 디스패치 그룹 수가 같습니다. 버퍼의 내용만 바뀌므로 셋 갱신이나 명령 재기록이 필요하지 않습니다. [커맨드 버퍼 재제출](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#commandbuffers-lifecycle)

GPU가 사용 중인 버퍼에 CPU가 쓰거나, pending 상태인 커맨드 버퍼를 초기화·해제해서는 안 됩니다. 이 반복문에서는 완료를 기다린 뒤 다음 입력을 쓰며, 마지막 제출도 기다린 뒤 `run()`에서 돌아옵니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤 저장소 루트에서 이동합니다.

```sh
cd examples/11-dispatch-synchronization
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target dispatch_synchronization_example
.\build\Debug\main.exe
```

완성본이 있다면 같은 구성에서 다음과 같이 실행합니다.

```powershell
cmake --build build --config Debug --target dispatch_synchronization_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target dispatch_synchronization_example
./build/main
```

완성본이 있다면 같은 구성에서 다음과 같이 실행합니다.

```sh
cmake --build build --target dispatch_synchronization_completed
./build/completed/main
```

### 완료 기준

기본 원소 수 8에서는 다음 메시지가 출력됩니다.

```text
Round 1: verified 8 elements; first=10, last=24
Round 2: verified 8 elements; first=12, last=26
Resources destroyed.
```

- 두 실행 모두 모든 원소가 CPU 기준값과 허용 오차 안에서 일치합니다.
- 입력을 바꾼 두 번째 실행도 성공하며 검증 오류가 없습니다.

`elementCount`를 1, 64, 65로 바꿔 다시 빌드하면 한 그룹보다 작은 입력, 정확히 한 그룹인 입력, 마지막 그룹에 여분의 인보케이션이 있는 입력을 확인할 수 있습니다. 원소 수가 달라져도 두 번의 결과 검사가 통과해야 합니다.

실행 결과가 보이도록 터미널을 캡처해 제출합니다. 다음 장에서는 CPU 전송용 메모리와 GPU 연산용 메모리를 나누고, 복사 명령과 전송 단계의 동기화를 추가합니다.

## 동기화 오류 검사

`VK_LAYER_KHRONOS_validation`의 일반 검증과 동기화 검증은 별도 설정입니다. 다음처럼 동기화 검증을 활성화해 실행하면 명령 사이의 잘못된 읽기·쓰기 의존성도 검사할 수 있습니다.

Windows PowerShell에서는 실행 전에 환경 변수를 설정합니다.

```powershell
$env:VK_VALIDATION_VALIDATE_SYNC = '1'
.\build\Debug\main.exe
```

Linux에서는 실행할 프로세스에 설정을 전달합니다.

```sh
VK_VALIDATION_VALIDATE_SYNC=1 ./build/main
```

결과값이 맞더라도 접근 순서에 오류가 있을 수 있으므로, CPU 정답 대조와 동기화 검증을 함께 확인합니다. 동기화 검증이 모든 호스트 접근이나 셰이더 내부 경쟁을 검출하지는 않습니다. [Khronos Synchronization Validation](https://vulkan.lunarg.com/doc/view/latest/linux/synchronization_usage.html)

## 참고 자료

이 장은 저장소의 벡터 덧셈 예제를 위해 작성했습니다. 사용한 배리어 API에 대응하는 예제는 [Khronos Synchronization Examples — Legacy synchronization APIs](https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples-%28Legacy-synchronization-APIs%29#cpu-read-back-of-data-written-by-a-compute-shader)에서 확인할 수 있습니다.
