# 07. Descriptor pool과 set

[이전: Descriptor set layout](../06-descriptor-layout/README-kr.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

Descriptor pool에서 descriptor set 한 개를 할당합니다.
set의 binding 0, 1, 2에 입력 버퍼 두 개와 출력 버퍼 한 개를 연결합니다.

06장의 `ComputeApplication`에 `descriptorPool`, `descriptorSets` 멤버와
`createDescriptorPool()`, `createDescriptorSets()` 함수를 추가합니다.
전체 코드는 [main.cpp](main.cpp)에 있습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README-kr.md)을 먼저 완료합니다.
C++20, CMake 3.24 이상, Vulkan SDK와 장치의 Vulkan 1.3 지원이 필요합니다.
02장에서 설정한 validation layer도 사용합니다.

저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/07-descriptor-sets
```

Windows에서는 다음 명령을 실행합니다. 실행 경로는 Visual Studio 생성기 기준입니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target descriptor_sets_example
.\build\Debug\descriptor_sets_example.exe
```

Linux에서 Makefiles 또는 Ninja를 사용하면 다음과 같습니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target descriptor_sets_example
./build/descriptor_sets_example
```

이 폴더에서 독립적으로 구성하고 빌드합니다. 이전 장의 `build` 폴더는 재사용하지 않습니다.

## 1. Layout에서 실제 리소스로

06장의 layout에는 binding마다 storage buffer 하나를 연결한다는 형식만 있습니다.
이번에는 그 형식으로 set을 할당하고 실제 버퍼와 범위를 지정합니다.

```text
Descriptor pool
  └─ Descriptor set 한 개
       ├─ binding 0 → inputA.buffer: offset 0, range 32
       ├─ binding 1 → inputB.buffer: offset 0, range 32
       └─ binding 2 → output.buffer: offset 0, range 32
```

Descriptor pool은 set과 descriptor의 저장 공간을 제공합니다.
버퍼 데이터용 메모리는 이미 05장에서 `DeviceMemory`로 할당했습니다.
pool을 만들 때 입력 데이터를 다시 할당하거나 복사하지 않습니다.

`initVulkan()`의 마지막에 새 함수를 추가합니다.

```cpp
return createInstance()
    && setupDebugMessenger()
    && pickPhysicalDevice()
    && createLogicalDevice()
    && createStorageBuffers()
    && createDescriptorSetLayout()
    && createDescriptorPool()
    && createDescriptorSets();
```

## 2. Pool 생성

먼저 layout 멤버 뒤에 pool과 set의 저장 공간을 선언합니다.

```cpp
vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;
```

`createDescriptorPool()`에서 종류별 descriptor 용량과 최대 set 수를 지정합니다.

```cpp
vk::DescriptorPoolSize poolSize{};
poolSize.type = vk::DescriptorType::eStorageBuffer;
poolSize.descriptorCount = 3;

vk::DescriptorPoolCreateInfo poolInfo{};
poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
poolInfo.maxSets = 1;
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes = &poolSize;

descriptorPool = vk::raii::DescriptorPool(device, poolInfo); // C API: vkCreateDescriptorPool
```

| 필드 | 값 | 의미 |
|---|---:|---|
| `poolSize.descriptorCount` | 3 | Storage buffer descriptor의 총용량 |
| `poolInfo.maxSets` | 1 | 할당할 수 있는 set의 최대 수 |
| `poolInfo.poolSizeCount` | 1 | `pPoolSizes` 배열의 항목 수. 여기서는 storage buffer 종류 하나 |

하나의 set에 세 binding이 있으므로 storage buffer descriptor 세 개가 필요합니다.
버퍼 하나에 `float`가 몇 개 들어 있는지는 pool 용량 계산과 관계없습니다.
[Pool 생성 정보](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorPoolCreateInfo.html)를 참고합니다.

`vk::raii::DescriptorSet`은 소멸할 때 `vkFreeDescriptorSets`를 호출합니다.
이 개별 해제를 허용하려면 pool 생성 시 `eFreeDescriptorSet`을 지정해야 합니다.
이 플래그를 빼고 같은 RAII 소멸 방식을 사용하면 올바르지 않습니다.

## 3. Set 할당

`createDescriptorSets()`에서 pool과 layout을 사용해 set 한 개를 할당합니다.

```cpp
const vk::DescriptorSetLayout layout = *descriptorSetLayout;
vk::DescriptorSetAllocateInfo allocInfo{};
allocInfo.descriptorPool = *descriptorPool;
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;

