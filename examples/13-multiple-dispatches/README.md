# 13. 연속 디스패치

[이전: 스테이징 버퍼](../12-staging-buffers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

12장에서는 입력을 업로드하고 한 번의 디스패치 결과를 CPU로 가져왔습니다. 이번에는 첫 연산의 출력을 GPU에 둔 채 다음 연산의 입력으로 연결하겠습니다. 기존 덧셈 파이프라인을 두 번 사용해 다음 계산을 수행합니다.

```text
디스패치 1: T = A + B
디스패치 2: output = T + B
```

최종 결과는 `A + 2B`입니다. 다음 연산도 GPU에서 수행하므로 중간값 `T`를 CPU로 가져왔다가 다시 올릴 이유가 없습니다. GPU에 남겨 두면 중간 전송과 CPU 대기를 줄일 수 있으며, 추론에서도 한 연산의 출력을 다음 연산의 입력으로 연결하는 데 이 구조를 사용합니다.

이 덧셈은 한 셰이더로 합쳐 디스패치와 중간 버퍼 접근을 줄일 수도 있습니다. 여기서는 서로 다른 연산을 연결할 때도 필요한 버퍼 연결·동기화를 살펴보려고 두 디스패치로 나눕니다.

[main.cpp](main.cpp)의 TODO를 따라 작성합니다. 완성된 코드는 공개 후 `examples/99-completed/13-multiple-dispatches.cpp`에서 확인할 수 있습니다.

## 파이프라인과 입력 연결 구분하기

파이프라인은 어떤 셰이더를 실행할지 정하고, 디스크립터 셋은 그 셰이더가 어떤 버퍼를 사용할지 정합니다. 두 연산 모두 같은 덧셈 셰이더를 사용하므로 파이프라인은 하나만 있으면 됩니다. 대신 디스패치별로 버퍼를 다르게 연결한 셋 두 개가 필요합니다.

| 바인딩 | 셰이더에서의 역할 | `descriptorSets[0]` | `descriptorSets[1]` |
|---|---|---|---|
| 0 | 첫 번째 입력 | `inputA` | `intermediate` |
| 1 | 두 번째 입력 | `inputB` | `inputB` |
| 2 | 출력 | `intermediate` | `output` |

`intermediate`가 중간값 `T`를 담습니다. 첫 디스패치는 이 버퍼에 쓰고, 둘째 디스패치는 이 버퍼에서 읽습니다. 같은 디스크립터 셋 레이아웃을 두 셋에 사용하므로 셰이더와 파이프라인 레이아웃을 바꿀 필요는 없습니다.

셋 하나를 바인딩해 첫 디스패치를 기록한 뒤, 그 셋의 버퍼 정보를 갱신해서 둘째 디스패치를 기록하면 되지 않을까요? 디스크립터 바인딩은 셋의 내용을 호출 시점별로 복사해 저장하는 방식이 아닙니다. 이 예제의 일반 디스크립터를 바인딩 후 갱신하면 기록 중이거나 실행 가능한 커맨드 버퍼가 무효화될 수 있습니다. 두 셋을 초기화할 때 모두 채워 두고, 기록할 때 사용할 셋을 선택하겠습니다. [디스크립터 갱신 규칙](https://docs.vulkan.org/refpages/latest/refpages/source/vkUpdateDescriptorSets.html)

## 두 디스패치 사이의 의존성

같은 커맨드 버퍼에서 먼저 기록했다는 사실만으로 뒤의 셰이더가 앞의 쓰기 결과를 사용할 수 있는 것은 아닙니다. `intermediate`에 대해 다음 의존성을 기록해야 합니다.

```text
디스패치 1
    ↓ COMPUTE_SHADER / SHADER_WRITE
intermediate의 버퍼 메모리 배리어
    ↓ COMPUTE_SHADER / SHADER_READ
디스패치 2
```

이것은 GPU 작업 사이의 의존성입니다. CPU가 기다리는 펜스는 전체 제출이 끝났는지 확인하는 용도이며, 두 디스패치 사이의 배리어를 대신하지 않습니다. `inputB`는 두 연산 모두 읽기만 하므로 이 구간에서 별도 쓰기 의존성이 없습니다. [Khronos의 컴퓨트 → 컴퓨트 예제](https://docs.vulkan.org/guide/latest/synchronization_examples.html#_compute_to_compute_dependencies)

## 중간 버퍼 추가하기

`output` 멤버 아래의 TODO 위치에 중간 버퍼를 선언합니다.

```cpp
BufferResource intermediate;
```

`createStorageBuffers()`의 `return createBuffer(...)` 호출 연결에서 `output` 생성 다음, `uploadA` 생성 앞에 다음 줄을 끼워 넣습니다.

```cpp
    && createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer, deviceMemory, intermediate)
```

`intermediate`는 셰이더가 쓰고 읽기만 합니다. CPU와 복사할 일이 없으므로 `eTransferSrc`나 `eTransferDst`를 추가하지 않습니다. 크기는 다른 연산용 버퍼와 같은 `bufferSize`이며, 요청하는 메모리 속성은 `eDeviceLocal`입니다.

## 디스크립터 풀의 용량 늘리기

셋 하나는 스토리지 버퍼 디스크립터 세 개를 사용합니다. 셋 두 개를 할당하려면 풀에서 총 여섯 개를 제공해야 합니다. `createDescriptorPool()`의 기존 `poolSize.descriptorCount = 3;`을 바꿉니다.

```cpp
poolSize.descriptorCount = 6;
```

같은 함수의 `poolInfo.maxSets = 1;`도 바꿉니다.

```cpp
poolInfo.maxSets = 2;
```

같은 `inputB`를 두 셋에서 사용해도 디스크립터 슬롯은 셋마다 필요합니다. 버퍼 개수와 디스크립터 개수를 구분해야 합니다. 풀의 `eFreeDescriptorSet` 플래그와 RAII 객체의 선언 순서는 그대로 유지합니다.

## 같은 레이아웃으로 셋 두 개 할당하기

`createDescriptorSets()`의 본문을 새로 작성하겠습니다. 기존 본문을 지우고, 먼저 사용할 레이아웃 두 개를 배열로 준비합니다.

```cpp
const std::array<vk::DescriptorSetLayout, 2> layouts = {*descriptorSetLayout, *descriptorSetLayout};
```

두 항목이 같은 레이아웃을 가리키므로 각 셋의 0, 1, 2번 바인딩 구조는 같습니다. 그 아래에서 할당 정보를 채웁니다.

```cpp
vk::DescriptorSetAllocateInfo allocInfo{};
allocInfo.descriptorPool = *descriptorPool;
allocInfo.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
allocInfo.pSetLayouts = layouts.data();
descriptorSets = device.allocateDescriptorSets(allocInfo); // C API: vkAllocateDescriptorSets
```

셋이 두 개라는 것은 셰이더의 `set = 0`과 `set = 1`을 동시에 사용한다는 뜻이 아닙니다. 두 셋 모두 같은 `set = 0` 자리에 바인딩할 후보이며, 디스패치마다 어느 셋을 연결할지 바꿉니다. 파이프라인 레이아웃의 `setLayoutCount`는 계속 1입니다.

## 각 셋에 다른 버퍼 연결하기

할당 코드 아래에 셋별로 사용할 버퍼 핸들을 모읍니다. 바깥 배열은 셋 번호, 안쪽 배열은 바인딩 번호에 대응합니다.

```cpp
const std::array<std::array<vk::Buffer, 3>, 2> buffers = {{
    {*inputA.buffer, *inputB.buffer, *intermediate.buffer},
    {*intermediate.buffer, *inputB.buffer, *output.buffer}
}};
```

이제 셋마다 세 디스크립터를 갱신할 반복문의 틀을 만듭니다.

```cpp
for (std::uint32_t set = 0; set < buffers.size(); ++set)
{
    std::array<vk::DescriptorBufferInfo, 3> bufferInfos{};
    std::array<vk::WriteDescriptorSet, 3> writes{};
}
```

반복문 안의 두 배열 선언 다음에 바인딩별 정보를 채우는 반복문을 추가합니다.

```cpp
for (std::uint32_t binding = 0; binding < bufferInfos.size(); ++binding)
{
    bufferInfos[binding].buffer = buffers[set][binding];
    bufferInfos[binding].offset = 0;
    bufferInfos[binding].range = bufferSize;
}
```

같은 안쪽 반복문에서 `range`를 채운 다음 줄부터 갱신 정보를 추가합니다.

```cpp
writes[binding].dstSet = *descriptorSets[set];
writes[binding].dstBinding = binding;
writes[binding].descriptorType = vk::DescriptorType::eStorageBuffer;
writes[binding].descriptorCount = 1;
writes[binding].pBufferInfo = &bufferInfos[binding];
```

안쪽 반복문이 끝난 직후, 바깥 `set` 반복문 안에서 갱신을 적용합니다.

```cpp
device.updateDescriptorSets(writes, {}); // C API: vkUpdateDescriptorSets
```

`bufferInfos`와 `writes`는 호출이 끝날 때까지 유효해야 합니다. 버퍼 자체는 클래스 멤버이므로 두 디스패치의 실행이 끝날 때까지 유지됩니다. 바깥 반복문 다음, 함수 마지막에서 성공을 반환합니다.

```cpp
return true;
```

이 함수는 초기화 중 호출됩니다. 명령을 기록하기 전에 두 셋의 참조가 모두 준비됩니다.

## 중간 버퍼의 쓰기와 읽기 연결하기

`recordCommandBuffer()`에는 12장의 입력 복사와 업로드 배리어가 이미 들어 있습니다. 이어지는 첫 번째 `dispatch()`도 그대로 사용합니다. 다만 `descriptorSets[0]`의 출력이 이제 `intermediate`이므로 첫 연산은 `T = A + B`를 기록합니다.

첫 `dispatch()` 다음 TODO 위치에 `intermediateBarrier`를 추가합니다. 기존 `downloadBarrier`보다 앞에 와야 합니다.

```cpp
vk::BufferMemoryBarrier intermediateBarrier{};
intermediateBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
intermediateBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
intermediateBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
intermediateBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
intermediateBarrier.buffer = *intermediate.buffer;
intermediateBarrier.offset = 0;
intermediateBarrier.size = bufferSize;
```

그 아래에서 컴퓨트 → 컴퓨트 의존성을 기록합니다.

```cpp
cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eComputeShader, {}, {}, intermediateBarrier, {}); // C API: vkCmdPipelineBarrier
```

단계가 양쪽 모두 같아도 앞의 셰이더 쓰기와 뒤의 셰이더 읽기는 별개 접근입니다. 이 배리어가 `intermediate`의 쓰기 결과를 뒤의 읽기에 연결합니다. 셰이더 안의 워크그룹 동기화로 서로 다른 디스패치 사이의 이 의존성을 대체할 수는 없습니다.

## 두 번째 셋 바인딩하고 다시 디스패치하기

방금 기록한 배리어 다음에 두 번째 셋을 바인딩하고 디스패치를 추가합니다.

```cpp
cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout,
                       0, *descriptorSets[1], {}); // C API: vkCmdBindDescriptorSets
cmd.dispatch(groupCount, 1, 1); // C API: vkCmdDispatch
```

바인딩할 위치는 계속 0입니다. 파이프라인과 푸시 상수의 원소 개수는 첫 디스패치와 같고 해당 상태가 유지되므로 다시 설정하지 않습니다. 바뀌는 것은 디스크립터 셋이며, 두 번째 셋이 `intermediate + inputB → output`을 연결합니다.

그 뒤에 이미 있던 `downloadBarrier`, `output → readback` 복사, `hostBarrier`, `cmd.end()`가 이어집니다. 최종 흐름은 다음과 같습니다.

```text
입력 업로드 → 업로드 배리어
→ descriptorSets[0] 바인딩 → 디스패치 1: intermediate = A + B
→ 중간 버퍼 배리어
→ descriptorSets[1] 바인딩 → 디스패치 2: output = intermediate + B
→ 다운로드 배리어 → output 복사 → 호스트 배리어
→ 한 번 제출 → 펜스 대기 → CPU 검증
```

기록 내용을 출력하는 줄도 실제 디스패치 개수에 맞게 바꿉니다. `recordCommandBuffer()`의 `cmd.end()` 다음 `std::cout` 한 문장을 교체합니다.

```cpp
std::cout << "Command buffer recorded: " << groupCount << " workgroups per dispatch, 2 dispatches.\n";
```

## CPU 기준값 바꾸기

`verifyOutput()`의 `expected` TODO를 다음과 같이 바꿉니다.

```cpp
const float expected = (a + b) + b;
```

수식으로는 `a + 2 * b`와 같지만, CPU에서도 두 GPU 연산의 순서대로 더합니다. 부동소수점에서는 연산 순서가 달라지면 반올림 결과가 달라질 수 있으므로 기존 오차 허용 비교도 유지합니다.

`run()`은 그대로 사용합니다. 각 회차마다 입력을 바꾸고, 두 디스패치가 들어 있는 커맨드 버퍼를 한 번 제출한 뒤 펜스를 기다립니다. 앞 회차의 펜스 대기가 끝나면 버퍼와 커맨드 버퍼를 재사용할 수 있습니다. `intermediate`도 매 회차 첫 디스패치가 전체 유효 범위를 다시 쓰므로 CPU 초기화나 리드백이 필요하지 않습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/13-multiple-dispatches
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target multiple_dispatches_example
.\build\Debug\main.exe
```

완성본이 있다면 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target multiple_dispatches_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target multiple_dispatches_example
./build/main
```

완성본이 있다면 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target multiple_dispatches_completed
./build/completed/main
```

### 실행 결과

입력을 바꾼 두 번의 실행에서 모두 CPU의 `A + 2B`와 결과가 일치해야 합니다. 검증 오류 없이 종료되는지 확인합니다. 결과가 `A + B`라면 두 번째 디스패치가 사용하는 셋과 출력 버퍼 연결을 먼저 확인하세요.

커맨드 버퍼에서는 디스패치 두 번 사이에 `intermediate`의 쓰기 → 읽기 배리어가 있어야 합니다. 중간 버퍼의 CPU 리드백 없이 마지막 `output`만 복사하고, 한 번의 제출과 펜스 대기로 두 연산을 완료했는지도 확인합니다.

[11장의 동기화 검증 설정](../11-dispatch-synchronization/README.md#동기화-오류-검사)을 적용해 실행합니다. 정답이 우연히 일치해도 디스패치 사이의 읽기·쓰기 의존성이 올바르다고 볼 수는 없습니다. 검증 메시지에 `SYNC-HAZARD-READ-AFTER-WRITE`가 있다면 `intermediateBarrier`의 대상과 단계·접근 마스크를 확인합니다.

기본 `elementCount = 8`에서 검증 출력은 다음과 같습니다.

```text
Command buffer recorded: 1 workgroups per dispatch, 2 dispatches.
Round 1: verified 8 elements; first=20, last=41
Round 2: verified 8 elements; first=23, last=44
Resources destroyed.
```

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.
