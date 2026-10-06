# 05. Buffer와 memory

[이전: Logical device와 compute queue](../04-logical-device/README-kr.md) · [다음: Descriptor layout](../06-descriptor-layout/README-kr.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

Storage buffer를 생성하고, 메모리를 할당·바인딩한 뒤 CPU에서 데이터를 기록합니다.
다시 매핑해 같은 값을 읽는 것으로 기록 결과를 확인합니다.

벡터 덧셈에 사용할 입력 A, 입력 B, 출력 버퍼를 준비합니다.
각 버퍼에는 `float` 값 8개를 저장합니다. 이 장의 출력 버퍼에는 초기값 `0`만 들어 있습니다.
GPU 덧셈은 dispatch와 동기화를 추가하는 11장에서 실행합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README-kr.md)을 완료한 환경에서 실행합니다.
C++20, CMake 3.24 이상, Vulkan SDK와 로더·GPU의 Vulkan 1.3 지원이 필요합니다.
02장에서 사용하는 validation layer도 설치되어 있어야 합니다.

저장소 루트에서 이 장의 폴더로 이동합니다.

```sh
cd examples/05-buffers-memory
```

Windows의 Visual Studio 생성기에서는 다음 명령을 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target buffers_memory_example
.\build\Debug\buffers_memory_example.exe
```

Linux의 Makefiles 또는 Ninja 생성기에서는 다음 명령을 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target buffers_memory_example
./build/buffers_memory_example
```

이 폴더는 04장에 버퍼 초기화를 추가한 독립 빌드 예제입니다.
전체 코드는 [main.cpp](main.cpp)에 있습니다.

## 실행 흐름

`initVulkan()`의 마지막에 `createStorageBuffers()`를 추가합니다.

```text
main → run → initVulkan
  → createInstance
  → setupDebugMessenger
  → pickPhysicalDevice
  → createLogicalDevice
  → createStorageBuffers
    → createBuffer × 3
      → 버퍼 생성
      → 메모리 요구 사항 조회
      → findMemoryType
      → 메모리 할당·바인딩
    → CPU 데이터 기록
    → 다시 매핑해 기록한 값 확인
```

| 추가 함수·멤버 | 역할 |
|---|---|
| `createStorageBuffers()` | 세 버퍼 생성과 CPU 데이터 초기화 |
| `createBuffer()` | 버퍼 하나의 생성·할당·바인딩 |
| `findMemoryType()` | 버퍼와 요청 속성에 맞는 메모리 유형 선택 |
| `writeAndVerifyBuffer()` | CPU 배열 복사 후 다시 읽어 비교 |
| `BufferResource` | 버퍼와 그 메모리의 수명 관리 |
| `inputA`, `inputB`, `output` | 입력 두 개와 출력 하나 |
| `elementCount`, `bufferSize` | 원소 개수와 버퍼의 바이트 크기 |

## 1. Buffer와 DeviceMemory 구분

`vk::Buffer`는 바이트 크기와 사용 목적을 가진 리소스입니다. 버퍼를 생성하는 것만으로
데이터를 저장할 메모리가 할당되지는 않습니다. 별도로 `vk::DeviceMemory`를 할당하고
버퍼에 연결해야 합니다.

```text
Buffer 생성 → 메모리 요구 사항 조회 → Memory 할당 → Buffer에 바인딩
```

세 버퍼를 같은 절차로 만들기 위해 버퍼와 메모리를 한 구조체에 둡니다.

```cpp
struct BufferResource
{
    vk::raii::DeviceMemory memory = nullptr;
    vk::raii::Buffer buffer = nullptr;
};

static constexpr std::uint32_t elementCount = 8;
static constexpr vk::DeviceSize bufferSize = sizeof(float) * elementCount;

BufferResource inputA;
BufferResource inputB;
BufferResource output;
```

메모리를 버퍼보다 먼저 선언합니다. 멤버는 선언의 역순으로 소멸하므로 버퍼를 해제한 뒤
메모리를 해제합니다. 세 `BufferResource`는 `device` 다음에 선언하여 논리 장치보다
먼저 정리되게 합니다. 초기화 함수가 끝난 뒤에도 버퍼와 메모리는 클래스 멤버로 유지됩니다.

## 2. 버퍼 생성

`createBuffer()`에서 버퍼의 크기와 용도를 지정합니다.

