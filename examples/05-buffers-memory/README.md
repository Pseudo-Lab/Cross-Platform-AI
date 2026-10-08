# 05. Buffer와 Memory

[이전: Logical device](../04-logical-device/README.md) · [다음: Descriptor layout](../06-descriptor-layout/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

논리적 디바이스와 큐를 만들었으니 연산할 데이터를 준비해 보겠습니다. Vulkan에서 버퍼(Buffer)는 데이터를 담는 자원이지만, 버퍼를 만드는 것만으로 저장 공간이 할당되지는 않습니다. 버퍼에 맞는 메모리를 찾아 할당하고 연결해야 합니다.

이번에는 입력 버퍼 두 개와 출력 버퍼 하나를 만들겠습니다. 각 버퍼에 CPU로 값을 기록하고 다시 읽어 확인하는 것까지 진행합니다. GPU에서 이 값을 연산하는 과정은 이후 장에서 이어집니다.

[main.cpp](main.cpp)를 열고 TODO 위치를 따라 작성해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/05-buffers-memory.cpp`에서 확인할 수 있습니다.

## 버퍼 크기와 보관할 자리 만들기

입력값과 읽어 온 값을 배열에 담기 위해 `<array>`가 필요합니다. 복사할 바이트 수에는 `std::size_t`를 사용하므로 `<cstddef>`도 추가합니다. 파일 위쪽의 헤더 TODO를 다음 코드로 바꿉니다.

```cpp
#include <array>
#include <cstddef>
```

각 버퍼에는 4바이트 `float` 값 8개를 담겠습니다. `private` 영역의 `validationLayer` 다음 TODO 위치에 원소 개수와 버퍼 크기를 적습니다.

```cpp
static_assert(sizeof(float) == 4);
static constexpr std::uint32_t elementCount = 8;
static constexpr vk::DeviceSize bufferSize = sizeof(float) * elementCount;
```

`bufferSize`는 원소 하나의 크기에 개수를 곱한 바이트 크기입니다. `static_assert`는 이 예제에서 사용할 `float`의 크기를 컴파일할 때 확인합니다.

버퍼마다 버퍼 객체와 메모리 객체를 함께 보관할 수 있도록, 바로 아래에 구조체를 추가합니다.

```cpp
struct BufferResource
{
    vk::raii::DeviceMemory memory = nullptr;
    vk::raii::Buffer buffer = nullptr;
};
```

멤버는 선언의 역순으로 소멸합니다. `memory`를 먼저 선언했으므로 `buffer`를 해제한 다음 연결된 메모리를 해제하게 됩니다.

이제 `computeQueue` 아래의 TODO를 입력 두 개와 출력 하나를 담을 멤버로 바꿉니다.

```cpp
BufferResource inputA;
BufferResource inputB;
BufferResource output;
```

## 버퍼 객체 만들기

입력과 출력 버퍼는 같은 방식으로 만들 수 있습니다. 클래스 끝의 함수 TODO 위치에 버퍼 하나를 생성할 함수의 틀을 추가합니다. `size`로 바이트 크기를 받고, 생성한 객체는 참조로 받은 `resource`에 저장하겠습니다.

```cpp
bool createBuffer(vk::DeviceSize size, BufferResource& resource)
{
}
```

먼저 함수 안에서 버퍼의 크기와 용도를 정합니다. 컴퓨트 셰이더가 읽고 쓸 데이터를 담을 것이므로 용도는 `eStorageBuffer`로 지정합니다.

```cpp
vk::BufferCreateInfo bufferInfo{};
bufferInfo.size = size;
bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
bufferInfo.sharingMode = vk::SharingMode::eExclusive;
```

`sharingMode`는 큐 패밀리 사이에서 자원을 공유하는 방식입니다. 이 예제에서는 앞에서 선택한 하나의 패밀리만 사용하므로 `eExclusive`로 둡니다.

생성 정보를 준비했으니 그 아래에서 버퍼 객체를 만듭니다.

```cpp
resource.buffer = vk::raii::Buffer(device, bufferInfo); // C API: vkCreateBuffer
```

여기까지는 버퍼 객체만 만들었습니다. 실제 데이터를 담으려면 이 버퍼가 사용할 수 있는 메모리를 연결해야 합니다.

## 버퍼가 요구하는 메모리 확인하기

메모리를 할당하기 전에 드라이버가 요구하는 크기와 메모리 유형을 알아야 합니다. 버퍼 생성 다음 줄에서 요구사항을 가져옵니다.

```cpp
const auto requirements = resource.buffer.getMemoryRequirements(); // C API: vkGetBufferMemoryRequirements
```

`requirements.size`는 실제로 할당해야 하는 크기로, 처음 요청한 버퍼 크기보다 클 수 있습니다. `alignment`는 메모리를 연결할 오프셋의 정렬 조건이고, `memoryTypeBits`는 버퍼에 사용할 수 있는 메모리 유형을 비트로 표시한 값입니다.

버퍼가 허용하는 유형 중에서도 CPU가 직접 값을 기록할 수 있는 메모리를 골라야 합니다. 조회 코드 아래에 필요한 속성을 적습니다.

```cpp
const auto properties = vk::MemoryPropertyFlagBits::eHostVisible
    | vk::MemoryPropertyFlagBits::eHostCoherent;
```

`eHostVisible`은 메모리를 매핑해 CPU에서 접근할 수 있다는 뜻입니다. `eHostCoherent`는 CPU 접근에 필요한 캐시의 flush와 invalidate를 명시적으로 호출하지 않아도 된다는 뜻입니다. GPU 작업의 완료를 기다려 주는 속성은 아닙니다.

이제 버퍼가 허용하면서 이 두 속성도 갖춘 메모리 유형을 찾아야 합니다. 검색 함수를 먼저 작성한 뒤 여기서 호출하겠습니다.

## 사용할 메모리 유형 찾기

잠시 `createBuffer()`에서 나와 그 아래에 다음 함수의 틀을 추가합니다.

```cpp
bool findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                    std::uint32_t& typeIndex)
{
}
```

`typeFilter`에는 버퍼가 허용하는 유형을, `properties`에는 프로그램이 원하는 속성을 전달합니다. 조건에 맞는 유형을 찾으면 `typeIndex`에 인덱스를 저장하고 `true`를 반환하도록 만들겠습니다.

먼저 함수 안에서 물리적 디바이스의 메모리 유형 정보를 조회합니다.

```cpp
const auto memoryProperties = physicalDevice.getMemoryProperties(); // C API: vkGetPhysicalDeviceMemoryProperties
```

유형마다 지원하는 속성이 다를 수 있으므로, 목록을 순서대로 검사합니다. 조회 코드 아래에 반복문의 틀을 추가합니다.

```cpp
for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
{
    // 버퍼의 조건과 필요한 메모리 속성을 검사합니다.
}
```

반복문 안에서 먼저 이 버퍼가 `i`번 메모리 유형을 허용하는지 확인합니다. 주석을 지우고 다음 줄을 넣습니다.

```cpp
const bool allowed = (typeFilter & (1u << i)) != 0;
```

`1u << i`는 `i`번째 비트만 켠 값입니다. `typeFilter`의 같은 비트가 켜져 있다면 이 버퍼에 해당 유형을 사용할 수 있습니다.

버퍼가 허용한다고 CPU 접근도 가능한 것은 아닙니다. 바로 아래에서 이 유형이 우리가 요청한 속성을 모두 갖췄는지 검사합니다.

```cpp
const bool hasProperties =
    (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
```

`properties`에는 `eHostVisible`과 `eHostCoherent`를 함께 넣을 예정입니다. 비트 연산 결과가 `properties`와 같아야 요청한 속성이 모두 포함돼 있다는 뜻입니다.

두 조건을 통과하면 그 유형의 인덱스를 저장하고 검색을 끝냅니다. 같은 반복문 안에 이어서 추가합니다.

```cpp
if (allowed && hasProperties)
{
    typeIndex = i;
    return true;
}
```

반복문을 끝까지 통과했다면 조건에 맞는 유형을 찾지 못했습니다. 반복문 밖, 함수 마지막에 실패 처리를 추가합니다.

```cpp
std::cerr << "No compatible HOST_VISIBLE | HOST_COHERENT memory type found.\n";
return false;
```

검색 함수가 완성됐습니다. `createBuffer()`로 돌아가 `properties`를 선언한 다음에 호출을 추가합니다.

```cpp
std::uint32_t memoryTypeIndex = 0;
if (!findMemoryType(requirements.memoryTypeBits, properties, memoryTypeIndex))
{
    return false;
}
```

검색에 실패하면 이 버퍼의 초기화를 중단합니다. 성공했다면 `memoryTypeIndex`에 사용할 유형의 인덱스가 들어 있으므로 메모리 할당으로 진행할 수 있습니다.

## 메모리 할당하고 버퍼에 연결하기

같은 함수의 방금 작성한 `if`문 아래에서 할당 정보를 만듭니다. 크기에는 드라이버가 알려 준 `requirements.size`를, 유형에는 검색 결과를 넣습니다.

```cpp
vk::MemoryAllocateInfo allocationInfo{};
allocationInfo.allocationSize = requirements.size;
allocationInfo.memoryTypeIndex = memoryTypeIndex;
```

이어서 메모리를 할당하고 `resource`에 보관합니다.

```cpp
resource.memory = vk::raii::DeviceMemory(device, allocationInfo); // C API: vkAllocateMemory
```

버퍼와 메모리가 각각 만들어졌으니 이제 둘을 연결합니다.

```cpp
resource.buffer.bindMemory(*resource.memory, 0); // C API: vkBindBufferMemory
```

두 번째 인수는 할당된 메모리 안에서 버퍼가 시작할 바이트 오프셋입니다. 버퍼마다 별도의 메모리를 할당했으므로 처음 위치인 0을 사용합니다. 0은 버퍼의 정렬 조건도 만족합니다.

함수 마지막에는 선택한 메모리 정보를 출력하고 성공을 반환합니다.

```cpp
std::cout << "Buffer memory: size " << requirements.size
          << ", alignment " << requirements.alignment
          << ", type " << memoryTypeIndex << '\n';
return true;
```

## CPU에서 값 기록하기

버퍼와 메모리를 준비했으니 실제로 값을 넣어 보겠습니다. `findMemoryType()` 아래에 기록과 확인을 담당할 함수의 틀을 추가합니다.

```cpp
bool writeAndVerifyBuffer(BufferResource& resource,
                          const std::array<float, elementCount>& values,
                          const char* label)
{
}
```

`resource`는 기록할 버퍼, `values`는 기록할 값입니다. `label`은 출력할 때 입력 A와 입력 B 등을 구분하는 이름으로 사용합니다.

CPU에서 메모리에 접근하려면 먼저 매핑해야 합니다. 함수의 첫 줄에서 버퍼 크기만큼 매핑합니다.

```cpp
void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
```

`mapMemory()`가 반환한 주소를 통해 CPU가 메모리에 접근할 수 있습니다. 이 주소로 배열의 바이트를 복사하고, 사용을 마쳤으면 매핑을 해제합니다.

```cpp
std::memcpy(mapped, values.data(), static_cast<std::size_t>(bufferSize));
resource.memory.unmapMemory(); // C API: vkUnmapMemory
```

`std::memcpy`에 필요한 `<cstring>`은 시작 코드에 포함되어 있습니다. `unmapMemory()`는 CPU 주소로의 매핑을 해제하는 것이며, 메모리 자체나 기록한 값을 없애지 않습니다.

## 기록한 값 다시 읽기

기록이 됐는지 확인하기 위해 같은 메모리를 다시 매핑하고 별도 배열로 읽겠습니다. 방금 작성한 `unmapMemory()` 아래에 다음 코드를 추가합니다.

```cpp
std::array<float, elementCount> readback{};
mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(readback.data(), mapped, static_cast<std::size_t>(bufferSize));
resource.memory.unmapMemory(); // C API: vkUnmapMemory
```

이번에는 복사 방향이 반대입니다. 버퍼의 메모리에서 CPU 배열인 `readback`으로 값을 가져왔습니다.

바로 아래에서 읽어 온 배열을 원래 값과 비교합니다. 다르면 어떤 버퍼에서 문제가 생겼는지 출력하고 실패를 반환합니다.

```cpp
if (readback != values)
{
    std::cerr << label << ": CPU data verification failed.\n";
    return false;
}
```

값이 같다면 읽은 배열을 출력해 눈으로도 확인하겠습니다. 함수 마지막에 다음 코드를 추가합니다.

```cpp
std::cout << label << ':';
for (float value : readback)
{
    std::cout << ' ' << value;
}
std::cout << '\n';
return true;
```

여기서는 CPU가 기록한 값을 CPU에서 다시 읽었습니다. 아직 GPU 작업을 제출하지 않았으므로 큐 제출이나 GPU 완료 대기 코드는 필요하지 않습니다.

## 입력과 출력 버퍼 준비하기

버퍼를 생성하는 함수와 값을 기록하는 함수가 모두 준비됐습니다. 이제 이 함수들을 사용해 입력 두 개와 출력 하나를 만들겠습니다. `createBuffer()` 앞에 다음 함수의 틀을 추가합니다.

```cpp
bool createStorageBuffers()
{
}
```

함수 안에서 각 멤버를 전달해 버퍼와 메모리를 만듭니다. 하나라도 실패하면 이후 초기화로 진행하지 않습니다.

```cpp
if (!createBuffer(bufferSize, inputA)
    || !createBuffer(bufferSize, inputB)
    || !createBuffer(bufferSize, output))
{
    return false;
}
```

버퍼를 만들었으니 기록할 값을 준비하겠습니다. `if`문 아래에 입력 배열 두 개와 출력 초기값 배열을 만듭니다.

```cpp
std::array<float, elementCount> valuesA{};
std::array<float, elementCount> valuesB{};
const std::array<float, elementCount> initialOutput{};
```

`{}`로 초기화한 원소는 모두 0입니다. 출력 배열은 이대로 두고, 입력 A에는 0부터 7까지, B에는 10부터 17까지 넣겠습니다.

```cpp
for (std::uint32_t i = 0; i < elementCount; ++i)
{
    valuesA[i] = static_cast<float>(i);
    valuesB[i] = 10.0f + static_cast<float>(i);
}
```

값을 준비했으니 반복문 다음에서 각 배열을 해당 버퍼에 기록하고 확인합니다.

```cpp
if (!writeAndVerifyBuffer(inputA, valuesA, "Input A")
    || !writeAndVerifyBuffer(inputB, valuesB, "Input B")
    || !writeAndVerifyBuffer(output, initialOutput, "Output (initial)"))
{
    return false;
}
```

각 호출은 데이터를 기록하고 다시 읽어 비교합니다. 모두 성공했다면 함수 마지막에서 준비가 끝났음을 출력합니다.

```cpp
std::cout << "Host-visible storage buffers ready: 3 x " << bufferSize << " bytes.\n"
          << "CPU write/readback verified. No GPU dispatch yet.\n";
return true;
```

## 초기화 순서에 연결하기

`createStorageBuffers()`가 완성됐으니 실제 초기화에 연결하겠습니다. 버퍼와 메모리는 논리적 디바이스를 통해 생성하므로, `initVulkan()`의 마지막에 호출을 추가합니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice()
        && createStorageBuffers();
}
```

버퍼 멤버를 `device` 뒤에 선언했으므로 버퍼와 메모리가 디바이스보다 먼저 정리됩니다. 다음 장에서는 이 버퍼들을 셰이더에 어떻게 연결할지 정하는 디스크립터 레이아웃을 만들겠습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/05-buffers-memory
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target buffers_memory_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target buffers_memory_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target buffers_memory_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target buffers_memory_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채우면 메모리 요구사항에 이어 다음 값이 출력됩니다. 메모리 유형 인덱스와 정렬 크기는 장치에 따라 다릅니다.

```text
Input A: 0 1 2 3 4 5 6 7
Input B: 10 11 12 13 14 15 16 17
Output (initial): 0 0 0 0 0 0 0 0
Host-visible storage buffers ready: 3 x 32 bytes.
CPU write/readback verified. No GPU dispatch yet.
Buffers and memory destroyed.
```

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Vertex buffer creation](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html)을 컴퓨트용 storage buffer의 할당·매핑 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.
