# 04. Logical device와 Queue

[이전: Physical devices](../03-physical-devices/README.md) · [다음: Buffers and memory](../05-buffers-memory/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서는 사용할 물리적 디바이스와 컴퓨트 큐 패밀리를 선택했습니다. 이제 그 장치에 작업을 요청할 논리적 디바이스(Logical device)를 만들 차례입니다. 이때 사용할 기능과 큐 개수를 지정하고, 생성된 큐의 핸들을 가져오겠습니다.

[main.cpp](main.cpp)를 열고 TODO 위치를 따라 작성해 보겠습니다. 완성된 코드는 공개 후 `examples/99-completed/04-logical-device.cpp`에서 확인할 수 있습니다.

## 디바이스와 큐를 보관할 자리 만들기

논리적 디바이스와 큐는 초기화가 끝난 뒤에도 사용합니다. `private` 영역의 `computeQueueFamilyIndex` 아래 TODO를 다음 멤버로 바꿉니다.

```cpp
vk::raii::Device device = nullptr;
vk::raii::Queue computeQueue = nullptr;
```

`device`는 논리적 디바이스를, `computeQueue`는 명령을 제출할 큐를 보관합니다. 아직 생성하지 않았으므로 두 객체 모두 `nullptr`로 초기화합니다.

이제 클래스 끝의 TODO 위치에 생성 함수의 틀을 추가합니다. 함수 안에서 큐 생성 정보부터 준비하겠습니다.

```cpp
bool createLogicalDevice()
{
}
```

## 생성할 큐 지정하기

03장에서 저장한 `computeQueueFamilyIndex`는 사용할 패밀리의 번호였습니다. 그 패밀리의 큐를 실제로 사용하려면 논리적 디바이스를 만들 때 큐를 요청해야 합니다. 우리는 큐 하나로 시작하겠습니다.

우선 요청할 큐의 우선순위를 정합니다. `createLogicalDevice()`의 첫 줄에 추가합니다.

```cpp
const float queuePriority = 1.0f;
```

우선순위는 0.0에서 1.0 사이의 값이며, 큐가 하나일 때도 지정해야 합니다.

이어서 어느 패밀리에서 큐를 몇 개 요청하는지 `vk::DeviceQueueCreateInfo`에 적습니다.

```cpp
vk::DeviceQueueCreateInfo queueCreateInfo{};
queueCreateInfo.queueFamilyIndex = computeQueueFamilyIndex;
queueCreateInfo.queueCount = 1;
queueCreateInfo.pQueuePriorities = &queuePriority;
```

`queueFamilyIndex`는 이전 장에서 찾은 패밀리, `queueCount`는 그 패밀리에서 요청할 큐 개수입니다. `pQueuePriorities`에는 요청한 큐 수만큼의 우선순위가 필요합니다. 여기서는 하나만 요청하므로 앞에서 만든 `queuePriority`의 주소를 전달합니다.

## 사용할 기능과 생성 정보 연결하기

큐를 정했으니 장치에서 활성화할 기능도 지정하겠습니다. 이전 장의 `getFeatures()`는 지원 여부를 조회한 것이었습니다. 지원되는 기능이라도 선택적 기능은 디바이스를 만들 때 사용을 요청해야 합니다.

이번 실습에는 별도의 선택적 기능이 필요하지 않습니다. 큐 생성 정보 아래에 빈 기능 구조체를 추가합니다.

```cpp
vk::PhysicalDeviceFeatures enabledFeatures{};
```

`{}`로 초기화하면 이 구조체의 기능들은 모두 비활성화됩니다. 예를 들어 이전 장에서 `shaderFloat64`가 지원된다고 출력됐어도 여기서 요청하지 않으면 활성화되지 않습니다.

이제 논리적 디바이스의 생성 정보를 만들고, 준비한 큐 정보를 연결합니다.

```cpp
vk::DeviceCreateInfo createInfo{};
createInfo.queueCreateInfoCount = 1;
createInfo.pQueueCreateInfos = &queueCreateInfo;
```

`queueCreateInfoCount`는 큐 개수가 아니라 **큐 생성 정보 구조체의 개수**입니다. 한 패밀리에 대한 구조체를 하나 만들었으므로 1을 넣고, `pQueueCreateInfos`에 그 주소를 전달합니다.

그 아래에 활성화할 기능 구조체도 연결합니다.

```cpp
createInfo.pEnabledFeatures = &enabledFeatures;
```

창에 이미지를 표시하지 않는 컴퓨트 실습이므로 swapchain 같은 디바이스 확장은 추가하지 않습니다.

## 디바이스를 만들고 큐 가져오기

생성 정보가 준비됐습니다. 같은 함수 안에서 선택한 물리적 디바이스와 생성 정보를 전달해 논리적 디바이스를 만듭니다.

```cpp
device = vk::raii::Device(physicalDevice, createInfo); // C API: vkCreateDevice
```

이때 `queueCreateInfo`로 요청했던 큐도 함께 생성됩니다. 다만 프로그램에서 사용할 큐 핸들은 아직 가져오지 않았습니다. 다음 줄에서 그 핸들을 `computeQueue`에 저장합니다.

```cpp
computeQueue = vk::raii::Queue(device, computeQueueFamilyIndex, 0); // C API: vkGetDeviceQueue
```

두 번째 인수는 패밀리 인덱스, 세 번째 인수는 **그 패밀리 안에서의 큐 인덱스**입니다. 큐 하나를 요청했으므로 가져올 큐의 인덱스는 0입니다. 이 호출은 큐를 하나 더 만드는 것이 아니라 앞에서 생성한 큐를 가져옵니다.

함수 마지막에는 생성 결과를 출력하고 성공을 반환합니다.

```cpp
std::cout << "Logical device created.\n"
          << "Compute queue ready: family " << computeQueueFamilyIndex << ", queue 0.\n";
return true;
```

## 초기화 순서에 연결하기

`createLogicalDevice()`가 완성됐으니 초기화 과정에 연결하겠습니다. 물리적 디바이스와 큐 패밀리를 먼저 선택해야 하므로, 기존 `initVulkan()`을 다음과 같이 바꿉니다.

```cpp
bool initVulkan()
{
    return createInstance()
        && setupDebugMessenger()
        && pickPhysicalDevice()
        && createLogicalDevice();
}
```

앞 단계가 실패하면 `&&` 뒤의 함수는 실행되지 않습니다. 장치 선택에 성공한 경우에만 그 장치로 논리적 디바이스를 만듭니다.

마지막으로 `main()`에서 `app`의 범위가 끝난 뒤 출력하는 메시지도 바꾸겠습니다. 해당 TODO와 기존 출력문을 다음 줄로 교체합니다.

```cpp
std::cout << "Device, debug messenger and instance destroyed.\n";
```

`app`이 소멸하면 RAII 멤버는 선언의 역순으로 정리됩니다. 큐는 논리적 디바이스가 해제될 때 함께 정리되므로 별도의 큐 해제 함수를 호출하지 않습니다.

여기까지 작성하면 사용할 디바이스와 큐가 준비됩니다. 다음 장에서는 이 디바이스를 통해 연산에 필요한 버퍼와 메모리를 만들겠습니다.

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