```cpp
vk::BufferCreateInfo bufferInfo{};
bufferInfo.size = size;
bufferInfo.usage = vk::BufferUsageFlagBits::eStorageBuffer;
bufferInfo.sharingMode = vk::SharingMode::eExclusive;

resource.buffer = vk::raii::Buffer(device, bufferInfo); // C API: vkCreateBuffer
```

`eStorageBuffer`는 이 버퍼를 셰이더의 storage buffer로 사용할 수 있게 합니다.
CPU 접근 가능 여부를 정하는 플래그는 아닙니다. CPU 접근 속성은 메모리를 선택할 때 정합니다.
셰이더에서 이 버퍼에 접근하기 위한 descriptor 연결은 06·07장에서 추가합니다.

`size`는 바이트 단위입니다. 이 예제는 32비트 `float` 8개이므로 버퍼 하나의 크기는
32바이트입니다. `eExclusive`는 한 시점에 한 큐 패밀리가 버퍼를 소유하는 방식입니다.
하나의 compute 큐 패밀리를 사용할 것이므로 패밀리 공유 목록은 필요하지 않습니다.

## 3. 메모리 요구 사항 조회

버퍼를 만든 뒤 드라이버가 요구하는 메모리 조건을 조회합니다.

```cpp
const auto requirements = resource.buffer.getMemoryRequirements(); // C API: vkGetBufferMemoryRequirements
```

| 필드 | 의미 |
|---|---|
| `size` | 바인딩에 필요한 메모리 크기. 버퍼의 요청 크기보다 클 수 있음 |
| `alignment` | 바인딩 시작 오프셋이 만족해야 하는 정렬 단위 |
| `memoryTypeBits` | 이 버퍼에 바인딩할 수 있는 메모리 유형의 비트 마스크 |

`alignment`는 시작 오프셋 자체가 아닙니다. 시작 오프셋이 이 값의 배수여야 한다는 조건입니다.
메모리 크기에는 `bufferSize`를 그대로 넣지 않고 `requirements.size`를 사용합니다.
[VkMemoryRequirements](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryRequirements.html)를 참고합니다.

## 4. 메모리 유형 선택

물리 장치는 여러 memory type과 memory heap을 제공합니다. Heap은 메모리 자원을 나타내며,
각 type에는 소속 heap의 인덱스와 접근 속성이 있습니다. 같은 heap에 서로 다른 속성의
type이 연결될 수 있습니다.

이 장은 CPU가 직접 접근하는 단계를 먼저 익히므로 다음 속성을 함께 요청합니다.

```cpp
const auto properties = vk::MemoryPropertyFlagBits::eHostVisible
    | vk::MemoryPropertyFlagBits::eHostCoherent;
```

- `eHostVisible`: CPU 주소 공간으로 매핑할 수 있습니다.
- `eHostCoherent`: 매핑된 메모리의 host/device 가시성을 위해 명시적인 flush·invalidate가 필요하지 않습니다.

`findMemoryType()`에서는 두 조건을 모두 검사합니다.

```cpp
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
```

첫 조건은 이 버퍼에 사용할 수 있는 type인지 검사합니다. 두 번째 조건은 요청한 속성을
모두 가지고 있는지 검사합니다. `&` 결과가 단순히 0이 아닌지만 검사하면 속성 중 하나만
있는 type도 통과하므로 `== properties`를 사용합니다.

일치하는 type이 없으면 오류를 출력하고 `false`를 반환합니다. 선택한 type에
`eDeviceLocal`이 함께 있을 수도 있습니다. `HOST_VISIBLE`이 시스템 RAM만을 뜻하거나,
`DEVICE_LOCAL`이 CPU 접근 불가를 뜻하는 것은 아닙니다.

## 5. 할당과 바인딩

선택한 type과 메모리 요구 크기를 사용해 메모리를 할당합니다.

```cpp
vk::MemoryAllocateInfo allocationInfo{};
allocationInfo.allocationSize = requirements.size;
allocationInfo.memoryTypeIndex = memoryTypeIndex;

resource.memory = vk::raii::DeviceMemory(device, allocationInfo); // C API: vkAllocateMemory
resource.buffer.bindMemory(*resource.memory, 0); // C API: vkBindBufferMemory
```

이 예제는 버퍼마다 메모리를 하나씩 할당하고 시작 오프셋을 `0`으로 둡니다.
`0`은 요구 정렬 단위의 배수입니다. 버퍼 핸들과 메모리 핸들을 연결하는 호출이 `bindMemory()`입니다.
이 호출은 CPU 배열의 데이터를 복사하지 않습니다.

