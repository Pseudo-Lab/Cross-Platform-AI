# 01. Instance 생성과 해제

[이전: 환경 설정](../00-env-settings/README.md) · [다음: Validation layers](../02-validation-layers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

## 목적

Vulkan Instance를 생성하고 해제합니다. 그 과정에서 애플리케이션의 API 버전을 지정하고,
사용 가능한 instance extension을 조회합니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 먼저 완료합니다.
C++20, CMake 3.24 이상과 Vulkan SDK 1.3 이상이 필요합니다.
프로그램이 요청하는 API 버전도 Vulkan 1.3입니다.
설치된 SDK가 1.4여도 이 예제는 API 1.3을 요청합니다.
이 장에서는 셰이더를 컴파일하지 않습니다.

저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/01-instance
```

Windows에서는 다음 명령을 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target instance_example
.\build\Debug\instance_example.exe
```

마지막 실행 경로는 Visual Studio 생성기 기준입니다.
Linux에서 Makefiles 또는 Ninja를 사용하면 다음과 같습니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target instance_example
./build/instance_example
```

각 장의 폴더는 이전 단계의 프로그램에 함수와 멤버를 추가한 독립 빌드용 스냅샷입니다.
이전 장의 `build` 디렉터리를 재사용하지 않습니다.

## 실행 흐름

```text
main: ComputeApplication 객체 생성 → Context 생성
  → run()
    → initVulkan()
      → createInstance()
        → 로더 API 버전 확인
        → 사용 가능한 instance extension 조회
        → ApplicationInfo와 InstanceCreateInfo 설정
        → 멤버 Instance 생성
  → app의 스코프 종료
    → Instance 해제
    → Context 소멸
```

`main()`은 애플리케이션 객체를 만들고 `run()`을 호출합니다.
`run()`은 `initVulkan()`을, `initVulkan()`은 `createInstance()`를 호출합니다.
이후 장에서는 `initVulkan()`에 해당 단계의 초기화 함수를 추가합니다.

| 함수·멤버 | 역할 |
|---|---|
| `run()` | 프로그램 실행 시작, 초기화 결과 반환 |
| `initVulkan()` | Vulkan 초기화 단계 호출 |
| `createInstance()` | 버전 확인, 확장 조회, Instance 생성 |
| `context` | 전역 Vulkan 함수 접근 |
| `instance` | 생성한 Instance 보유 |
| `printVersion()` | 버전 번호 출력용 보조 함수 |

전체 코드는 [main.cpp](main.cpp), 빌드 설정은 [CMakeLists.txt](CMakeLists.txt)에 있습니다.

### 1. Context와 Instance

`vk::raii::Context`는 Vulkan-Hpp에서 전역 Vulkan 함수에 접근하기 위한 C++ 객체입니다.
Vulkan C API에 `VkContext`라는 객체가 있는 것은 아닙니다.
기본 생성자는 로더를 동적으로 불러오고 전역 함수 포인터를 준비합니다.

```cpp
vk::raii::Context context;
vk::raii::Instance instance = nullptr;
```

`vk::raii::Instance`는 `VkInstance`의 수명을 관리합니다. Instance를 만들면
Vulkan 구현에 애플리케이션 정보를 전달하고 이후 물리 장치 조회 등을 시작할 수 있습니다.
이 단계에서 GPU를 선택하거나 연산용 Device·Queue를 만드는 것은 아닙니다.

두 객체는 `ComputeApplication`의 멤버입니다. `instance`는 빈 핸들로 시작하고
`createInstance()`에서 생성한 객체를 대입받습니다. 함수가 끝나도 Instance는 유지됩니다.

### 2. API 버전

`enumerateInstanceVersion()`으로 로더의 instance 수준 지원 버전을 조회합니다.
이 함수는 Vulkan 1.1부터 제공되므로 코드에서는 함수 포인터의 존재부터 확인합니다.

```cpp
constexpr auto requestedVersion = VK_API_VERSION_1_3;
const auto loaderVersion = context.enumerateInstanceVersion();
```

| 값 | 의미 |
|---|---|
| SDK·헤더 버전 | 컴파일할 때 사용할 수 있는 선언의 범위 |
| `loaderVersion` | 실행 환경의 instance 수준 API 지원 버전 |
| `appInfo.apiVersion` | 애플리케이션이 사용하도록 작성된 API 버전 |
| 물리 장치의 `apiVersion` | 해당 GPU 구현의 장치 수준 API 지원 버전. GPU 선택 장에서 확인 |

로더가 1.4를 지원해도 모든 GPU가 1.4 기능을 지원한다는 뜻은 아닙니다.
이 튜토리얼은 이후 컴퓨트 예제와 vAi 학습에 사용할 기준으로 Vulkan 1.3을 요청합니다.
Instance 생성 자체나 RAII 사용이 Vulkan 1.3을 필수로 요구하는 것은 아닙니다.

### 3. Instance extension 조회

확장은 Vulkan의 기본 API에 기능을 추가합니다.
`enumerateInstanceExtensionProperties()`는 레이어 이름을 지정하지 않으면
로더·드라이버와 암시적으로 활성화된 레이어가 제공하는 instance extension을 조회합니다.
설치된 모든 명시적 레이어의 확장을 합친 목록은 아닙니다.

```cpp
const auto extensions = context.enumerateInstanceExtensionProperties();
```

**지원 목록 조회와 확장 활성화는 별개입니다.** 목록을 출력하는 것만으로 활성화되지 않습니다.
이 장에서는 활성화할 확장이 없으므로 추가 extension과 layer 없이 Instance를 생성합니다.

### 4. 생성 정보

`vk::ApplicationInfo`에 애플리케이션 이름·버전과 사용할 API 버전을 적습니다.
`applicationVersion`과 `engineVersion`은 프로그램·엔진 자체의 버전이며 Vulkan 버전이 아닙니다.

```cpp
vk::ApplicationInfo appInfo{};
appInfo.pApplicationName = "01-instance";
appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.apiVersion = requestedVersion;

vk::InstanceCreateInfo createInfo{};
createInfo.pApplicationInfo = &appInfo;
```

Vulkan-Hpp는 구조체의 `sType`을 설정합니다. `pNext`는 기본값인 `nullptr`로 둡니다.
`pApplicationInfo`는 `appInfo`를 가리킵니다. 따라서 `appInfo`는 Instance 생성 호출이
끝날 때까지 유효해야 합니다. 확장과 레이어 개수는 기본값 0으로 둡니다.

### 5. 생성과 해제

`createInstance()`에서 생성한 Instance를 클래스 멤버에 저장합니다.

```cpp
instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
std::cout << "Instance created.\n";
return true;
```

생성자는 내부에서 `vkCreateInstance`를 호출합니다. 별도 allocator를 전달하지 않아
기본 할당 방식을 사용합니다. 대입은 생성된 핸들의 소유권을 멤버 `instance`로 옮깁니다.
이 멤버는 함수가 반환된 뒤에도 다음 초기화 단계에서 사용할 수 있습니다.

```cpp
int main()
{
    {
        ComputeApplication app;
        if (!app.run())
        {
            return 1;
        }
    } // C API: vkDestroyInstance (app.instance 소멸자)

    std::cout << "Instance destroyed.\n";
    return 0;
}
```

`app`이 스코프를 벗어나면 멤버가 선언의 역순으로 소멸합니다.
`context`를 `instance`보다 먼저 선언했으므로 Instance가 먼저 해제됩니다.
Instance 소멸자는 `vkDestroyInstance`를 호출합니다.
`Instance destroyed.`는 이 해제가 끝난 뒤 출력합니다.
RAII가 소유하는 핸들을 직접 다시 파괴하면 안 됩니다.

로더 버전 검사에 실패하면 `createInstance()`는 오류 메시지를 출력하고 `false`를 반환합니다.
이 결과를 `initVulkan()`과 `run()`이 전달하면 `main()`이 종료 코드 1을 반환합니다.
Vulkan-Hpp의 기본 예외 기능은 유지하며, 이 예제에서는 발생한 예외를 별도로 처리하지 않습니다.

## 완료 기준

출력 앞부분의 버전과 확장 목록은 실행 환경에 따라 달라집니다.
마지막에 다음 두 줄이 순서대로 출력되고 종료 코드가 0이면 이 장의 목표를 완료한 것입니다.

```text
Instance created.
Instance destroyed.
```

GPU 선택과 컴퓨트 실행 성공 여부는 이후 장에서 확인합니다.

## 문제 해결

| 문제 | 확인할 내용 |
|---|---|
| CMake가 Vulkan을 찾지 못함 | SDK 설치, `VULKAN_SDK`, SDK 헤더 버전 확인 |
| 로더를 불러오지 못함 | Vulkan 런타임·GPU 드라이버 설치와 라이브러리 검색 경로 확인 |
| Vulkan 1.3 로더가 필요하다는 오류 | 로더·드라이버를 갱신하고 앞 장의 `env_check` 실행 |
| `ErrorIncompatibleDriver` | 요청 API 버전과 Vulkan 드라이버 설치 확인 |

## 참고 자료

- [Khronos Vulkan-Hpp RAII 안내](https://github.com/KhronosGroup/Vulkan-Hpp/blob/main/docs/VkRaiiProgrammingGuide.md)
- [Vulkan API 버전](https://docs.vulkan.org/guide/latest/versions.html)
- [VkApplicationInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkApplicationInfo.html)
- [Instance extension 조회](https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumerateInstanceExtensionProperties.html)
