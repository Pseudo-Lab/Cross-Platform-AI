# 12. 스테이징 버퍼

[이전: 디스패치와 동기화](../11-dispatch-synchronization/README.md) · [다음: 연속 디스패치](../13-multiple-dispatches/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

11장에서는 CPU가 매핑한 스토리지 버퍼를 컴퓨트 셰이더도 사용했습니다. 이번에는 CPU가 데이터를 주고받는 버퍼와 GPU가 연산하는 버퍼를 나누겠습니다. 셰이더의 `A + B` 연산은 그대로 두고, 입력 업로드와 결과 다운로드를 커맨드 버퍼에 추가합니다.

[main.cpp](main.cpp)에 아래 순서대로 코드를 작성합니다. 완성된 코드는 공개 후 `examples/99-completed/12-staging-buffers.cpp`에서 확인할 수 있습니다.

## 전송용 버퍼와 연산용 버퍼 나누기

CPU가 매핑할 수 있는 메모리와 GPU 연산에 유리한 메모리가 항상 같지는 않습니다. 별도 VRAM을 쓰는 GPU에서는 입력을 장치 메모리로 한 번 옮겨 두면 연산 중 호스트 메모리를 반복해서 읽는 비용을 줄일 수 있습니다. 이 전송을 위해 CPU가 작성한 값을 잠시 담는 것이 스테이징 버퍼(Staging buffer)이며, 결과는 CPU가 읽을 수 있는 리드백 버퍼로 옮깁니다.

```text
CPU → uploadA ──copy──→ inputA ─┐
                              ├─ 디스패치: A + B → output ──copy──→ readback → CPU
CPU → uploadB ──copy──→ inputB ─┘
```

각 버퍼의 용도와 필요한 메모리 속성은 다음과 같습니다.

| 버퍼 | `usage` | 요청할 메모리 속성 | CPU 접근 |
|---|---|---|---|
| `uploadA`, `uploadB` | `eTransferSrc` | `eHostVisible \| eHostCoherent` | 입력 기록 |
| `inputA`, `inputB` | `eStorageBuffer \| eTransferDst` | `eDeviceLocal` | 직접 접근하지 않음 |
| `output` | `eStorageBuffer \| eTransferSrc` | `eDeviceLocal` | 직접 접근하지 않음 |
| `readback` | `eTransferDst` | `eHostVisible \| eHostCoherent` | 초기화·결과 읽기 |

`usage`는 버퍼에 허용할 명령을 정하고, 메모리 속성은 버퍼를 어디에 할당할지 고르는 조건입니다. 복사 원본에는 `eTransferSrc`, 복사 대상에는 `eTransferDst`가 필요합니다. 디스크립터에는 연산용 `inputA`, `inputB`, `output`만 연결합니다. 전송용 버퍼에는 스토리지 디스크립터가 필요하지 않습니다. [복사 명령의 요구사항](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBuffer.html)

`eDeviceLocal`은 장치 접근에 적합한 메모리라는 뜻이며, CPU 매핑 가능 여부는 `eHostVisible`로 판단합니다. 하나의 유형이 두 속성을 모두 가질 수도 있습니다. 이 예제는 연산용 메모리에 `eHostVisible`을 요구하지 않으므로 직접 매핑하지 않습니다. [메모리 속성 정의](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html)

복사 비용보다 이후 GPU 접근에서 아끼는 비용이 커야 스테이징의 이득이 있습니다. 이 예제처럼 작은 배열을 한 번 더하거나 CPU와 GPU가 메모리를 공유하는 UMA 장치에서는 직접 접근이 더 적합할 수 있으므로, 장치·입력 크기·데이터 재사용 횟수에 따라 측정해야 합니다. [Khronos의 스테이징과 UMA 설명](https://docs.vulkan.org/guide/latest/synchronization_examples.html#_upload_data_from_the_cpu_to_a_vertex_buffer)

> [!NOTE]
> **생각해볼 점**
>
> CPU에서 접근 가능한 메모리를 GPU가 직접 읽는 방식과, 스테이징 버퍼에서 장치 메모리로 복사한 뒤 읽는 방식 중 어느 쪽이 유리할까요? 데이터를 한 번만 읽는 경우와 여러 번 재사용하는 경우, 내장 GPU와 외장 GPU를 나누어 비교해 보세요.

## 전송과 연산 사이의 의존성

11장의 `COMPUTE_SHADER / SHADER_WRITE → HOST / HOST_READ` 의존성은 GPU 출력 버퍼를 CPU가 직접 읽기 위한 것이었습니다. 이제 CPU가 읽는 버퍼가 달라졌으므로 다음 세 의존성이 필요합니다.

| 대상 버퍼 | 먼저 일어나는 접근 | 뒤에 일어나는 접근 |
|---|---|---|
| `inputA`, `inputB` | `TRANSFER / TRANSFER_WRITE` | `COMPUTE_SHADER / SHADER_READ` |
| `output` | `COMPUTE_SHADER / SHADER_WRITE` | `TRANSFER / TRANSFER_READ` |
| `readback` | `TRANSFER / TRANSFER_WRITE` | `HOST / HOST_READ` |

버퍼 데이터를 옮기는 명령은 `copyBuffer()`이며, 배리어는 그 결과를 다음 접근에서 사용할 수 있도록 의존성을 지정합니다. CPU는 마지막 배리어가 포함된 제출의 펜스를 기다린 다음 `readback`을 읽습니다.

복사와 디스패치는 앞에서 선택한 하나의 컴퓨트 큐에 제출합니다. 컴퓨트 기능이 있는 큐는 이 버퍼 복사도 지원하므로 별도 전송 큐를 만들 필요가 없습니다.

## 버퍼 생성 함수에 용도와 메모리 속성 전달하기

`output` 멤버 바로 아래에 전송용 버퍼를 추가합니다.

```cpp
BufferResource uploadA;
BufferResource uploadB;
BufferResource readback;
```

버퍼마다 용도와 메모리 속성이 달라졌습니다. `createBuffer()`의 인자 목록을 다음과 같이 바꿉니다.

```cpp
bool createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                  vk::MemoryPropertyFlags properties, BufferResource& resource)
```

함수 안에서 `bufferInfo.usage`에 고정된 `eStorageBuffer` 대신 전달받은 값을 넣습니다.

```cpp
bufferInfo.usage = usage;
```

`requirements` 조회 다음에 있던 지역 변수 `const auto properties = ...` 두 줄은 지웁니다. 이제 함수 인자로 받은 `properties`를 기존 `findMemoryType()` 호출에 전달합니다. 메모리 요구사항 조회, 할당, 버퍼 바인딩은 그대로 사용합니다.

`findMemoryType()`도 `HOST_VISIBLE` 이외의 속성을 검색하게 됐습니다. 함수 마지막의 실패 메시지를 바꿉니다.

```cpp
std::cerr << "No compatible memory type for requested properties.\n";
```

검색 조건은 바꾸지 않습니다. `memoryTypeBits`가 허용하면서 요청한 속성을 모두 포함하는 유형을 고릅니다. `eDeviceLocal`만 요청했을 때 `eHostVisible`도 가진 유형이 선택되는 것은 유효합니다.

## 역할별 버퍼 만들기

`createStorageBuffers()`의 기존 본문을 지우고 다음 선언부터 넣습니다.

```cpp
const auto hostMemory = vk::MemoryPropertyFlagBits::eHostVisible
                      | vk::MemoryPropertyFlagBits::eHostCoherent;
const auto deviceMemory = vk::MemoryPropertyFlagBits::eDeviceLocal;
const auto inputUsage = vk::BufferUsageFlagBits::eStorageBuffer
                      | vk::BufferUsageFlagBits::eTransferDst;
const auto outputUsage = vk::BufferUsageFlagBits::eStorageBuffer
                       | vk::BufferUsageFlagBits::eTransferSrc;
```

그 아래에서 연산용 버퍼와 전송용 버퍼를 각각 생성합니다.

```cpp
return createBuffer(bufferSize, inputUsage, deviceMemory, inputA)
    && createBuffer(bufferSize, inputUsage, deviceMemory, inputB)
    && createBuffer(bufferSize, outputUsage, deviceMemory, output)
    && createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, hostMemory, uploadA)
    && createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, hostMemory, uploadB)
    && createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst, hostMemory, readback);
```

버퍼는 모두 같은 `bufferSize`를 사용합니다. `createDescriptorSets()`에는 이미 `inputA`, `inputB`, `output`이 연결돼 있으므로 디스크립터 코드는 그대로 둡니다.

## CPU 쓰기 대상을 스테이징으로 바꾸기

`writeInputs()`가 만드는 입력 배열과 NaN 초기값은 유지합니다. 함수 마지막의 세 `writeBuffer()` 호출만 다음과 같이 바꿉니다.

```cpp
return writeBuffer(uploadA, valuesA)
    && writeBuffer(uploadB, valuesB)
    && writeBuffer(readback, initialOutput);
```

이제 CPU는 연산용 메모리를 매핑하지 않습니다. `readback`에 넣는 NaN은 결과 복사가 누락됐을 때 확인할 초기값이며, 연산용 `output`의 초기값은 아닙니다. 셰이더는 유효한 모든 출력 원소를 기록해야 합니다.

CPU 쓰기는 `submitAndWait()` 호출 전에 끝나고, 전송용 메모리는 `HOST_COHERENT` 속성이 있으므로 명시적 flush가 필요하지 않습니다. 이 조건에서는 제출 전에 끝낸 호스트 쓰기를 장치에서 사용할 수 있으므로 입력 복사 앞에 별도 호스트 → 전송 배리어를 추가하지 않습니다. `HOST_COHERENT` 속성은 GPU 완료 대기를 대신하지 않으므로, 다음 회차의 CPU 쓰기는 앞 회차의 펜스 대기 이후에 수행합니다. [호스트 쓰기와 큐 제출](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-submission-host-writes)

## 입력 복사 기록하기

`recordCommandBuffer()`에서 `cmd.begin(beginInfo)` 다음에 복사 범위와 명령을 추가합니다.

```cpp
vk::BufferCopy copyRegion{};
copyRegion.size = bufferSize;
cmd.copyBuffer(*uploadA.buffer, *inputA.buffer, copyRegion); // C API: vkCmdCopyBuffer
cmd.copyBuffer(*uploadB.buffer, *inputB.buffer, copyRegion); // C API: vkCmdCopyBuffer
```

`srcOffset`과 `dstOffset`은 `{}` 초기화로 0입니다. 각 버퍼 처음부터 `bufferSize` 바이트를 복사합니다. 이 호출은 복사 명령을 기록하며, 실제 복사는 이후 큐에 제출했을 때 실행됩니다.

두 입력의 복사가 끝난 다음 셰이더가 읽도록 의존성을 기록하겠습니다. 복사 명령 아래에 두 버퍼 메모리 배리어를 준비합니다.

```cpp
std::array<vk::BufferMemoryBarrier, 2> uploadBarriers{};
uploadBarriers[0].buffer = *inputA.buffer;
uploadBarriers[1].buffer = *inputB.buffer;
```

이어서 두 배리어에 공통 속성을 채웁니다.

```cpp
for (auto& barrier : uploadBarriers)
{
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.offset = 0;
    barrier.size = bufferSize;
}
```

대상은 복사 원본인 업로드 버퍼가 아니라, 복사가 기록한 `inputA`와 `inputB`입니다. 큐 패밀리를 바꾸지 않으므로 두 인덱스는 `VK_QUEUE_FAMILY_IGNORED`로 지정합니다.

반복문 아래에서 배리어를 기록합니다.

```cpp
cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eComputeShader, {}, {}, uploadBarriers, {}); // C API: vkCmdPipelineBarrier
```

파이프라인 단계는 작업 종류를, 접근 마스크는 그 단계에서의 메모리 접근을 지정합니다. 여기서는 전송의 쓰기를 컴퓨트 셰이더의 읽기에 연결했습니다. 이 코드 다음에 기존 파이프라인 바인딩, 디스크립터 셋 바인딩, 푸시 상수, `dispatch()`가 이어집니다.

## 연산 결과를 리드백 버퍼로 복사하기

`dispatch()` 아래의 기존 `hostBarrier` 선언부터 `cmd.pipelineBarrier()`까지 지웁니다. `cmd.end()`는 남겨 두고, 지운 위치에 출력 버퍼의 의존성을 추가합니다.

```cpp
vk::BufferMemoryBarrier downloadBarrier{};
downloadBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
downloadBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
downloadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
downloadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
downloadBarrier.buffer = *output.buffer;
downloadBarrier.offset = 0;
downloadBarrier.size = bufferSize;
```

셰이더가 쓴 `output`을 복사 명령이 읽도록, 그 아래에 배리어와 복사를 순서대로 기록합니다.

```cpp
cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eTransfer, {}, {}, downloadBarrier, {}); // C API: vkCmdPipelineBarrier
cmd.copyBuffer(*output.buffer, *readback.buffer, copyRegion); // C API: vkCmdCopyBuffer
```

입력과 출력의 크기가 같으므로 앞에서 만든 `copyRegion`을 재사용합니다. 이제 GPU 결과가 CPU에서 읽을 수 있는 버퍼로 복사됩니다.

## 복사 결과를 CPU에서 읽도록 연결하기

방금 추가한 출력 복사 다음에 새 `hostBarrier`를 넣습니다. 대상은 `readback`, 앞의 접근은 전송 쓰기입니다.

```cpp
vk::BufferMemoryBarrier hostBarrier{};
hostBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
hostBarrier.dstAccessMask = vk::AccessFlagBits::eHostRead;
hostBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
hostBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
hostBarrier.buffer = *readback.buffer;
hostBarrier.offset = 0;
hostBarrier.size = bufferSize;
```

그 아래에서 전송 → 호스트 의존성을 기록합니다. 이후 기존 `cmd.end()`와 함수의 나머지 코드가 이어집니다.

```cpp
cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                    vk::PipelineStageFlagBits::eHost, {}, {}, hostBarrier, {}); // C API: vkCmdPipelineBarrier
```

명령 기록 순서를 확인하면 다음과 같습니다.

```text
입력 복사 두 번
→ 전송 쓰기 / 컴퓨트 읽기 배리어
→ 디스패치
→ 컴퓨트 쓰기 / 전송 읽기 배리어
→ 출력 복사
→ 전송 쓰기 / 호스트 읽기 배리어
```

각 의존성을 `vk::BufferMemoryBarrier`와 `pipelineBarrier()`로 표현했습니다. 버퍼 메모리 배리어가 다루는 범위와 단계·접근 마스크의 관계는 [Vulkan API reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier.html)에서 확인할 수 있습니다.

## 리드백과 재사용 확인하기

`verifyOutput()`의 시작 부분에서 `output.memory`를 사용한 매핑과 해제를 `readback.memory`로 바꿉니다. 배열 선언과 복사까지 포함하면 다음과 같습니다.

```cpp
std::array<float, elementCount> results{};
void* mapped = readback.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(results.data(), mapped, static_cast<size_t>(bufferSize));
readback.memory.unmapMemory(); // C API: vkUnmapMemory
```

CPU 기준값을 계산하고 결과를 비교하는 반복문은 그대로 사용합니다. `readback`에는 `HOST_COHERENT` 메모리를 사용하므로 명시적 invalidate가 필요하지 않습니다.

`run()`은 커맨드 버퍼를 한 번 기록한 뒤, 입력을 바꿔 두 번 제출합니다. 커맨드 버퍼에는 버퍼의 핸들과 복사 범위가 기록되므로 두 번째 제출도 같은 리소스를 사용하되 새 입력값을 읽습니다. 앞의 펜스 대기가 끝나야 업로드 버퍼를 덮어쓰고 커맨드 버퍼를 다시 제출할 수 있습니다. 버퍼·디스크립터·파이프라인을 매 회차 다시 생성하지 않습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/12-staging-buffers
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target staging_buffers_example
.\build\Debug\main.exe
```

완성본이 있다면 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target staging_buffers_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target staging_buffers_example
./build/main
```

완성본이 있다면 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target staging_buffers_completed
./build/completed/main
```

### 실행 결과

입력을 바꾼 두 번의 실행에서 모두 CPU의 `A + B`와 결과가 일치해야 합니다. 검증 오류 없이 종료되는지 확인하고, 어떤 버퍼를 매핑하는지 코드와 대조해 보세요. CPU에서 접근하는 대상은 `uploadA`, `uploadB`, `readback`이며, 디스크립터에 연결된 대상은 `inputA`, `inputB`, `output`입니다.

기본 `elementCount = 8`에서 검증 출력은 다음과 같습니다.

```text
Round 1: verified 8 elements; first=10, last=24
Round 2: verified 8 elements; first=12, last=26
Resources destroyed.
```

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.
