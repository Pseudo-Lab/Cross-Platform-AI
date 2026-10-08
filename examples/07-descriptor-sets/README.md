# 07. 디스크립터 풀과 셋

[이전: 디스크립터 셋 레이아웃](../06-descriptor-layout/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서는 0, 1, 2번 바인딩에 스토리지 버퍼가 하나씩 들어간다는 것을 레이아웃으로 정의했습니다. 이제 그 구조를 따르는 디스크립터 셋(Descriptor set)을 할당하고, 각 바인딩에 입력과 출력 버퍼를 연결하겠습니다.

디스크립터 셋은 디스크립터 풀(Descriptor pool)에서 할당받습니다. 먼저 풀에 필요한 용량을 마련하고, 셋을 할당한 뒤 버퍼 정보를 써 넣는 순서로 진행합니다.

[main.cpp](main.cpp)를 열고 이전 장의 레이아웃 생성 다음에 이 과정을 추가해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/07-descriptor-sets.cpp`에서 확인할 수 있습니다.

## 풀과 셋을 보관할 자리 만들기

할당받은 셋을 보관할 컨테이너로 `std::vector`를 사용하겠습니다. 파일 위쪽의 헤더 목록에 다음 줄을 추가합니다.

```cpp
#include <vector>
```

그다음 `private` 영역의 `descriptorSetLayout` 멤버 바로 아래에 풀과 셋을 순서대로 선언합니다.

```cpp
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;
```

RAII 멤버는 선언한 순서의 역순으로 소멸합니다. 셋을 풀 뒤에 선언하면, 셋이 자신을 해제할 때 풀이 아직 남아 있게 됩니다. 이번에는 셋을 하나만 할당하며 `descriptorSets[0]`으로 접근하겠습니다.

## 풀의 용량 정하기

풀은 할당할 셋과 디스크립터를 담을 용량이 필요합니다. 우선 클래스 끝의 TODO 위치에 풀 생성 함수의 틀을 만듭니다.

```cpp
bool createDescriptorPool()
{
}
```

함수 안에서 어떤 유형의 디스크립터를 몇 개 제공할지 정합니다. 입력 두 개와 출력 하나를 연결하므로 스토리지 버퍼 디스크립터 세 개를 준비합니다.

```cpp
vk::DescriptorPoolSize poolSize{};
poolSize.type = vk::DescriptorType::eStorageBuffer;
poolSize.descriptorCount = 3;
```

이 값은 풀 전체에서 사용할 스토리지 버퍼 디스크립터의 용량입니다. 레이아웃에서는 바인딩마다 `descriptorCount`를 1로 지정했지만, 풀은 세 바인딩을 모두 수용해야 합니다.

이어서 풀의 생성 정보를 만듭니다. RAII 셋은 소멸할 때 자신을 개별적으로 해제하므로, 이를 허용하는 플래그를 넣습니다.

```cpp
vk::DescriptorPoolCreateInfo poolInfo{};
poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
```

`eFreeDescriptorSet`이 지정된 풀에서는 할당받은 셋을 하나씩 해제할 수 있습니다. 다음으로 풀에서 할당할 셋의 최대 개수를 설정합니다.

```cpp
poolInfo.maxSets = 1;
```

세 디스크립터를 한 셋에 넣을 것이므로 셋은 하나면 됩니다. `maxSets`와 `poolSize.descriptorCount`는 각각 셋 개수와 특정 유형의 디스크립터 개수를 제한합니다.

이제 앞에서 만든 유형별 용량 정보를 생성 정보에 연결합니다.

```cpp
poolInfo.poolSizeCount = 1;
poolInfo.pPoolSizes = &poolSize;
```

`poolSizeCount`는 `pPoolSizes`로 전달하는 용량 정보의 개수입니다. 스토리지 버퍼라는 한 유형만 사용하므로 정보 하나의 주소를 전달합니다.

## 풀 생성하기

생성 정보가 준비됐으니 논리적 디바이스에서 풀을 만들겠습니다. 같은 함수에서 방금 작성한 코드 아래에 이어 붙입니다.

```cpp
descriptorPool = vk::raii::DescriptorPool(device, poolInfo); // C API: vkCreateDescriptorPool
```

생성한 풀은 멤버에 저장됩니다. 이제 다른 함수에서도 이 풀을 사용해 셋을 할당할 수 있습니다. 함수 끝에는 결과를 출력하고 성공을 반환합니다.

```cpp
std::cout << "Descriptor pool created: 1 set, 3 storage-buffer descriptors.\n";
return true;
```

## 레이아웃을 사용해 셋 할당하기

이제 풀에서 셋을 할당받겠습니다. 방금 만든 함수 아래, 클래스가 끝나기 전에 다음 함수의 틀을 추가합니다.

```cpp
bool createDescriptorSets()
{
}
```

셋을 할당할 때는 각 셋이 따를 레이아웃을 전달해야 합니다. 먼저 함수 안에서 이전 장에 만든 RAII 레이아웃의 Vulkan 핸들을 꺼냅니다.

```cpp
const vk::DescriptorSetLayout layout = *descriptorSetLayout;
```

RAII 객체 앞의 `*`는 내부 핸들을 꺼냅니다. 이 핸들은 앞에서 정의한 세 스토리지 버퍼 바인딩의 구조를 가리킵니다.

할당 정보를 만들고 사용할 풀을 지정합니다. 여기도 RAII 객체 안의 풀 핸들을 전달합니다.

```cpp
vk::DescriptorSetAllocateInfo allocInfo{};
allocInfo.descriptorPool = *descriptorPool;
```

이어서 할당받을 셋의 개수와 레이아웃을 지정합니다. 셋이 하나이므로 레이아웃 핸들 하나의 주소를 전달하면 됩니다.

```cpp
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;
```

`pSetLayouts`는 각 셋에 사용할 레이아웃 핸들의 배열을 가리킵니다. Vulkan은 이 정보를 보고 풀에서 필요한 디스크립터 용량을 사용합니다.

이제 할당 함수를 호출하고 반환된 RAII 셋들을 멤버에 보관합니다.

```cpp
descriptorSets = device.allocateDescriptorSets(allocInfo); // C API: vkAllocateDescriptorSets
```

할당은 끝났지만 아직 셋에 실제 버퍼 정보가 들어 있지는 않습니다. 이어서 0, 1, 2번 바인딩이 각각 어느 버퍼의 어느 영역을 참조할지 지정하겠습니다.

## 버퍼와 사용할 영역 지정하기

버퍼를 참조하는 디스크립터에는 버퍼 핸들, 시작 위치, 사용할 크기가 필요합니다. 이를 담는 `vk::DescriptorBufferInfo` 배열을 할당 코드 아래에 추가합니다.

```cpp
std::array<vk::DescriptorBufferInfo, 3> bufferInfos{};
bufferInfos[0].buffer = *inputA.buffer;
bufferInfos[1].buffer = *inputB.buffer;
bufferInfos[2].buffer = *output.buffer;
```

뒤에서 배열 인덱스를 바인딩 번호로 사용할 것이므로 0번에 `inputA`, 1번에 `inputB`, 2번에 `output`을 넣었습니다. 이 단계는 지역 배열을 채운 것으로, 아직 셋에 반영되지는 않았습니다.

각 버퍼의 처음부터 전체 데이터를 사용하겠습니다. 배열을 채운 코드 아래에 다음 반복문을 추가합니다.

```cpp
for (auto& bufferInfo : bufferInfos)
{
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;
}
```

`offset`은 버퍼 안에서 사용할 영역의 시작 위치이고, `range`는 그 영역의 바이트 크기입니다. 이전 장에서 `bufferSize`를 `sizeof(float) * elementCount`로 정했으므로 각 버퍼의 모든 원소가 포함됩니다.

## 어느 바인딩에 쓸지 지정하기

버퍼 정보가 준비됐으니, 이제 갱신할 셋과 바인딩 번호를 함께 지정하겠습니다. `vk::WriteDescriptorSet` 하나는 디스크립터에 써 넣을 정보를 나타냅니다. 버퍼 영역을 채운 반복문 아래에 배열과 새 반복문을 추가합니다.

```cpp
std::array<vk::WriteDescriptorSet, 3> writes{};
for (std::uint32_t i = 0; i < writes.size(); ++i)
{
    // 이 바인딩에 써 넣을 디스크립터 정보를 지정합니다.
}
```

먼저 반복문 안의 주석을 다음 코드로 바꿉니다. 세 항목 모두 같은 셋을 갱신하며, 바인딩 번호만 `i`에 따라 달라집니다.

```cpp
writes[i].dstSet = *descriptorSets[0];
writes[i].dstBinding = i;
writes[i].dstArrayElement = 0;
```

`dstSet`은 갱신할 셋, `dstBinding`은 그 안의 바인딩 번호입니다. 바인딩은 디스크립터 배열을 가질 수도 있는데, 우리는 바인딩마다 하나만 두므로 첫 원소를 뜻하는 `dstArrayElement = 0`을 사용합니다.

이어서 같은 반복문 안에 디스크립터의 유형과 개수를 넣습니다. 이 값들은 이전 장의 레이아웃에서 정한 구조와 맞아야 합니다.

```cpp
writes[i].descriptorType = vk::DescriptorType::eStorageBuffer;
writes[i].descriptorCount = 1;
```

각 바인딩에 스토리지 버퍼 디스크립터 하나를 씁니다. 마지막으로 앞에서 준비한 버퍼 정보를 연결합니다.

```cpp
writes[i].pBufferInfo = &bufferInfos[i];
```

이제 `writes[0]`은 셋의 0번 바인딩에 `inputA`의 영역을, `writes[1]`은 1번에 `inputB`의 영역을, `writes[2]`는 2번에 `output`의 영역을 쓰도록 지정됐습니다.

## 셋에 갱신 내용 적용하기

반복문까지는 갱신할 내용을 지역 배열에 준비한 것입니다. 반복문이 끝난 뒤 다음 함수를 호출해야 실제 디스크립터 셋에 반영됩니다.

```cpp
device.updateDescriptorSets(writes, {}); // C API: vkUpdateDescriptorSets
```

첫 번째 인수는 방금 채운 갱신 항목들입니다. 두 번째 인수에는 다른 디스크립터에서 복사할 항목을 전달할 수 있지만, 여기서는 사용하지 않아 빈 목록 `{}`을 넘깁니다.

이 호출은 버퍼 속 데이터를 복사하는 것이 아니라 셋이 해당 버퍼 영역을 참조하도록 설정합니다. 버퍼 데이터는 이전 장에서 메모리에 써 두었습니다. 함수 마지막에 결과 출력과 반환을 추가합니다.

```cpp
std::cout << "Descriptor set updated: bindings 0, 1, 2 -> inputA, inputB, output.\n";
return true;
```

## 초기화 순서에 연결하기

풀 생성과 셋 할당·갱신 함수가 모두 완성됐습니다. 이제 `initVulkan()`의 끝부분의 `&& createDescriptorSetLayout();` 한 줄을 다음 세 줄로 바꿉니다. 앞서 작성한 초기화 호출은 그대로 이어집니다.

```cpp
    && createDescriptorSetLayout()
    && createDescriptorPool()
    && createDescriptorSets();
```

레이아웃이 만들어지면 풀을 만들고, 그 풀에서 셋을 할당해 버퍼 정보를 채웁니다. 앞 단계가 `false`를 반환하면 뒤의 호출은 실행되지 않습니다.

여기까지 작성하면 버퍼를 참조하는 디스크립터 셋이 준비됩니다. 셰이더가 이 셋을 사용해 연산하려면 이후 파이프라인과 명령 버퍼를 준비하고, 셋을 바인딩한 뒤 컴퓨트 명령을 제출해야 합니다.

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
