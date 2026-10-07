# 04. Logical device와 compute queue

[이전: Physical device와 queue family](../03-physical-devices/README.md) · [다음: Buffer와 memory](../05-buffers-memory/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

선택한 물리 장치에서 논리 장치를 생성하고 compute queue를 가져옵니다.
03장에서 조회한 기능과 큐 정보를 장치 생성 요청에 어떻게 사용하는지 확인합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 완료한 환경에서 실행합니다.
C++20, CMake 3.24 이상, Vulkan SDK와 로더·GPU의 Vulkan 1.3 지원이 필요합니다.
02장에서 사용하는 validation layer도 설치되어 있어야 합니다.

저장소 루트에서 이 장의 폴더로 이동합니다.

```sh
cd examples/04-logical-device
```

Windows의 Visual Studio 생성기에서는 다음 명령을 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target logical_device_example
.\build\Debug\logical_device_example.exe
```

Linux의 Makefiles 또는 Ninja 생성기에서는 다음 명령을 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target logical_device_example
./build/logical_device_example
```

이 폴더는 03장에 필요한 함수와 멤버를 추가한 독립 빌드 예제입니다.
전체 코드는 [main.cpp](main.cpp)에 있습니다.

## 실행 흐름

`ComputeApplication`에 `createLogicalDevice()`를 추가합니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice();
}
```

앞 단계가 실패하면 다음 함수는 실행하지 않습니다. `false`는 `run()`을 거쳐
`main()`으로 전달되고 프로그램은 종료 코드 1을 반환합니다.

| 추가 함수·멤버 | 역할 |
|---|---|
| `createLogicalDevice()` | 큐와 기능을 요청하고 논리 장치 생성 |
| `device` | 논리 장치의 수명 관리 |
| `computeQueue` | 작업을 제출할 큐 핸들 보관 |

## 1. Physical device와 logical device

`physicalDevice`는 선택한 Vulkan 구현의 장치 핸들입니다. 이 핸들로 기능·한계·큐 패밀리를
조회합니다. `device`는 그 물리 장치에서 사용할 큐와 기능을 지정해 만든 논리 장치입니다.
이후 버퍼와 파이프라인 같은 장치 리소스는 이 논리 장치를 통해 만듭니다.

```cpp
vk::raii::Device device = nullptr;
vk::raii::Queue computeQueue = nullptr;
```

두 핸들은 클래스 멤버로 둡니다. `createLogicalDevice()`가 끝난 뒤에도 다음 장에서
버퍼를 만들고 작업을 제출할 때 사용할 수 있어야 합니다.

## 2. 생성할 큐 지정

03장의 `computeQueueFamilyIndex`는 compute 연산을 지원하고 `queueCount`가 1 이상인
큐 패밀리의 인덱스입니다. 이 패밀리에서 큐 하나를 요청합니다.

```cpp
const float queuePriority = 1.0f;
vk::DeviceQueueCreateInfo queueCreateInfo{};
queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
queueCreateInfo.queueCount = 1;
queueCreateInfo.pQueuePriorities = &queuePriority;
```

| 필드 | 의미 |
|---|---|
| `queueFamilyIndex` | 사용할 큐 패밀리 |
| `queueCount` | 해당 패밀리에서 생성할 큐 개수 |
| `pQueuePriorities` | 각 큐의 우선순위 배열 |

큐가 하나여도 우선순위는 지정해야 합니다. 값의 범위는 `0.0f`부터 `1.0f`까지이며,
`1.0f`가 GPU를 독점하거나 일정한 실행 시간을 보장하지는 않습니다.
우선순위 배열의 길이는 `queueCount`와 같아야 합니다.

**큐 패밀리 인덱스와 큐 인덱스는 다릅니다.** 패밀리 인덱스는 물리 장치의 큐 패밀리를
고르고, 큐 인덱스는 그 패밀리에서 요청한 큐 중 하나를 고릅니다.
이 예제에서 큐 인덱스는 `0`입니다.

## 3. 기능 조회와 활성화 구분

03장의 `getFeatures()`는 지원 여부를 조회합니다. 지원되는 모든 기능이 자동으로 켜지는
것은 아닙니다. 선택 기능을 사용하려면 지원 여부를 확인하고 장치 생성 시
필요한 항목을 요청해야 합니다.

이 예제의 큐 생성과 이후의 32비트 `float` 벡터 덧셈에는 추가 선택 기능이 필요하지 않습니다.
따라서 모든 필드가 `false`인 요청 구조체를 사용합니다.

```cpp
vk::PhysicalDeviceFeatures enabledFeatures{};
```

예를 들어 03장에서 `shaderFloat64`가 지원된다고 출력되어도 이 코드가 64비트 셰이더
연산을 활성화하는 것은 아닙니다. 조회 결과를 그대로 요청 구조체에 복사하지 않습니다.

API 버전과 선택 기능은 별개입니다. Vulkan 1.3을 요청하는 것만으로 모든 feature 비트가
켜지지 않으며, 기능 요청을 비워 두었다고 API 버전이 1.0으로 낮아지지도 않습니다.
추가 feature 구조체가 필요한 경우에는 `pNext`로 연결합니다. `vk::StructureChain`은
그 연결을 관리하는 Vulkan-Hpp 도구이며, 모든 장치 생성에서 필요한 것은 아닙니다.
[기능 활성화 규칙](https://docs.vulkan.org/guide/latest/enabling_features.html)을 참고합니다.

## 4. 논리 장치 생성

큐 요청과 기능 요청을 `vk::DeviceCreateInfo`에 연결합니다.

```cpp
vk::DeviceCreateInfo createInfo{};
createInfo.queueCreateInfoCount = 1;
createInfo.pQueueCreateInfos = &queueCreateInfo;
createInfo.pEnabledFeatures = &enabledFeatures;

