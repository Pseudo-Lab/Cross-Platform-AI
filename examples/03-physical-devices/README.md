# 03. Physical device와 Queue family

[이전: Validation layer](../02-validation-layers/README.md) · [다음: Logical device](../04-logical-device/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

Vulkan 1.3과 컴퓨트 명령을 지원하는 물리 장치를 선택합니다.
선택한 장치의 큐 패밀리 번호를 저장하고 기능과 연산 한계를 조회합니다.

02장의 Instance와 Debug messenger를 유지하고 `pickPhysicalDevice()`를 추가합니다.
이 장의 결과는 장치와 큐 패밀리의 **선택**입니다. 실제 Queue는 다음 장에서 가져옵니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 완료합니다.
C++20, CMake 3.24 이상, Vulkan SDK 1.3 이상과 validation layer가 필요합니다.
실행할 물리 장치도 Vulkan 1.3 이상을 지원해야 합니다.
저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/03-physical-devices
```

Windows의 Visual Studio 생성기에서는 다음 명령을 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target physical_devices
.\build\Debug\physical_devices.exe
```

Linux의 Makefiles 또는 Ninja에서는 다음 명령을 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target physical_devices
./build/physical_devices
```

전체 코드는 [main.cpp](main.cpp)에 있습니다. 이전 장의 빌드 결과를 참조하지 않습니다.

## 실행 흐름과 멤버

```text
main → run → initVulkan
  → createInstance
  → setupDebugMessenger
  → pickPhysicalDevice
    → 물리 장치 열거
    → API 버전 확인
    → findComputeQueueFamily
    → 선택한 장치와 큐 패밀리 번호 저장
    → printDeviceInfo
```

`initVulkan()`에 새 단계를 연결합니다.

```cpp
return createInstance() && setupDebugMessenger() && pickPhysicalDevice();
```

클래스에는 다음 멤버를 추가합니다.

```cpp
vk::raii::PhysicalDevice physicalDevice = nullptr;
std::uint32_t computeQueueFamilyIndex = 0;
```

`physicalDevice`는 선택한 물리 장치의 핸들을 보관합니다.
`computeQueueFamilyIndex`는 그 장치에서 사용할 큐 패밀리의 배열 인덱스입니다.
초기값 0이 선택 성공을 뜻하지는 않습니다. `pickPhysicalDevice()`가 성공한 뒤에만 사용합니다.

## 1. Physical device와 Logical device

Physical device는 Vulkan 구현이 노출하는 장치입니다.
보통 내장 GPU나 외장 GPU에 해당하지만 소프트웨어 구현일 수도 있습니다.
Instance는 접근할 수 있는 물리 장치 목록을 제공합니다.

```cpp
auto physicalDevices = instance.enumeratePhysicalDevices(); // C API: vkEnumeratePhysicalDevices
```

이 호출은 장치를 생성하지 않습니다. 사용할 수 있는 장치의 핸들을 가져옵니다.
목록이 비어 있으면 Vulkan을 사용할 장치가 없으므로 오류를 출력하고 `false`를 반환합니다.

Physical device에서는 장치의 속성, 기능, 큐 패밀리, 메모리 구성을 조회합니다.
실제로 자원을 생성하고 작업을 제출하려면 선택한 Physical device를 바탕으로
Logical device를 만들어야 합니다. Logical device 생성은 다음 장에서 다룹니다.

## 2. 사용할 장치의 조건

이 예제는 다음 조건을 모두 만족하는 첫 번째 장치를 선택합니다.

1. 장치의 `apiVersion`이 Vulkan 1.3 이상입니다.
2. 큐가 하나 이상 있고 컴퓨트 명령을 지원하는 큐 패밀리가 있습니다.

먼저 속성에서 API 버전을 확인합니다.

```cpp
const auto properties = candidate.getProperties(); // C API: vkGetPhysicalDeviceProperties
if (properties.apiVersion < VK_API_VERSION_1_3)
{
    continue;
}
```

01장에서 조회한 로더 버전과 여기서 조회한 장치 버전은 다릅니다.
로더가 Vulkan 1.3 이상이어도 특정 장치의 드라이버는 더 낮은 버전만 지원할 수 있습니다.

장치 유형이나 이름을 성능 점수로 사용하지 않습니다.
나열된 순서에서 조건에 맞는 첫 장치를 사용하므로, 여러 장치가 있으면
선택 결과가 가장 빠른 장치라는 보장은 없습니다.

## 3. Queue family 찾기

Queue는 장치에 작업을 제출하는 통로입니다.
Queue family는 같은 종류의 명령을 지원하는 큐의 집합입니다.
각 family에는 지원 명령을 나타내는 `queueFlags`와 사용할 수 있는 큐 수 `queueCount`가 있습니다.

```cpp
const auto queueFamilies = candidate.getQueueFamilyProperties(); // C API: vkGetPhysicalDeviceQueueFamilyProperties
```

반환된 배열의 인덱스가 queue family index입니다.
컴퓨트 명령을 제출하려면 `eCompute` 비트가 포함된 family를 선택해야 합니다.

```cpp
for (std::uint32_t index = 0; index < queueFamilies.size(); ++index)
{
    const auto& family = queueFamilies[index];
    if (family.queueCount > 0 && (family.queueFlags & vk::QueueFlagBits::eCompute))
    {
        return index;
    }
}
return std::nullopt;
```

`queueFlags`에는 여러 비트가 함께 설정될 수 있으므로 비트 AND로 검사합니다.
`queueFlags == eCompute`처럼 비교하면 다른 명령도 지원하는 유효한 family를 제외하게 됩니다.

`findComputeQueueFamily()`의 반환형은 `std::optional<std::uint32_t>`입니다.
인덱스 0도 유효하므로, 찾지 못한 경우를 0으로 표현하면 구분할 수 없습니다.
`std::nullopt`는 조건에 맞는 family가 없다는 뜻입니다.
호출부의 `if (!queueFamily)`는 숫자가 0인지가 아니라 값이 존재하는지 검사합니다.

| 값 | 의미 |
|---|---|
| Queue family index | 어떤 명령 집합의 큐를 사용할지 지정 |
| Queue index | 선택한 family 안에서 몇 번째 큐를 사용할지 지정 |
| `queueCount` | 그 family에서 요청할 수 있는 큐 개수 |

예를 들어 family index가 2이고 `queueCount`가 4라면, 그 family의 queue index는
0부터 3까지입니다. 이 장은 family index만 저장합니다.
다음 장에서는 이 family에 큐 하나를 요청하고 queue index 0을 가져옵니다.

## 4. 선택 결과 저장

API 버전과 큐 조건을 모두 만족하면 장치 핸들과 family index를 멤버로 저장합니다.

```cpp
physicalDevice = std::move(candidate);
computeQueueFamilyIndex = *queueFamily;
printDeviceInfo();
return true;
```

`std::move`는 C++ 래퍼의 상태를 멤버로 이동합니다. GPU의 데이터를 복사하거나
새 장치를 생성하는 Vulkan 호출이 아닙니다.
열거 결과를 담은 지역 변수가 사라진 뒤에도 선택 결과를 멤버에서 사용할 수 있습니다.

모든 후보가 조건을 통과하지 못하면 오류를 출력하고 `false`를 반환합니다.
실패 결과는 `initVulkan()`과 `run()`을 거쳐 `main()`의 종료 코드 1로 전달됩니다.

`PhysicalDevice`에는 `vkDestroyPhysicalDevice`라는 해제 함수가 없습니다.
핸들은 이를 열거한 Instance가 유효한 동안 사용합니다.
따라서 멤버 `physicalDevice`를 `instance`보다 뒤에 선언해 먼저 소멸하도록 둡니다.

## 5. Properties, Features, Extensions

장치가 무엇을 제공하는지는 다음과 같이 구분합니다.

| 구분 | 확인하는 내용 | 예 |
|---|---|---|
| Properties | 장치 정보와 구현의 한계 | `deviceName`, `apiVersion`, `limits` |
| Features | 지원 여부를 조회하고 필요하면 활성화하는 기능 | `shaderFloat64` |
| Extensions | 추가 API·동작을 제공하는 이름 있는 규격 | 장치 확장 목록에서 지원 여부 조회 |

Feature를 코어 전용 기능으로 한정하면 안 됩니다. 확장에도 feature 구조체가 있으며,
확장 기능이 이후 Vulkan 코어 버전에 포함될 수도 있습니다.
확장 지원 여부와 그 확장에서 제공하는 feature 지원 여부는 각각 확인해야 합니다.

`printDeviceInfo()`는 64비트 부동소수점 셰이더 연산의 지원 여부를 예로 조회합니다.

```cpp
const auto features = physicalDevice.getFeatures(); // C API: vkGetPhysicalDeviceFeatures
```

`features.shaderFloat64`가 참이어도 애플리케이션에서 자동으로 활성화되지는 않습니다.
이 기능이 필요하면 Logical device를 생성할 때 명시적으로 요청해야 합니다.
이 튜토리얼의 벡터 덧셈은 32비트 `float`를 사용하므로 `shaderFloat64`를
선택 조건으로 사용하거나 활성화하지 않습니다.

`getFeatures()`는 `VkPhysicalDeviceFeatures`에 있는 기능을 조회합니다.
이 구조체에 없는 버전·확장 기능은 `getFeatures2()`와 필요한 feature 구조체로 조회합니다.
이 예제에서는 추가 device extension이나 선택적 feature를 요구하지 않습니다.

## 6. 컴퓨트 한계 조회

`getProperties()` 결과의 `limits`에는 지원 가능한 크기와 개수의 한계가 있습니다.
`printDeviceInfo()`는 이후 셰이더와 버퍼를 작성할 때 필요한 값을 출력합니다.

| 한계 | 의미 |
|---|---|
| `maxComputeWorkGroupCount[3]` | 한 번의 dispatch에서 지정할 수 있는 X·Y·Z별 workgroup 수의 상한 |
| `maxComputeWorkGroupSize[3]` | 한 workgroup 안의 X·Y·Z별 invocation 수의 상한 |
| `maxComputeWorkGroupInvocations` | 한 workgroup 안의 전체 invocation 수의 상한 |
| `maxStorageBufferRange` | Storage buffer descriptor 하나에서 접근 범위로 지정할 수 있는 최대 바이트 수 |

Invocation은 셰이더를 한 번 실행하는 단위이며, workgroup은 invocation의 묶음입니다.
Workgroup의 각 축 크기가 개별 상한 이하여도 X·Y·Z 크기의 곱이
`maxComputeWorkGroupInvocations`를 넘으면 안 됩니다.
예를 들어 로컬 크기 `(32, 32, 1)`에는 invocation 1,024개가 있습니다.
각 축의 상한과 전체 개수 상한을 모두 확인해야 합니다.

이 값은 지원 범위이며 권장 성능 설정이 아닙니다.
Workgroup 구성과 dispatch는 컴퓨트 셰이더·명령 버퍼 단계에서 적용합니다.
`maxStorageBufferRange`도 장치 메모리 총량이나 모든 버퍼의 최대 할당 크기를 뜻하지 않습니다.

## 완료 기준

프로그램이 종료 코드 0으로 끝나고 다음 정보를 출력해야 합니다.

- 선택한 장치 이름과 Vulkan API 버전
- Compute queue family index와 해당 family의 큐 개수
- `shaderFloat64` 지원 여부와 컴퓨트·storage buffer 한계
- 02장에서 추가한 `tutorial.callback-check` 메시지

API 버전은 1.3 이상이어야 하며, 선택한 family는 `eCompute`를 포함하고 `queueCount > 0`이어야 합니다.
장치 이름과 family index·한계 값은 환경마다 달라집니다.
이 단계에서는 Queue 생성이나 GPU 연산을 실행하지 않습니다.

## 문제 해결

| 문제 | 확인할 내용 |
|---|---|
| `No Vulkan physical devices found` | GPU 드라이버와 Vulkan 장치 조회 결과 확인 |
| 조건에 맞는 장치가 없음 | 장치의 API 버전, compute queue family 지원 확인 |
| 예상과 다른 GPU가 선택됨 | 조건에 맞는 첫 장치를 선택하는 코드와 장치 열거 순서 확인 |
| Validation layer 관련 오류 | 이전 장의 레이어·확장 설치 확인 |

## 참고 자료와 라이선스

- [Khronos Vulkan Tutorial — Physical devices and queue families](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html)
- [VkQueueFamilyProperties](https://docs.vulkan.org/refpages/latest/refpages/source/VkQueueFamilyProperties.html)
- [VkPhysicalDeviceLimits](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceLimits.html)
- [Vulkan Guide — Enabling Features](https://docs.vulkan.org/guide/latest/enabling_features.html)

이 문서는 Khronos Vulkan Tutorial contributors의 위 Physical devices and queue families 장을
번역한 학습 노트를 바탕으로 컴퓨트 장치 선택에 맞게 각색했습니다.
이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)으로 제공합니다.
