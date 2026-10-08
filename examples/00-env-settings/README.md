# 00. 환경 설정

[English](README-en.md)

## 목적

이 장에서는 Vulkan 컴퓨트 튜토리얼에 필요한 도구를 설치합니다.
C++ 예제는 Vulkan-Hpp로 Vulkan 로더 버전을 출력합니다.
CMake는 간단한 컴퓨트 셰이더를 SPIR-V 파일로 컴파일합니다.

장치와 설치된 validation layer는 `vulkaninfo`로 확인합니다.

## 필요한 소프트웨어

| 항목 | 요구 사항 |
|---|---|
| C++ 컴파일러 | C++20 컴파일 모드 지원 |
| CMake | 버전 3.24 이상 |
| Vulkan SDK | 1.3 이상, Vulkan 헤더·Vulkan-Hpp·`glslc` 포함 |
| Vulkan 로더 | 1.3 이상 |
| GPU 드라이버 | Vulkan 1.3 지원 |
| Vulkan 도구 | `vulkaninfo`, `VK_LAYER_KHRONOS_validation` |

## 도구 설치

### Windows

#### C++ 개발 도구 설치

Visual Studio는 IDE와 개발 도구를 함께 제공합니다.
Visual Studio Build Tools는 IDE 없이 개발 도구를 설치하는 선택지입니다.
두 제품 모두 설치 프로그램을 실행하면 Visual Studio Installer에서 구성 요소를 선택합니다.

1. [Visual Studio 2022 이상 또는 해당 버전의 Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/) 설치 프로그램을 내려받습니다.
2. 내려받은 설치 프로그램을 실행합니다.
3. **C++를 사용한 데스크톱 개발** 워크로드를 선택합니다.

   - x64/x86용 MSVC 빌드 도구
   - Windows SDK
   - Windows용 C++ CMake 도구

