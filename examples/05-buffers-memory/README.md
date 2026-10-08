# 05. Buffer와 Memory

[이전: Logical device](../04-logical-device/README.md) · [다음: Descriptor layout](../06-descriptor-layout/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

Vulkan에서 버퍼(Buffer)는 데이터를 담는 자원입니다. 다만 버퍼를 생성하는 것만으로 저장 공간까지 할당되지는 않습니다. 버퍼가 요구하는 메모리 조건을 확인하고, 메모리를 할당한 뒤 버퍼에 연결해야 합니다.

이번에는 컴퓨트 셰이더에서 사용할 입력 버퍼 두 개와 출력 버퍼 하나를 만들어 보겠습니다. 먼저 CPU에서 값을 기록하고 다시 읽어 확인합니다. GPU 연산은 이후 장에서 연결합니다.

[main.cpp](main.cpp)에는 이전 장에서 완성한 코드가 들어 있습니다. 아래 설명을 따라 TODO를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/completed/05-buffers-memory.cpp`에서 확인할 수 있습니다.

## 버퍼를 담을 구조체 추가하기

파일 위쪽의 헤더 TODO를 다음 코드로 바꿉니다. `std::array`는 입력값과 읽어 온 값을 담고, `std::size_t`는 복사할 바이트 수를 나타낼 때 사용합니다.

```cpp
#include <array>
#include <cstddef>
```

`private` 안에서 `validationLayer` 다음의 TODO를 아래 코드로 바꿉니다.

```cpp
static_assert(sizeof(float) == 4);
static constexpr std::uint32_t elementCount = 8;
static constexpr vk::DeviceSize bufferSize = sizeof(float) * elementCount;

struct BufferResource
{
    vk::raii::DeviceMemory memory = nullptr;
    vk::raii::Buffer buffer = nullptr;
};
```

각 버퍼에는 `float` 값 8개를 담습니다. `bufferSize`는 원소 하나의 크기에 개수를 곱한 바이트 크기입니다.

`BufferResource`는 버퍼와 그 버퍼에 연결할 메모리를 함께 보관합니다. 멤버가 선언의 역순으로 소멸하므로 `memory`를 먼저 선언합니다. 이렇게 하면 버퍼를 해제한 뒤 연결된 메모리를 해제할 수 있습니다.

`computeQueue` 아래의 TODO에는 입력 두 개와 출력 하나를 추가합니다.

```cpp
BufferResource inputA;
BufferResource inputB;
BufferResource output;
```

## 버퍼 생성하기

클래스 끝의 함수 TODO 위치에 `createBuffer()`를 추가합니다. 크기와 결과를 담을 `BufferResource`를 전달받도록 하겠습니다.

```cpp
bool createBuffer(vk::DeviceSize size, BufferResource& resource)
{
}
```

이 함수 안에 버퍼 생성 정보를 작성합니다.

```cpp
vk::BufferCreateInfo bufferInfo{};
bufferInfo.size = size;
bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
bufferInfo.sharingMode = vk::SharingMode::eExclusive;
resource.buffer = vk::raii::Buffer(device, bufferInfo); // C API: vkCreateBuffer
```

`size`는 버퍼에 담을 데이터의 바이트 크기입니다. `usage`에는 컴퓨트 셰이더에서 데이터를 읽고 쓸 수 있는 `eStorageBuffer`를 지정합니다. 하나의 큐 패밀리에서 사용할 것이므로 `sharingMode`는 `eExclusive`로 둡니다.

여기까지는 버퍼 객체만 만들었습니다. 이제 버퍼가 사용할 메모리를 준비해야 합니다.

## 메모리 요구사항 확인하기

버퍼 생성 코드 아래에 다음 코드를 추가합니다.

```cpp
const auto requirements = resource.buffer.getMemoryRequirements(); // C API: vkGetBufferMemoryRequirements
const auto properties = vk::MemoryPropertyFlagBits::eHostVisible
    | vk::MemoryPropertyFlagBits::eHostCoherent;
std::uint32_t memoryTypeIndex = 0;
if (!findMemoryType(requirements.memoryTypeBits, properties, memoryTypeIndex))
{
    return false;
}
```

`getMemoryRequirements()`가 돌려주는 `size`는 실제로 할당해야 하는 크기이고, `alignment`는 연결할 오프셋의 정렬 조건입니다. `memoryTypeBits`는 이 버퍼에 사용할 수 있는 메모리 유형들을 비트로 나타냅니다.

버퍼의 조건 외에 프로그램이 원하는 메모리 속성도 지정해야 합니다. `eHostVisible`은 CPU에서 매핑할 수 있다는 뜻이고, `eHostCoherent`는 호스트 캐시를 명시적으로 flush하거나 invalidate할 필요가 없다는 뜻입니다. GPU 작업의 완료까지 보장하는 속성은 아닙니다.

## 사용할 메모리 유형 찾기

`createBuffer()` 아래에 `findMemoryType()`을 추가합니다.

```cpp
bool findMemoryType(std::uint32_t typeFilter, vk::MemoryPropertyFlags properties,
                    std::uint32_t& typeIndex)
{
    const auto memoryProperties = physicalDevice.getMemoryProperties(); // C API: vkGetPhysicalDeviceMemoryProperties
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const bool allowed = (typeFilter & (1u << i)) != 0;
        const bool hasProperties =
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (allowed && hasProperties)
        {
            typeIndex = i;
            return true;
        }
    }

    std::cerr << "No compatible HOST_VISIBLE | HOST_COHERENT memory type found.\n";
    return false;
}
```

먼저 물리적 디바이스가 제공하는 메모리 유형을 조회합니다. `(typeFilter & (1u << i)) != 0`은 버퍼가 `i`번 유형을 허용하는지 확인합니다. 다음 조건은 해당 유형이 요청한 속성을 모두 포함하는지 확인합니다.

두 조건을 모두 만족하면 인덱스를 `typeIndex`에 저장하고 `true`를 반환합니다. 찾지 못하면 `false`를 반환하여 버퍼 초기화를 중단합니다.

## 메모리 할당하고 연결하기

다시 `createBuffer()`로 돌아가서, `findMemoryType()`의 실패를 검사하는 `if`문 아래에 다음 코드를 추가합니다.

```cpp
vk::MemoryAllocateInfo allocationInfo{};
allocationInfo.allocationSize = requirements.size;
allocationInfo.memoryTypeIndex = memoryTypeIndex;
resource.memory = vk::raii::DeviceMemory(device, allocationInfo); // C API: vkAllocateMemory
resource.buffer.bindMemory(*resource.memory, 0); // C API: vkBindBufferMemory