descriptorSets = device.allocateDescriptorSets(allocInfo); // C API: vkAllocateDescriptorSets
```

`pSetLayouts`는 할당할 set마다 사용할 layout을 지정하는 배열입니다.
여기서는 set 하나이므로 layout 핸들 하나의 주소를 전달합니다.
이 핸들 변수는 할당 호출이 끝날 때까지 유효해야 합니다.

반환값은 RAII set의 목록이므로 vector 멤버에 보관합니다. 이 예제에서 목록의 길이는 1입니다.
할당 직후에는 buffer descriptor에 사용할 버퍼 정보가 아직 지정되지 않았습니다.

## 4. 버퍼 범위 지정

각 descriptor가 참조할 버퍼와 byte 범위를 준비합니다.

```cpp
std::array<vk::DescriptorBufferInfo, 3> bufferInfos{};
bufferInfos[0].buffer = *inputA.buffer;
bufferInfos[1].buffer = *inputB.buffer;
bufferInfos[2].buffer = *output.buffer;
for (auto& bufferInfo : bufferInfos)
{
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;
}
```

`offset`은 버퍼 시작부터의 byte 위치입니다. `range`는 이 descriptor로 노출할 byte 수입니다.
`bufferSize`는 `8 * sizeof(float)`, 즉 32바이트입니다. 원소 수인 8을 `range`에 넣지 않습니다.
[DescriptorBufferInfo 정의](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorBufferInfo.html)를 참고합니다.

버퍼를 메모리에 바인딩할 때 사용한 offset과도 구분합니다.
`bindMemory()`의 offset은 `DeviceMemory` 안에서 버퍼가 시작하는 위치이고,
`DescriptorBufferInfo::offset`은 그 버퍼 안에서 셰이더에 노출할 범위가 시작하는 위치입니다.

Storage buffer descriptor의 offset은 `minStorageBufferOffsetAlignment`의 배수여야 합니다.
offset 0은 이 정렬 조건을 만족합니다. 범위는 버퍼 끝을 넘을 수 없으며
`maxStorageBufferRange` 이하여야 합니다. 이 예제는 각 32바이트 버퍼 전체를 지정합니다.

## 5. Descriptor 갱신

버퍼 정보와 갱신할 binding을 연결하는 `WriteDescriptorSet` 세 개를 만듭니다.

```cpp
std::array<vk::WriteDescriptorSet, 3> writes{};
for (std::uint32_t i = 0; i < writes.size(); ++i)
{
    writes[i].dstSet = *descriptorSets[0];
    writes[i].dstBinding = i;
    writes[i].dstArrayElement = 0;
    writes[i].descriptorType = vk::DescriptorType::eStorageBuffer;
    writes[i].descriptorCount = 1;
    writes[i].pBufferInfo = &bufferInfos[i];
}

device.updateDescriptorSets(writes, {}); // C API: vkUpdateDescriptorSets
```

`dstBinding`은 06장 layout의 binding 번호입니다. `dstArrayElement = 0`과
`descriptorCount = 1`은 해당 binding의 첫 번째 descriptor 하나를 갱신한다는 뜻입니다.
`descriptorType`도 layout에 지정한 종류와 일치해야 합니다.

각 `pBufferInfo`는 `bufferInfos` 안의 항목을 가리킵니다.
두 배열은 모두 `updateDescriptorSets()`가 반환될 때까지 살아 있어야 합니다.
호출이 끝나면 이 임시 배열은 제거할 수 있지만, descriptor가 참조하는 버퍼와
그 메모리는 사용할 동안 유지해야 합니다.

두 번째 인수 `{}`는 descriptor 간 복사 요청이 없다는 뜻입니다.
이 호출은 descriptor의 참조 정보를 갱신합니다. 입력 데이터를 버퍼로 복사하거나
컴퓨트 작업을 제출하지 않습니다. 입력 데이터 기록은 05장에서 완료했습니다.

## 6. 수명과 해제 순서

멤버는 선언의 역순으로 소멸합니다. 예제의 선언 순서에 따라 다음 순서로 해제합니다.

```text
Descriptor sets → Descriptor pool → Descriptor set layout
  → 각 Buffer → 각 Buffer의 DeviceMemory → Device
  → Debug messenger → Instance → Context
```

set을 해제하려면 pool과 device가 유효해야 합니다. 따라서 `descriptorSets`를 pool보다
뒤에 선언합니다. 버퍼와 메모리도 set보다 오래 유지합니다.
`BufferResource` 내부에서는 memory를 먼저, buffer를 나중에 선언했으므로
buffer가 memory보다 먼저 해제됩니다.

RAII 객체의 핸들에 별도로 `vkFreeDescriptorSets`나 `vkDestroyDescriptorPool`을 호출하지 않습니다.
이 장은 GPU에 작업을 제출하지 않으므로 종료 전에 대기할 GPU 작업이 없습니다.
제출 후 자원 사용이 끝나는 시점은 11장의 동기화에서 다룹니다.

## 완료 기준

앞 장의 출력 뒤에 다음 줄이 출력되고 종료 코드가 0이어야 합니다.
종료 시에도 validation layer의 사용 오류가 없어야 합니다.

```text
Descriptor pool created: 1 set, 3 storage-buffer descriptors.
Descriptor set updated: bindings 0, 1, 2 -> inputA, inputB, output.
```

`tutorial.callback-check` 메시지는 02장에서 직접 보낸 콜백 확인 메시지입니다.
Vulkan 사용 오류와 구분합니다.

- set 하나를 layout에 따라 할당합니다.
- 세 binding에 각각 올바른 버퍼 핸들, offset 0, range 32를 지정합니다.
- set이 pool보다 먼저 해제됩니다.

이 완료 기준은 버퍼를 descriptor에 연결한 상태입니다. 셰이더 실행과 벡터 덧셈 결과는
검증하지 않습니다. 08장에서 셰이더를 준비하고, 09~11장에서 파이프라인 생성·명령 기록·제출을
이어갑니다.

## 참고 자료

- [Khronos Tutorial: Descriptor pool and sets](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/01_Descriptor_pool_and_sets.html)
- [VkDescriptorPoolCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorPoolCreateInfo.html)
- [VkDescriptorBufferInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkDescriptorBufferInfo.html)
- [VkWriteDescriptorSet](https://docs.vulkan.org/refpages/latest/refpages/source/VkWriteDescriptorSet.html)
- [Vulkan-Hpp RAII 안내](https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/docs/VkRaiiProgrammingGuide.md)

Khronos Vulkan Tutorial contributors의 [공식 원문](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/01_Descriptor_pool_and_sets.html)을
번역한 학습 노트를 바탕으로 컴퓨트용으로 각색했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.