할당한 메모리는 초기 데이터가 보장되지 않으므로 입력뿐 아니라 출력 버퍼도 직접 초기화합니다.
많은 버퍼를 사용하는 애플리케이션의 메모리 부분 할당은 이 장의 범위에서 제외합니다.

## 6. 매핑과 CPU 데이터 기록

`writeAndVerifyBuffer()`는 메모리를 매핑하고 CPU 배열을 복사합니다.

```cpp
void* mapped = resource.memory.mapMemory(0, bufferSize); // C API: vkMapMemory
std::memcpy(mapped, values.data(), static_cast<std::size_t>(bufferSize));
resource.memory.unmapMemory(); // C API: vkUnmapMemory
```

`mapMemory()`는 CPU가 접근할 주소를 반환합니다. `std::memcpy()`가 그 주소로 실제 데이터를
씁니다. `unmapMemory()`는 CPU 매핑을 해제하며, 버퍼나 메모리 할당을 해제하지 않습니다.
매핑을 해제한 포인터는 더 이상 사용하면 안 됩니다.

복사하는 크기는 유효 데이터 크기인 `bufferSize`입니다. 메모리 할당 크기가 더 크더라도
CPU 배열의 끝을 넘어 `requirements.size`만큼 복사하면 안 됩니다.

기록한 값은 다시 매핑해 별도의 CPU 배열로 복사하고 원본과 비교합니다.

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

이 비교는 CPU가 쓴 값의 보존 여부를 확인합니다. 아직 GPU 작업을 제출하지 않았으므로
GPU 계산 결과를 검사하는 코드가 아닙니다.

### Coherent와 동기화

이 예제는 `HOST_COHERENT` 메모리를 선택하여 `flushMappedMemoryRanges()`와
`invalidateMappedMemoryRanges()`를 생략합니다. Non-coherent 메모리는 host 쓰기 후 flush,
device 쓰기 결과를 host에서 읽기 전 invalidate가 필요하며 범위 정렬 조건도 따라야 합니다.
`unmapMemory()`는 flush의 대체 수단이 아닙니다.

Coherent 속성이 CPU와 GPU의 실행 순서를 맞춰 주지는 않습니다. `mapMemory()`도 GPU 작업의
완료를 기다리지 않습니다. GPU가 같은 버퍼를 사용하기 시작하면 데이터 의존성과 완료 대기를
별도로 처리해야 합니다. 이 내용은 11장에서 추가합니다.
[메모리 속성](https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html)과
[매핑 규칙](https://docs.vulkan.org/refpages/latest/refpages/source/vkMapMemory.html)을 참고합니다.

## 완료 기준

앞 장의 출력에 이어 다음 값과 검사 결과가 출력되어야 합니다.

```text
Input A: 0 1 2 3 4 5 6 7
Input B: 10 11 12 13 14 15 16 17
Output (initial): 0 0 0 0 0 0 0 0
Host-visible storage buffers ready: 3 x 32 bytes.
CPU write/readback verified. No GPU dispatch yet.
```

프로그램의 종료 코드는 `0`이어야 합니다. 02장의 진단 경로 확인 메시지 외에
Vulkan validation 오류가 없어야 합니다. 메모리 type이나 할당 크기 같은 조회값은
실행 환경에 따라 달라질 수 있습니다.

다음 장에서는 세 버퍼를 셰이더에서 구분할 set·binding과 descriptor 종류를 정의합니다.
Device-local 연산용 버퍼와 staging buffer를 분리하는 단계는 12장에서 다룹니다.

## 참고 자료와 라이선스

- [Khronos Vulkan Tutorial — Vertex buffer creation](https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/01_Vertex_buffer_creation.html)
- [vkBindBufferMemory](https://docs.vulkan.org/refpages/latest/refpages/source/vkBindBufferMemory.html)
- [Vulkan Guide: Memory Allocation](https://docs.vulkan.org/guide/latest/memory_allocation.html)
- [Vulkan-Hpp RAII 안내](https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/docs/VkRaiiProgrammingGuide.md)

이 문서는 Khronos Vulkan Tutorial contributors의 위 Vertex buffer creation 장을
번역한 학습 노트를 바탕으로 컴퓨트용 storage buffer의 할당·매핑에 맞게 각색했습니다.
정점 데이터를 벡터 입력·출력으로 바꾸고 CPU 기록·재매핑 검증을 추가했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.