std::cout << "Buffer memory: size " << requirements.size
          << ", alignment " << requirements.alignment
          << ", type " << memoryTypeIndex << '\n';
return true;
```

할당 크기에는 처음 요청한 `size` 대신 드라이버가 알려 준 `requirements.size`를 사용합니다. `memoryTypeIndex`에는 앞에서 찾은 메모리 유형을 넣습니다.

메모리를 할당한 다음 `bindMemory()`로 버퍼에 연결합니다. 두 번째 인수는 할당된 메모리 안에서 버퍼가 시작할 오프셋입니다. 여기서는 버퍼마다 별도의 메모리를 할당하므로 0을 사용하며, 0은 버퍼의 정렬 조건도 만족합니다.

## CPU에서 값을 기록하고 확인하기

`findMemoryType()` 아래에 다음 함수를 추가합니다. 버퍼에 기록할 배열과 출력에 사용할 이름을 받도록 합니다.

```cpp
bool writeAndVerifyBuffer(BufferResource& resource,
                          const std::array<float, elementCount>& values,
                          const char* label)
{
}
```

함수 안에서 먼저 메모리를 매핑하고 값을 복사합니다.

```cpp
void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(mapped, values.data(), static_cast<std::size_t>(bufferSize));
resource.memory.unmapMemory(); // C API: vkUnmapMemory
```

`mapMemory()`는 CPU가 접근할 수 있는 주소를 반환합니다. 그 주소로 배열의 바이트를 복사한 뒤 `unmapMemory()`로 매핑을 해제합니다. 메모리를 해제하는 것은 아니므로 저장한 값은 남아 있습니다.

그 아래에서 다시 매핑하여 값을 읽고, 원래 배열과 비교합니다.

```cpp
std::array<float, elementCount> readback{};
mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(readback.data(), mapped, static_cast<std::size_t>(bufferSize));
resource.memory.unmapMemory(); // C API: vkUnmapMemory
if (readback != values)
{
    std::cerr << label << ": CPU data verification failed.\n";
    return false;
}
```

이어서 읽은 값을 출력하고 성공 여부를 반환합니다.

```cpp
std::cout << label << ':';
for (float value : readback)
{
    std::cout << ' ' << value;
}
std::cout << '\n';
return true;
```

이 검사는 CPU가 기록한 값을 CPU에서 다시 읽는 과정입니다. 아직 GPU에 명령을 제출하지 않았으므로 여기에는 큐 제출이나 대기 코드가 필요하지 않습니다.

## 입력과 출력 버퍼 준비하기

`createBuffer()` 앞에 `createStorageBuffers()`를 추가합니다.

```cpp
bool createStorageBuffers()
{
}
```

먼저 함수 안에서 앞서 만든 `createBuffer()`를 호출하여 세 버퍼를 생성합니다.

```cpp
if (!createBuffer(bufferSize, inputA)
    || !createBuffer(bufferSize, inputB)
    || !createBuffer(bufferSize, output))
{
    return false;
}
```

그 아래에는 입력값을 준비합니다. A에는 0부터 7까지, B에는 10부터 17까지 담고, 출력은 0으로 초기화합니다.

```cpp
std::array<float, elementCount> valuesA{};
std::array<float, elementCount> valuesB{};
const std::array<float, elementCount> initialOutput{};
for (std::uint32_t i = 0; i < elementCount; ++i)
{
    valuesA[i] = static_cast<float>(i);
    valuesB[i] = 10.0f + static_cast<float>(i);
}
```

마지막으로 각 배열을 해당 버퍼에 기록하고 확인합니다.

```cpp
if (!writeAndVerifyBuffer(inputA, valuesA, "Input A")
    || !writeAndVerifyBuffer(inputB, valuesB, "Input B")
    || !writeAndVerifyBuffer(output, initialOutput, "Output (initial)"))
{
    return false;
}

std::cout << "Host-visible storage buffers ready: 3 x " << bufferSize << " bytes.\n"
          << "CPU write/readback verified. No GPU dispatch yet.\n";
return true;
```

## 초기화 순서에 연결하기

`initVulkan()`을 다음과 같이 바꿉니다. 버퍼와 메모리는 논리적 디바이스를 통해 생성하므로 `createLogicalDevice()` 다음에 연결합니다.

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

마지막으로 `main()`의 TODO와 기존 출력문을 다음 출력문으로 바꿉니다.

```cpp
std::cout << "Buffers, memory, device, debug messenger and instance destroyed.\n";
```

버퍼 멤버를 `device` 뒤에 선언했으므로 프로그램이 끝날 때 버퍼와 메모리가 디바이스보다 먼저 해제됩니다.

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
```

아직 채우지 않은 시작 코드는 이전 장처럼 논리적 디바이스와 큐 생성까지만 실행합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Vertex buffer creation](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html)을 컴퓨트용 storage buffer의 할당·매핑 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.
