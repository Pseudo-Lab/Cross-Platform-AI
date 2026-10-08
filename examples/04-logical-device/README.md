# 04. Logical device와 Queue

[이전: Physical devices](../03-physical-devices/README.md) · [다음: Buffers and memory](../05-buffers-memory/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

사용할 물리적 디바이스를 선택했다면, 이제 해당 하드웨어에 작업을 요청할 논리적 디바이스(Logical Device)를 생성해야 합니다. 인스턴스를 만들 때처럼 사용할 기능을 구조체에 담고, 함께 생성할 큐(Queue)를 지정하게 됩니다.

[main.cpp](main.cpp)에는 이전 장에서 완성한 코드가 들어 있습니다. 아래 설명을 따라 TODO를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/completed/04-logical-device.cpp`에서 확인할 수 있습니다.

## 논리적 디바이스 생성 함수 추가하기

먼저 `computeQueueFamilyIndex` 아래의 TODO를 다음 멤버 변수로 바꿉니다.

```cpp
vk::raii::Device device = nullptr;
vk::raii::Queue computeQueue = nullptr;
```

`device`는 논리적 디바이스를, `computeQueue`는 명령을 제출할 큐를 담습니다. 아직 생성하지 않았으므로 둘 다 `nullptr`로 초기화합니다.

클래스 끝의 TODO에는 다음 함수를 추가합니다. 이제 이 함수의 빈 부분을 순서대로 채워 보겠습니다.

```cpp
bool createLogicalDevice()
{
}
```

## 생성할 큐 지정하기

`createLogicalDevice()` 안에 `vk::DeviceQueueCreateInfo`를 작성합니다.

```cpp
const float queuePriority = 1.0f;
vk::DeviceQueueCreateInfo queueCreateInfo{};
queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
queueCreateInfo.queueCount = 1;
queueCreateInfo.pQueuePriorities = &queuePriority;
```

`queueFamilyIndex`에는 이전 장에서 찾은 컴퓨트 큐 패밀리의 인덱스를 넣습니다. `queueCount`가 1이므로 이 패밀리에서 큐 하나를 요청합니다.

큐가 하나여도 우선순위는 지정해야 합니다. `pQueuePriorities`는 요청한 큐 수만큼의 우선순위 값을 가리키며, 각 값은 0.0에서 1.0 사이입니다. 여기서는 `queuePriority` 하나의 주소를 전달합니다.

## 디바이스 생성 정보 설정하기

큐 생성 정보 아래에 사용할 기능과 디바이스 생성 정보를 추가합니다.

```cpp
vk::PhysicalDeviceFeatures enabledFeatures{};
vk::DeviceCreateInfo createInfo{};
createInfo.queueCreateInfoCount = 1;
createInfo.pQueueCreateInfos = &queueCreateInfo;
createInfo.pEnabledFeatures = &enabledFeatures;
```

`vk::PhysicalDeviceFeatures`는 선택적으로 활성화할 기능을 담습니다. 여기서는 별도의 선택 기능이 필요하지 않으므로 `{}`로 모두 비활성화합니다. 이전 장에서 조회한 기능도 디바이스 생성 시 요청해야 활성화됩니다.

`queueCreateInfoCount`는 큐 생성 정보 구조체의 개수입니다. 앞에서 한 큐 패밀리의 생성 정보만 작성했으므로 1을 넣고, `pQueueCreateInfos`에 그 주소를 전달합니다. 창에 이미지를 표시하지 않으므로 swapchain 같은 디바이스 확장은 추가하지 않습니다.

## 디바이스 생성하고 큐 핸들 가져오기

이어서 논리적 디바이스를 만들고 큐 핸들을 가져옵니다.

```cpp
device = vk::raii::Device(physicalDevice, createInfo); // C API: vkCreateDevice
computeQueue = vk::raii::Queue(device, computeQueueFamilyIndex, 0); // C API: vkGetDeviceQueue
std::cout << "Logical device created.\n"
          << "Compute queue ready: family " << computeQueueFamilyIndex << ", queue 0.\n";
return true;
```

`vk::raii::Device`에는 선택한 물리적 디바이스와 생성 정보를 전달합니다. 이때 요청한 큐도 함께 생성됩니다.

`vk::raii::Queue`는 이미 생성된 큐의 핸들을 가져옵니다. 두 번째 인수는 큐 패밀리 인덱스이고, 세 번째 인수는 그 패밀리 안의 큐 인덱스입니다. 큐 하나만 요청했으므로 인덱스는 0입니다.

## 초기화 순서에 연결하기

`initVulkan()`을 다음과 같이 바꿉니다. 물리적 디바이스를 선택한 다음 논리적 디바이스를 생성합니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice();
}
```

앞 단계가 `false`를 반환하면 `&&` 뒤의 함수는 실행되지 않습니다. 사용할 하드웨어를 찾지 못한 상태에서 논리적 디바이스를 만들지 않도록 하는 흐름입니다.

마지막으로 `main()`의 TODO와 기존 출력문을 다음 출력문으로 바꿉니다.

```cpp
std::cout << "Device, debug messenger and instance destroyed.\n";
```

`app`이 소멸하면 멤버는 선언의 역순으로 정리됩니다. 큐는 논리적 디바이스가 해제될 때 함께 정리되므로 별도의 큐 해제 함수를 호출하지 않습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/04-logical-device
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target logical_device_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target logical_device_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target logical_device_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target logical_device_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 물리적 디바이스 정보 뒤에 다음 메시지가 출력됩니다. 큐 패밀리 인덱스는 장치에 따라 다릅니다.

```text
Logical device created.
Compute queue ready: family 0, queue 0.
```

아직 채우지 않은 시작 코드는 이전 장처럼 물리적 디바이스 선택까지만 실행합니다.

---

이 문서는 [Khronos Vulkan Tutorial: Logical device and queues](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.
