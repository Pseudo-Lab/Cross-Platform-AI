# 08. 컴퓨트 셰이더

[이전: 디스크립터 풀과 셋](../07-descriptor-sets/README.md) · [다음: 컴퓨트 파이프라인](../09-compute-pipeline/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서 입력 `inputA`, `inputB`와 출력 `output`을 디스크립터 셋에 연결했습니다. 이번에는 각 원소를 더하는 컴퓨트 셰이더(Compute shader)를 작성하고, C++에서 사용할 셰이더 모듈(Shader module)을 만들겠습니다.

셰이더는 GLSL로 작성하고, 빌드할 때 SPIR-V 바이너리로 변환합니다. C++ 프로그램은 이 바이너리를 읽어 Vulkan에 전달합니다. 이번 장의 완료 지점은 모듈 생성이며, GPU 연산은 이후 명령을 기록하고 제출하는 장에서 실행합니다.

[add.comp](add.comp)와 [main.cpp](main.cpp)를 엽니다. 셰이더 파일은 완성된 형태로 제공되므로 먼저 선언과 계산을 살펴봅니다. `main.cpp`에는 07장까지의 코드가 들어 있으며 이번 장에서 추가할 위치에 TODO가 있습니다. 완성된 코드는 공개 후 `examples/99-completed/08-compute-shader.cpp`에서 확인할 수 있습니다.

## 한 번의 셰이더 실행이 맡을 일

CPU에서 배열을 더한다면 반복문으로 `output[i] = inputA[i] + inputB[i]`를 계산할 수 있습니다. 여기서는 셰이더 실행 하나가 인덱스 하나를 맡습니다. 이 실행 하나를 인보케이션(Invocation), 인보케이션의 묶음을 워크그룹(Workgroup)이라고 합니다.

`add.comp`의 맨 위에 있는 다음 선언부터 확인합니다.

```glsl
#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
```

워크그룹 하나에 X축 방향으로 인보케이션 64개를 배치합니다. `64`는 배열 원소 수가 아닙니다. C++에서 사용하는 `elementCount = 8`과 별개이며, 실행할 워크그룹 수는 10장에서 정합니다. 한 그룹을 실행하면 64개 중 8개만 배열 원소를 계산하도록 범위를 검사하겠습니다.

## 디스크립터 바인딩에 맞춰 버퍼 선언하기

워크그룹 선언 아래에서 첫 입력을 정의합니다.

```glsl
layout(set = 0, binding = 0, std430) readonly buffer InputA
{
    float values[];
} inputA;
```

`set = 0`은 사용할 디스크립터 셋 번호, `binding = 0`은 그 셋 안의 바인딩 번호입니다. 07장에서 `inputA.buffer`를 연결한 바인딩과 일치합니다. `buffer` 블록은 스토리지 버퍼의 내용을 선언하고, `readonly`는 이 셰이더에서 해당 내용을 읽기만 한다는 뜻입니다.

`std430`은 블록 안 데이터의 배치 규칙입니다. 여기의 `float values[]`는 원소가 4바이트 간격으로 이어져, C++의 `std::array<float, elementCount>`를 복사한 메모리와 맞습니다. 다른 자료형이나 구조체를 사용하면 해당 형식의 정렬과 원소 간격도 맞춰야 합니다.

그 아래의 두 번째 입력과 출력도 같은 구조입니다.

```glsl
layout(set = 0, binding = 1, std430) readonly buffer InputB
{
    float values[];
} inputB;

layout(set = 0, binding = 2, std430) writeonly buffer Output
{
    float values[];
} resultBuffer;
```

세 블록은 다음과 같이 앞 장의 C++ 코드에 대응합니다. GLSL 변수 이름이 C++ 변수와 같아서 연결되는 것이 아니라, 셋·바인딩 번호로 연결됩니다.

| 셋 | 바인딩 | C++에서 연결한 버퍼 | 셰이더 접근 |
|---|---|---|---|
| 0 | 0 | `inputA.buffer` | `inputA.values[index]` 읽기 |
| 0 | 1 | `inputB.buffer` | `inputB.values[index]` 읽기 |
| 0 | 2 | `output.buffer` | `resultBuffer.values[index]` 쓰기 |

`values[]`는 블록의 마지막에 놓인 길이 미지정 배열입니다. 선언 자체가 메모리를 할당하지 않습니다. 디스크립터가 가리키는 버퍼 영역 안에서만 접근해야 합니다.

## 실제 원소 수 받기

버퍼 선언 아래에는 원소 수를 받는 푸시 상수(Push constant)가 있습니다.

```glsl
layout(push_constant) uniform Parameters
{
    uint count;
} params;
```

푸시 상수는 원소 수처럼 작은 값을 명령 버퍼에 기록해 셰이더에 전달하는 방법입니다. 별도의 버퍼나 디스크립터 바인딩을 추가하지 않고 `count`를 받습니다. C++에서는 32비트 부호 없는 정수인 `std::uint32_t`로 맞추겠습니다.

이 선언만으로 `count`에 8이 들어가지는 않습니다. 09장에서 이 값의 크기와 셰이더 접근 범위를 파이프라인 레이아웃에 정의하고, 10장에서 실제 값을 기록합니다.

## 인덱스를 구하고 범위 검사하기

파일 끝의 `main()`이 셰이더의 진입 함수입니다.

```glsl
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= params.count)
    {
        return;
    }

    resultBuffer.values[index] = inputA.values[index] + inputB.values[index];
}
```

`gl_GlobalInvocationID.x`는 전체 실행에서 이 인보케이션이 맡는 X축 번호입니다. 한 그룹에서는 0~63, 다음 그룹에서는 64~127이 됩니다. `gl_LocalInvocationID.x`를 사용하면 그룹마다 0부터 다시 시작하므로 배열 전체 인덱스로 사용할 수 없습니다.

범위 검사는 버퍼를 읽기 전에 수행합니다. `count = 8`이면 0~7번만 덧셈을 하고 나머지는 종료합니다. 각 인보케이션이 서로 다른 출력 원소에 쓰고 다른 인보케이션의 계산 결과를 읽지 않으므로, 이 셰이더 안에서는 인보케이션 사이의 동기화가 필요하지 않습니다.

## GLSL을 SPIR-V로 빌드하기

00장에서 설치한 `glslc`가 GLSL을 SPIR-V로 변환합니다. 이 장의 [CMakeLists.txt](CMakeLists.txt)에는 셰이더 빌드 규칙이 준비돼 있습니다. 예제를 빌드하면 `add.comp`의 변경을 확인해 `build/add.comp.spv`를 생성합니다.

컴파일 명령에 사용하는 옵션은 다음과 같습니다.

```sh
glslc --target-env=vulkan1.3 --target-spv=spv1.3 add.comp -o build/add.comp.spv
```

위 명령을 따로 실행할 필요는 없습니다. 아래의 CMake 빌드가 같은 작업을 수행합니다. Vulkan 대상 환경은 1.3으로, 출력 SPIR-V 버전은 1.3으로 지정합니다. 고정 워크그룹 크기를 사용하는 이 예제에서는 별도의 `maintenance4` 기능 활성화가 필요한 `LocalSizeId`를 피하도록 출력 버전을 지정했습니다.

CMake는 생성한 바이너리의 절대 경로를 `ADD_SHADER_PATH`라는 C++ 컴파일 정의로 전달합니다. 실행 프로그램은 이 경로를 사용하므로 터미널의 작업 디렉터리를 기준으로 셰이더 파일을 찾지 않습니다. 빌드 폴더를 다른 곳으로 옮겼다면 새 경로에서 CMake를 다시 구성하고 빌드합니다.

## C++에서 셰이더 모듈 만들기

이제 [main.cpp](main.cpp)로 돌아갑니다. 파일 위쪽의 헤더 목록에 파일 입력을 위한 헤더를 추가합니다. `<vector>`와 `<cstdint>`는 앞 장 코드에 이미 있습니다.

```cpp
#include <fstream>
```

`private` 영역의 `descriptorSets` 멤버 바로 아래에 셰이더 모듈을 보관할 자리를 추가합니다.

```cpp
vk::raii::ShaderModule shaderModule = nullptr;
```

클래스 끝의 TODO 위치에 다음 함수의 틀을 추가합니다.

```cpp
bool createShaderModule()
{
}
```

함수 안에서 CMake가 지정한 파일을 바이너리 모드로 엽니다. `std::ios::ate`는 파일을 연 뒤 읽기 위치를 끝으로 옮기므로 파일 크기를 확인할 수 있습니다.

```cpp
std::ifstream file(ADD_SHADER_PATH, std::ios::ate | std::ios::binary);
```

파일 열기에 실패하면 사용한 경로를 출력하고 초기화를 중단합니다. 파일을 여는 코드 바로 아래에 추가합니다.

```cpp
if (!file)
{
    std::cerr << "Cannot open shader: " << ADD_SHADER_PATH << '\n';
    return false;
}
```

이어서 파일 끝의 위치로 바이트 크기를 얻고 검사합니다.

```cpp
const std::streamsize byteCount = file.tellg();
if (byteCount <= 0 || byteCount % sizeof(std::uint32_t) != 0)
{
    std::cerr << "Invalid SPIR-V byte count.\n";
    return false;
}
```

SPIR-V는 32비트 워드로 구성되므로 크기가 0보다 큰 4바이트 배수여야 합니다. 이 검사는 빈 파일이나 잘못된 길이를 걸러내며, 파일 내용 전체가 유효한 SPIR-V인지를 검사하는 것은 아닙니다.

## 바이너리를 32비트 배열로 읽기

같은 함수에 바이너리를 담을 배열을 추가합니다. `byteCount`는 바이트 수이므로 `sizeof(std::uint32_t)`로 나눠 원소 수를 정합니다.

```cpp
std::vector<std::uint32_t> code(static_cast<std::size_t>(byteCount) / sizeof(std::uint32_t));
file.seekg(0);
```

`std::vector<std::uint32_t>`를 사용하면 뒤에서 Vulkan에 전달할 포인터의 타입과 정렬을 맞출 수 있습니다. 파일 끝에서 크기를 읽었으므로 `seekg(0)`으로 다시 처음으로 이동합니다.

이어서 전체 바이트를 읽습니다.

```cpp
if (!file.read(reinterpret_cast<char*>(code.data()), byteCount))
{
    std::cerr << "Cannot read SPIR-V file.\n";
    return false;
}
```

`read()`는 바이트를 받을 `char*`를 요구하므로 여기서만 포인터를 변환합니다. 저장 공간 자체는 32비트 정수 배열이며, 읽기 실패 시 불완전한 데이터를 Vulkan에 전달하지 않습니다.

## SPIR-V를 모듈에 전달하기

바이너리 읽기가 끝난 아래에 모듈 생성 정보를 추가합니다.

```cpp
vk::ShaderModuleCreateInfo moduleInfo{};
moduleInfo.codeSize = static_cast<std::size_t>(byteCount);
moduleInfo.pCode = code.data();
```

`pCode`는 32비트 워드 배열을 가리키지만 `codeSize`의 단위는 바이트입니다. 여기에 `code.size()`를 그대로 넣으면 크기가 네 배 작아집니다.

다음으로 모듈을 생성하고 결과를 출력합니다.

```cpp
shaderModule = vk::raii::ShaderModule(device, moduleInfo); // C API: vkCreateShaderModule
std::cout << "Shader module created: " << moduleInfo.codeSize << " bytes.\n";
return true;
```

모듈 생성이 끝나면 지역 배열 `code`는 소멸해도 됩니다. 모듈은 멤버에 남아 다음 장의 파이프라인 생성에 사용됩니다.

## 초기화 순서에 연결하기

`initVulkan()` 끝의 `&& createDescriptorSets();` 한 줄을 다음 두 줄로 바꿉니다.

```cpp
    && createDescriptorSets()
    && createShaderModule();
```

디스크립터 셋까지 준비한 뒤 SPIR-V를 읽고 모듈을 생성합니다. 다음 장에서는 이 모듈과 셰이더의 리소스 구조를 묶어 컴퓨트 파이프라인을 만들겠습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/08-compute-shader
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target compute_shader_example
.\build\Debug\main.exe
```

완성본이 있다면 같은 구성에서 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target compute_shader_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target compute_shader_example
./build/main
```

완성본이 있다면 같은 구성에서 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target compute_shader_completed
./build/completed/main
```

### 실행 결과

셰이더 모듈 생성 메시지와 SPIR-V의 바이트 크기가 출력되고 검증 레이어 오류가 없는지 확인합니다. 바이너리 크기는 컴파일러에 따라 달라질 수 있습니다. `build/add.comp.spv`가 존재하고 크기가 0보다 큰지도 확인합니다.

출력 버퍼의 덧셈 결과는 아직 확인하지 않습니다. 모듈을 만들기만 했으므로 GPU는 셰이더를 실행하지 않았습니다. 이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

참고: [GLSL 변수·타입과 메모리 배치](https://docs.vulkan.org/glsl/latest/chapters/variables.html), [Vulkan Guide: Push Constants](https://docs.vulkan.org/guide/latest/push_constants.html), [VkShaderModuleCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkShaderModuleCreateInfo.html), [glslc 옵션](https://github.com/google/shaderc/blob/main/glslc/README.asciidoc).