device = vk::raii::Device(physicalDevice, createInfo); // C API: vkCreateDevice
```

`queueCreateInfoCount`는 큐 개수가 아니라 큐 생성 정보 구조체의 개수입니다.
한 구조체 안의 `queueCount`가 해당 패밀리에서 요청할 큐 개수를 정합니다.
`queuePriority`, `queueCreateInfo`, `enabledFeatures`는 생성 호출이 끝날 때까지 유효합니다.

이 단계의 compute 경로에는 추가 device extension이 필요하지 않습니다.
`enabledExtensionCount`와 `ppEnabledExtensionNames`는 기본값인 `0`, `nullptr`로 둡니다.
Instance에서 활성화한 `VK_EXT_debug_utils`와 device extension의 요청 위치를 구분합니다.

Validation layer는 02장의 Instance 생성 과정에서 활성화했습니다.
`DeviceCreateInfo`의 오래된 device layer 필드는 `0`, `nullptr`로 둡니다.
[장치 생성 구조체](https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceCreateInfo.html)의 정의를 참고합니다.

## 5. 큐 핸들 가져오기

큐는 논리 장치를 생성할 때 함께 만들어집니다. 생성 요청에 사용한 패밀리 인덱스와
큐 인덱스 `0`으로 그 핸들을 가져옵니다.

```cpp
computeQueue = vk::raii::Queue(device, computeQueueFamilyIndex, 0); // C API: vkGetDeviceQueue
```

이 호출은 큐를 하나 더 만드는 호출이 아닙니다. 요청하지 않은 패밀리나 큐 인덱스로
핸들을 가져올 수 없습니다. 큐를 여러 개 만들었다고 하드웨어에서 동시에 실행된다고
보장되는 것도 아닙니다.

`computeQueue`를 `device` 다음에 선언했으므로 C++ 객체는 큐 래퍼부터 소멸합니다.
큐에 별도의 `vkDestroyQueue` 함수는 없습니다. Vulkan 큐의 수명은 논리 장치에 속하며,
Device 소멸자가 `vkDestroyDevice`를 호출할 때 함께 끝납니다.

## 완료 기준

앞 장의 버전·확장·GPU 정보에 이어 다음 내용이 출력되어야 합니다.
`N`은 실행 환경에서 선택된 큐 패밀리 인덱스입니다.

```text
Logical device created.
Compute queue ready: family N, queue 0.
```

02장에서 넣은 진단 경로 확인 메시지 외에 Vulkan validation 오류가 없어야 하며,
프로그램의 종료 코드는 `0`이어야 합니다. 이 장에서 확인하는 것은 compute queue의 준비입니다.
GPU에 제출할 명령은 이후 장에서 작성합니다.

## 참고 자료와 라이선스

- [Khronos Vulkan Tutorial — Logical device and queues](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html)
- [VkDeviceQueueCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceQueueCreateInfo.html)
- [vkGetDeviceQueue](https://docs.vulkan.org/refpages/latest/refpages/source/vkGetDeviceQueue.html)
- [Vulkan-Hpp RAII 안내](https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/docs/VkRaiiProgrammingGuide.md)

이 문서는 Khronos Vulkan Tutorial contributors의 위 Logical device and queues 장을
번역한 학습 노트를 바탕으로 컴퓨트 큐 생성에 맞게 각색했습니다.
그래픽 전용 기능·확장 요청을 제외하고 기능 조회와 활성화의 구분을 보완했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.
