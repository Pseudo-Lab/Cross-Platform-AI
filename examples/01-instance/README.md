# 01. Instance 생성과 해제

[이전: 환경 설정](../00-env-settings/README.md) · [다음: Validation layers](../02-validation-layers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

Vulkan을 사용하려면 먼저 인스턴스(Instance)를 생성해야 합니다. 인스턴스는 애플리케이션과 Vulkan 라이브러리를 연결하며, 생성할 때 드라이버에 애플리케이션 정보와 사용할 기능을 전달합니다.

[main.cpp](main.cpp)를 열고 아래 설명을 따라 코드를 채워 보겠습니다. 완성된 코드는 공개 후 `examples/completed/01-instance.cpp`에서 확인할 수 있습니다.

## 코드 구조

먼저 프로그램의 틀부터 살펴보겠습니다.

```cpp
#include <vulkan/vulkan_raii.hpp>

#include <iostream>

class ComputeApplication
{
public:
    void run()
    {
        initVulkan();
    }

private:
    // TODO: Context와 Instance 멤버를 추가합니다.

    void initVulkan()
    {
        // TODO: createInstance()를 호출합니다.
    }

    // TODO: createInstance() 함수를 추가하고 생성 정보를 채웁니다.
};

int main()
{
    ComputeApplication app;
    app.run();
    return 0;
}
```

`main()`에서 `ComputeApplication` 객체를 만들고 `run()`을 호출합니다. `run()`은 Vulkan 초기화를 맡는 `initVulkan()`을 호출합니다. 앞으로 Vulkan 객체를 하나씩 만들 때마다 이 함수에 초기화 단계를 추가하게 됩니다.

## 인스턴스 생성 함수 추가하기

먼저 `private` 안에 `createInstance()` 함수를 만들고, `initVulkan()`에서 호출해 보겠습니다.

```cpp
void initVulkan()
{
    createInstance();
}

void createInstance()
{
}
```

여기에 Vulkan 함수에 접근할 Context와 인스턴스를 담을 멤버 변수도 추가합니다. 두 변수는 `private` 바로 아래, 함수들보다 앞에 둡니다.

```cpp
vk::raii::Context context;
vk::raii::Instance instance = nullptr;
```

`vk::raii::Context`는 Vulkan 로더를 불러오고, 인스턴스를 만들기 전부터 필요한 Vulkan 함수에 접근할 수 있게 해 줍니다. `instance`는 아직 생성하지 않았으므로 `nullptr`로 초기화합니다. 멤버 변수에 담아 두면 `createInstance()`가 끝난 뒤에도 인스턴스를 사용할 수 있습니다.

## 애플리케이션 정보 설정: vk::ApplicationInfo

이제 `createInstance()` 안을 채워 보겠습니다. 먼저 애플리케이션 정보를 담는 `vk::ApplicationInfo`를 작성합니다.

```cpp
vk::ApplicationInfo appInfo{};
appInfo.pApplicationName = "01-instance";
appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.pEngineName = "No Engine";
appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
appInfo.apiVersion = VK_API_VERSION_1_3;
```

`pApplicationName`과 `applicationVersion`은 프로그램의 이름과 버전입니다. 별도의 엔진을 사용하지 않으므로 `pEngineName`에는 `"No Engine"`을 적었습니다. `VK_MAKE_API_VERSION(0, 1, 0, 0)`은 variant 0의 버전 1.0.0을 나타냅니다.

마지막의 `apiVersion`은 애플리케이션에서 사용할 Vulkan API 버전입니다. 이 튜토리얼에서는 Vulkan 1.3을 사용합니다. 구조체의 종류를 나타내는 `sType`은 Vulkan-Hpp가 설정해 줍니다.

## 사용 가능한 확장 기능 확인하기

확장(Extension)은 Vulkan에 기능을 추가하는 방법입니다. 인스턴스를 만들기 전에 어떤 확장을 사용할 수 있는지 확인해 보겠습니다. 앞서 작성한 `appInfo` 설정 아래에 다음 코드를 추가합니다.

```cpp
const auto extensions = context.enumerateInstanceExtensionProperties(); // C API: vkEnumerateInstanceExtensionProperties
std::cout << "Available instance extensions:\n";
for (const auto& extension : extensions)
{
    std::cout << "  " << extension.extensionName << '\n';
}
```

`enumerateInstanceExtensionProperties()`는 사용 가능한 인스턴스 확장 목록을 반환합니다. 각 항목의 `extensionName`을 출력하면 확장 이름을 확인할 수 있습니다. 이 코드는 목록을 조회하며, 확장을 활성화하지는 않습니다.

## 인스턴스 생성 정보 설정: vk::InstanceCreateInfo

Vulkan은 객체를 생성할 때 필요한 정보를 구조체에 담아 전달합니다. 확장 목록을 출력하는 반복문 아래에 `vk::InstanceCreateInfo`를 추가합니다.

```cpp
vk::InstanceCreateInfo createInfo{};
createInfo.pApplicationInfo = &appInfo;
```

`pApplicationInfo`에는 앞에서 작성한 `appInfo`의 주소를 전달합니다. 이 구조체에는 활성화할 확장과 레이어도 지정할 수 있습니다. 여기서는 창 없이 컴퓨트 작업을 준비하므로 창 시스템용 확장은 필요하지 않습니다. 레이어는 다음 장에서 다룰 예정이므로 확장과 레이어 개수는 기본값인 0으로 둡니다.

## 인스턴스 생성하기

이제 필요한 정보를 모두 준비했습니다. `createInfo` 설정 바로 아래에서 인스턴스를 생성하고, 멤버 변수에 저장합니다.

```cpp
instance = vk::raii::Instance(context, createInfo); // C API: vkCreateInstance
std::cout << "Instance created.\n";
```

`vk::raii::Instance`에 Vulkan 함수에 접근할 `context`와 생성 정보인 `createInfo`를 전달합니다. 생성이 성공하면 다음 줄에서 `Instance created.`가 출력됩니다. 여기까지 작성하면 `createInstance()`가 완성됩니다.

## 인스턴스 해제하기

`vk::raii` 객체는 수명이 끝날 때 자신이 관리하는 Vulkan 자원을 해제합니다. 이를 확인하기 위해 `main()`을 다음과 같이 바꿔 보겠습니다.

```cpp
int main()
{
    {
        ComputeApplication app;
        app.run();
    } // C API: vkDestroyInstance (app.instance 소멸자)

    std::cout << "Instance destroyed.\n";
    return 0;
}
```

안쪽 중괄호가 끝나면 `app`이 소멸하면서 멤버 변수도 선언의 역순으로 소멸합니다. `context` 다음에 `instance`를 선언했으므로 인스턴스가 먼저 해제됩니다. 그 뒤에 `Instance destroyed.`가 출력됩니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 작성한 `main.cpp`를 빌드해 보겠습니다. 저장소 루트에서 이 예제 폴더로 이동합니다.

```sh
cd examples/01-instance
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target instance_example
.\build\Debug\main.exe
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target instance_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target instance_example
./build/main
```

완성본이 있다면 위의 CMake 구성 명령을 실행한 뒤, 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target instance_completed
./build/completed/main
```

### 실행 결과

코드를 모두 채웠다면 확장 목록 뒤에 다음 두 줄이 출력됩니다. 아직 채우지 않은 시작 코드는 아무것도 출력하지 않고 종료합니다.

```text
Instance created.
Instance destroyed.
```

---

이 문서는 [Khronos Vulkan Tutorial: Base code](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/00_Base_code.html)와 [Instance](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/01_Instance.html)를 컴퓨트 실습에 맞게 번역·각색했습니다. 원문과 이 문서는 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)을 따릅니다.
