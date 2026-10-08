# 07. 디스크립터 풀과 셋

[이전: 디스크립터 셋 레이아웃](../06-descriptor-layout/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서는 세 바인딩에 스토리지 버퍼가 들어간다는 것을 레이아웃으로 정의했습니다. 이번에는 이 레이아웃으로 디스크립터 셋(descriptor set)을 할당하고, 입력과 출력 버퍼를 실제로 연결해 보겠습니다.

[main.cpp](main.cpp)는 이전 장의 완성 코드에서 시작합니다. 아래 설명을 따라 TODO를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/completed/07-descriptor-sets.cpp`에서 확인할 수 있습니다.

## 코드 구조

디스크립터 셋은 풀(pool)에서 할당받습니다. 따라서 풀을 먼저 만들고, 그다음 셋을 할당하도록 `initVulkan()`을 바꿉니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice()
        && createStorageBuffers()
        && createDescriptorSetLayout()
        && createDescriptorPool()
        && createDescriptorSets();
}
```

셋들을 보관할 컨테이너가 필요하므로 파일 위쪽의 헤더 목록에 `<vector>`를 추가합니다.

```cpp
#include <vector>
```

`descriptorSetLayout` 멤버 바로 아래에 다음 멤버를 순서대로 추가합니다.

```cpp
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;
```

`descriptorSets`를 풀 뒤에 선언하면, 프로그램이 끝날 때 셋이 먼저 해제되고 풀이 나중에 해제됩니다. 이제 클래스 끝의 TODO 위치에 두 함수를 추가합니다. 각 함수 안은 아래 절을 따라 채우겠습니다.

```cpp
bool createDescriptorPool()
{
    // 풀의 크기를 정합니다.
    // 풀을 생성하고 true를 반환합니다.
}

bool createDescriptorSets()
{
    // 셋을 할당합니다.
    // 연결할 버퍼 영역을 지정합니다.
    // 디스크립터를 갱신하고 true를 반환합니다.
}
```

## 디스크립터 풀 생성하기

먼저 풀이 어떤 유형의 디스크립터를 몇 개 제공할지 정해야 합니다. `createDescriptorPool()`의 첫 번째 주석에 다음 코드를 넣습니다.

```cpp
vk::DescriptorPoolSize poolSize{};
poolSize.type = vk::DescriptorType::eStorageBuffer;
poolSize.descriptorCount = 3;
```

입력 두 개와 출력 하나를 연결할 것이므로 스토리지 버퍼 디스크립터 세 개가 필요합니다. 이어서 두 번째 주석을 다음 코드로 채웁니다.

```cpp
vk::DescriptorPoolCreateInfo poolInfo{};
poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
poolInfo.maxSets = 1;
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes = &poolSize;
descriptorPool = vk::raii::DescriptorPool(device, poolInfo); // C API: vkCreateDescriptorPool
std::cout << "Descriptor pool created: 1 set, 3 storage-buffer descriptors.\n";
return true;
```

`maxSets`는 풀에서 할당할 수 있는 셋의 최대 개수입니다. 디스크립터 세 개를 하나의 셋에 넣으므로 1로 설정합니다. `poolSizeCount`는 디스크립터 개수가 아니라 `pPoolSizes`가 가리키는 유형별 크기 정보의 개수입니다. 여기서는 스토리지 버퍼만 사용하므로 크기 정보 하나면 됩니다.

`eFreeDescriptorSet`은 셋을 개별적으로 해제할 수 있게 합니다. `vk::raii::DescriptorSet`은 소멸할 때 자신을 풀에 반환하므로 이 플래그가 필요합니다.

## 디스크립터 셋 할당하기

풀을 만들었으므로 이제 셋을 할당할 수 있습니다. `createDescriptorSets()`의 첫 번째 주석에 다음 코드를 넣습니다.

```cpp
const vk::DescriptorSetLayout layout = *descriptorSetLayout;
vk::DescriptorSetAllocateInfo allocInfo{};
allocInfo.descriptorPool = *descriptorPool;
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;
descriptorSets = device.allocateDescriptorSets(allocInfo); // C API: vkAllocateDescriptorSets
```

`descriptorPool`은 할당에 사용할 풀입니다. `descriptorSetCount`는 할당할 셋의 개수이고, `pSetLayouts`에는 각 셋의 레이아웃을 전달합니다. 하나만 할당하므로 레이아웃 핸들 하나의 주소를 사용합니다.

RAII 객체 앞의 `*`는 내부 Vulkan 핸들을 꺼냅니다. `allocateDescriptorSets()`가 반환한 RAII 셋들을 `descriptorSets`에 보관하면 함수가 끝난 뒤에도 사용할 수 있습니다. 다만 할당만 한 상태에서는 어느 버퍼를 참조할지 정해지지 않았습니다.

## 버퍼 영역 지정하기

버퍼를 참조하는 디스크립터는 `vk::DescriptorBufferInfo`로 구성합니다. 할당 코드 아래의 두 번째 주석을 다음 코드로 바꿉니다.

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

배열의 순서를 바인딩 번호에 맞췄습니다. 0번에는 `inputA`, 1번에는 `inputB`, 2번에는 `output`을 연결합니다. `offset`은 버퍼 안에서 읽거나 쓸 영역의 시작이고, `range`는 그 영역의 바이트 크기입니다. 각 버퍼 전체를 사용하므로 시작은 0, 크기는 앞서 정의한 `bufferSize`입니다.

## 디스크립터 갱신하기

마지막으로 어떤 셋의 어떤 바인딩을 갱신할지 지정하겠습니다. 세 번째 주석에 다음 코드를 넣습니다.

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
std::cout << "Descriptor set updated: bindings 0, 1, 2 -> inputA, inputB, output.\n";
return true;
```

`dstSet`은 갱신할 셋, `dstBinding`은 그 안의 바인딩 번호입니다. 각 바인딩에 디스크립터 하나를 두므로 `dstArrayElement`는 0, `descriptorCount`는 1입니다. `descriptorType`은 레이아웃에서 정한 `eStorageBuffer`와 같아야 합니다. `pBufferInfo`는 앞에서 준비한 버퍼 정보를 가리킵니다.

`updateDescriptorSets()`에 갱신할 항목들을 전달하면 연결이 적용됩니다. 두 번째 인수는 다른 디스크립터에서 복사할 항목들인데, 여기서는 사용하지 않아 `{}`을 전달합니다.

이 함수는 버퍼의 데이터를 복사하지 않습니다. 데이터는 이전 장에서 메모리에 써 두었고, 여기서는 그 버퍼를 참조하도록 디스크립터를 설정했습니다. 셰이더를 실행하려면 이후 파이프라인과 명령 버퍼를 준비하고, 이 셋을 바인딩한 뒤 컴퓨트 작업을 제출해야 합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/07-descriptor-sets
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target descriptor_sets_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target descriptor_sets_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target descriptor_sets_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target descriptor_sets_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 레이아웃 생성 출력에 이어 다음 두 줄이 출력됩니다. 시작 코드를 그대로 실행하면 이전 장의 레이아웃 생성까지만 수행합니다.

```text
Descriptor pool created: 1 set, 3 storage-buffer descriptors.
Descriptor set updated: bindings 0, 1, 2 -> inputA, inputB, output.
```

---

이 문서는 [Khronos Vulkan Tutorial: Descriptor pool and sets](https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/01_Descriptor_pool_and_sets.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.