**Windows용 C++ CMake 도구**를 설치했다면 CMake를 별도로 설치할 필요가 없습니다.
별도 설치를 원하면 [CMake 설치 프로그램](https://cmake.org/download/)을 사용할 수 있습니다.

#### 일반 터미널에서 CMake 사용

`PATH`는 명령 이름으로 실행 파일을 찾을 때 검색하는 디렉터리 목록입니다.
`cmake.exe`가 있는 디렉터리를 등록하면 작업 디렉터리와 관계없이 `cmake` 명령을 호출할 수 있습니다.
다만 `build` 같은 상대 경로는 여전히 현재 작업 디렉터리를 기준으로 해석합니다.

1. 새 PowerShell 터미널에서 다음 명령을 실행합니다.

   ```powershell
   cmake --version
   ```

2. 명령을 찾지 못하면 설치된 `cmake.exe`의 위치를 확인합니다.

Visual Studio에 포함된 CMake는 설치 위치 아래의 다음 디렉터리에 있습니다.
Installer 하단의 설치 위치를 기준으로 찾습니다.

```text
Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
```

별도 CMake 설치 프로그램을 사용했다면 해당 프로그램에서 지정한 설치 위치를 확인합니다.

3. Windows 검색에서 **시스템 환경 변수 편집**을 엽니다.
4. **환경 변수**를 선택합니다.
5. **사용자 변수**의 `Path`를 선택하고 **편집**을 누릅니다.
6. **새로 만들기**를 눌러 `cmake.exe`가 있는 디렉터리의 전체 경로를 추가합니다.
7. 각 창에서 **확인**을 눌러 저장합니다.
8. 터미널 앱을 완전히 종료한 뒤 다시 엽니다.
9. `cmake --version`을 다시 실행합니다.

1번 명령이 정상적으로 실행되면 2~9번은 생략합니다.
VS Code 등의 통합 터미널을 사용한다면 해당 앱도 다시 시작합니다.

#### Vulkan SDK 설치와 확인

1. GPU 제조사 웹사이트에서 GPU 드라이버를 설치합니다.
2. Windows용 [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)를 설치합니다.
3. 터미널 앱을 완전히 종료한 뒤 다시 엽니다.
4. PowerShell에서 SDK 경로와 셰이더 컴파일러를 확인합니다.

   ```powershell
   $env:VULKAN_SDK
   glslc --version
   ```

Vulkan SDK의 기본 설치 과정은 시스템 환경 변수 `VULKAN_SDK`와 SDK의 `Bin` 검색 경로를 등록합니다.
`VULKAN_SDK`는 SDK 설치 디렉터리를 가리킵니다.
경로와 컴파일러 버전이 정상적으로 출력되면 환경 변수를 추가로 설정할 필요가 없습니다.
설치 동작은 [Windows SDK 안내](https://vulkan.lunarg.com/doc/view/latest/windows/getting_started.html)를 참고합니다.

#### 환경 변수가 누락된 경우

새 터미널에서도 SDK 경로가 비어 있거나 `glslc`를 찾지 못하면 저장된 환경 변수를 확인합니다.

1. **시스템 환경 변수 편집**에서 **환경 변수**를 엽니다.
2. **사용자 변수**와 **시스템 변수**에서 `VULKAN_SDK`를 찾습니다.
3. 변수가 없으면 **사용자 변수**에 다음 값을 추가합니다.

   | 변수 이름 | 변수 값 예시 |
   |---|---|
   | `VULKAN_SDK` | `C:\VulkanSDK\1.4.341.1` |

4. `Path`에 SDK의 `Bin` 디렉터리가 없으면 사용자 `Path`에 해당 전체 경로를 추가합니다.

   ```text
   C:\VulkanSDK\1.4.341.1\Bin
   ```

5. 각 창에서 **확인**을 눌러 저장합니다.
6. 터미널 앱을 다시 시작합니다.
7. SDK 경로와 `glslc --version` 출력을 다시 확인합니다.

예시의 버전은 실제 설치한 버전으로 바꿉니다.
기존 변수가 잘못된 경로를 가리키면 해당 값을 수정합니다.

다음 PowerShell 명령은 현재 터미널과 그 터미널에서 실행한 자식 프로세스에만 적용됩니다.
영구 등록을 대신하지 않습니다.

```powershell
$env:VULKAN_SDK = 'C:\VulkanSDK\1.4.341.1'
$env:Path = "$env:VULKAN_SDK\Bin;$env:Path"
```

### Ubuntu 24.04

1. 패키지 목록을 갱신합니다.

   ```sh
   sudo apt update
   ```

2. 컴파일러, CMake, Vulkan 개발 패키지를 설치합니다.

   ```sh
   sudo apt install build-essential cmake libvulkan-dev vulkan-tools glslc vulkan-validationlayers
   ```

3. GPU에 맞는 Vulkan 드라이버를 설치합니다.

Ubuntu는 지원 대상 Intel·AMD GPU에 Mesa Vulkan 드라이버를 제공합니다.
NVIDIA GPU에는 호환되는 NVIDIA 드라이버가 필요합니다.
사용 중인 Ubuntu 버전과 GPU에 맞는 드라이버 설치 절차를 따릅니다.

4. 새 터미널을 엽니다.

다른 Linux 배포판에서는 [LunarG Linux SDK 안내](https://vulkan.lunarg.com/doc/view/latest/linux/getting_started.html)를 따릅니다.

## 도구 확인

1. CMake 버전이 3.24 이상인지 확인합니다.

   ```sh
   cmake --version
   ```

2. 셰이더 컴파일러 버전을 출력합니다.

   ```sh
   glslc --version
   ```

3. Vulkan 정보를 출력합니다.

   ```sh
   vulkaninfo --summary
   ```

4. 출력에서 GPU 이름과 해당 장치의 `apiVersion`이 1.3 이상인지 확인합니다.
5. 레이어 목록에서 `VK_LAYER_KHRONOS_validation`을 확인합니다.


## 예제 코드 따라가기

도구가 준비됐다면 [main.cpp](main.cpp)를 열어 설치 상태를 어떻게 검사하는지 보겠습니다. 이 장의 코드는 이미 작성되어 있습니다. `main()` 안에서 로더에 접근하고, 지원 버전을 조회한 뒤 필요한 버전인지 확인하는 순서입니다.

먼저 Vulkan-Hpp와 콘솔 출력에 필요한 헤더를 포함합니다. `vulkan_raii.hpp`는 이후 장에서도 Vulkan 객체를 관리할 때 사용할 헤더입니다.

```cpp
#include <vulkan/vulkan_raii.hpp>
#include <iostream>
```

`main()`의 첫 줄에서는 Vulkan 로더 함수에 접근할 `Context`를 만듭니다.

```cpp
vk::raii::Context context;
```

버전을 묻기 전에, 로더가 버전 조회 함수를 제공하는지 확인합니다. 오래된 로더에서는 이 함수가 없을 수 있으므로, 함수 포인터가 비어 있으면 오류를 출력하고 종료합니다.

```cpp
if (!context.getDispatcher()->vkEnumerateInstanceVersion)
{
    std::cerr << "This example requires a Vulkan 1.3 or newer loader.\n";
    return 1;
}
```

함수를 사용할 수 있다면 지원 버전을 가져옵니다. 이 조회는 인스턴스를 만들기 전에도 할 수 있습니다.

```cpp
const auto version = context.enumerateInstanceVersion(); // C API: vkEnumerateInstanceVersion
```

`version`은 버전 정보가 담긴 정수입니다. major, minor, patch를 각각 꺼내 화면에 출력합니다.

```cpp
std::cout << "Vulkan loader version: "
          << VK_API_VERSION_MAJOR(version) << '.'
          << VK_API_VERSION_MINOR(version) << '.'
          << VK_API_VERSION_PATCH(version) << '\n';
```

마지막으로 이 튜토리얼에서 사용할 Vulkan 1.3 이상인지 확인합니다. 조건을 만족하지 못하면 종료 코드 1로 실패를 알립니다. 조건을 통과해 `main()` 끝에 도달하면 종료 코드는 0입니다.

```cpp
if (version < VK_API_VERSION_1_3)
{
    std::cerr << "This example requires a Vulkan 1.3 or newer loader.\n";
    return 1;
}
```

## 셰이더 컴파일 준비하기

C++ 프로그램과 함께 셰이더 컴파일러도 확인하겠습니다. [check.comp](check.comp)는 컴퓨트 셰이더이며, 첫 줄은 사용할 GLSL 버전을 지정합니다.

```glsl
#version 450
```

그다음에는 작업 그룹 하나의 크기를 지정합니다. 여기서는 x, y, z를 모두 1로 두어 그룹 하나에 실행 단위 하나를 둡니다.

```glsl
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
```

컴파일이 되는지 확인할 용도이므로 진입 함수의 본문은 비워 둡니다.

```glsl
void main()
{
}
```

이제 빌드하면 `CMakeLists.txt`가 `glslc`를 호출해 이 파일을 SPIR-V로 변환합니다. 출력 파일은 `build/check.comp.spv`입니다. 이 장에서는 셰이더를 컴파일하는 데까지 확인하며, GPU에 제출하는 코드는 이후 장에서 작성합니다.

## 예제 빌드와 실행

저장소 루트에서 예제 폴더로 이동합니다. 이 폴더의 `CMakeLists.txt`로 독립적으로 빌드할 수 있습니다.

```sh
cd examples/00-env-settings
```

### Windows

먼저 Visual Studio 생성기로 프로젝트를 구성합니다. `-S .`은 현재 폴더의 소스를, `-B build`는 빌드 결과를 저장할 폴더를 지정합니다.

```powershell
cmake -S . -B build
```

이어서 Debug 구성의 `env_check` 대상을 빌드합니다. C++ 프로그램과 셰이더가 함께 컴파일됩니다.

```powershell
cmake --build build --config Debug --target env_check
```

빌드가 성공하면 생성된 프로그램을 실행합니다.

```powershell
.\build\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja 생성기를 사용할 때는 구성 단계에서 Debug를 지정합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

구성이 끝나면 C++ 프로그램과 셰이더를 빌드합니다.

```sh
cmake --build build --target env_check
```

이 생성기에서는 실행 파일이 `build` 바로 아래에 생깁니다.

```sh
./build/main
```

### 실행 결과

출력된 로더 버전이 튜토리얼의 요구 버전을 만족하는지 확인합니다. 셰이더 컴파일 결과인 `build/check.comp.spv`도 함께 생성되었는지 확인합니다.

## 완료 기준

| 확인 항목 | 필요한 결과 |
|---|---|
| 장치 조회 | `vulkaninfo`에 GPU 이름과 1.3 이상의 API 버전이 표시됨 |
| 레이어 설치 | `vulkaninfo`에 `VK_LAYER_KHRONOS_validation`이 표시됨 |
| C++ 빌드와 실행 | `main`이 1.3 이상의 로더 버전을 출력하고 종료 코드 0을 반환함 |
| 셰이더 컴파일 | 컴파일 오류 없이 `check.comp.spv`가 생성됨 |

이 결과로 도구 설치 상태를 확인합니다.
GPU 계산이나 이후 장에서 필요한 장치 기능까지 검증하는 것은 아닙니다.

## 셰이더 개별 컴파일

1. 이 예제의 디렉터리(`examples/00-env-settings`)에서 터미널을 엽니다.
2. 앞의 빌드 절차를 완료한 뒤 셰이더를 컴파일합니다.

   ```sh
   glslc --target-env=vulkan1.3 check.comp -o build/check-manual.spv
   ```

`.comp` 확장자는 컴퓨트 셰이더 단계를 나타냅니다.
대상 환경 옵션은 이 셰이더의 Vulkan 버전을 1.3으로 지정합니다.
명령이 성공하면 출력 파일을 생성하고 종료 코드 0을 반환합니다.

## 문제 해결

| 문제 | 조치 |
|---|---|
| `cmake` 또는 `glslc`를 찾을 수 없음 | 도구를 설치합니다. 실행 파일 디렉터리를 `PATH`에 추가합니다. 새 터미널을 엽니다. |
| CMake가 C++ 컴파일러를 찾지 못함 | 운영체제에 맞는 C++ 워크로드 또는 컴파일러를 설치합니다. |
| CMake가 Vulkan을 찾지 못함 | SDK 경로를 지정합니다. Linux에서는 개발 패키지를 설치합니다. |
| Validation layer가 없음 | SDK의 validation layer 또는 Linux의 validation 패키지를 설치합니다. |
| Vulkan 장치가 표시되지 않음 | 호환되는 GPU 드라이버를 설치합니다. |
| CMake에서 생성기 불일치 오류가 발생함 | 새 생성기에 사용할 빈 빌드 디렉터리를 지정합니다. |

## 참고 자료

- [Vulkan Tutorial: 개발 환경](https://docs.vulkan.org/tutorial/latest/02_Development_environment.html)
- [CMake: FindVulkan](https://cmake.org/cmake/help/v3.24/module/FindVulkan.html)
- [ASD-STE100: Simplified Technical English](https://www.asd-ste100.org/)
