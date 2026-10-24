# 09. 컴퓨트 파이프라인

[이전: 컴퓨트 셰이더](../08-compute-shader/README.md) · [다음: 커맨드 버퍼](../10-command-buffers/README.md) · [전체 튜토리얼](../../README.md#튜토리얼)

이전 장에서 벡터 덧셈 셰이더를 SPIR-V로 컴파일하고 셰이더 모듈을 만들었습니다. 이번에는 셰이더가 사용할 리소스 구조와 진입 함수를 지정해 컴퓨트 파이프라인(Compute pipeline)을 만들겠습니다.

파이프라인 레이아웃(Pipeline layout)은 디스크립터 셋 레이아웃과 푸시 상수의 범위를 정의합니다. 컴퓨트 파이프라인은 이 레이아웃과 셰이더의 진입 함수를 묶어 GPU에서 실행할 연산을 준비합니다. 생성 과정에 장치별 컴파일·최적화 비용이 들 수 있으므로 보통 한 번 만든 파이프라인을 여러 디스패치에서 재사용합니다. [파이프라인과 컴파일](https://docs.vulkan.org/guide/latest/pipeline_cache.html)

[main.cpp](main.cpp)에는 08장까지의 코드가 들어 있습니다. 아래 순서대로 두 객체와 생성 함수를 추가합니다. 완성된 코드는 공개 후 `examples/99-completed/09-compute-pipeline.cpp`에서 확인할 수 있습니다. [add.comp](add.comp)와 셰이더 빌드 규칙은 이전 장과 같습니다.

## 두 레이아웃의 역할 구분하기

06장의 디스크립터 셋 레이아웃은 셋 하나 안의 바인딩을 정의했습니다. 이번 장의 파이프라인 레이아웃은 그 셋을 셰이더의 어느 셋 번호에 놓을지, 푸시 상수는 어느 범위를 사용할지 정합니다.

| 객체 | 이 예제에서 정의하는 내용 |
|---|---|
| `descriptorSetLayout` | 바인딩 0, 1, 2에 각각 스토리지 버퍼 하나 |
| `pipelineLayout` | 셋 0의 레이아웃, 컴퓨트 단계에서 읽을 4바이트 푸시 상수 |
| `computePipeline` | 위 레이아웃을 사용하는 셰이더 모듈의 `main` 진입 함수 |

디스크립터 셋에는 실제 버퍼 참조가 들어 있고, 파이프라인 레이아웃에는 그 구조만 들어갑니다. 이렇게 분리하면 같은 바인딩 구조를 유지하는 한 입력·출력 버퍼가 달라져도 파이프라인을 다시 만들 필요가 없습니다. 10장에서 명령을 기록할 때 파이프라인과 디스크립터 셋을 각각 바인딩합니다.

## 파이프라인을 보관할 자리 만들기

`private` 영역의 `shaderModule` 멤버 바로 아래에 다음 두 멤버를 선언합니다.

```cpp
vk::raii::PipelineLayout pipelineLayout = nullptr;
vk::raii::Pipeline computePipeline = nullptr;
```

빈 RAII 객체로 시작해 초기화 함수에서 실제 객체를 저장합니다. 클래스의 `private` 영역 끝에 파이프라인 레이아웃 생성 함수의 틀을 추가합니다.

```cpp
bool createPipelineLayout()
{
}
```

## 푸시 상수의 범위 정의하기

셰이더에는 `uint count` 하나를 담은 푸시 상수 블록이 있습니다. 이 값은 버퍼의 바이트 크기가 아니라 연산할 원소 수입니다. 함수 안에 C++ 쪽에서 사용할 범위를 선언합니다.

```cpp
vk::PushConstantRange pushConstantRange{};
pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
pushConstantRange.offset = 0;
pushConstantRange.size = sizeof(uint32_t);
```

`stageFlags`는 값을 읽을 셰이더 단계, `offset`은 푸시 상수 영역 안의 시작 바이트, `size`는 사용할 바이트 수입니다. 이 예제의 GLSL `uint`와 C++ `uint32_t`는 4바이트이므로 오프셋 0부터 4바이트를 컴퓨트 단계에 노출합니다.

범위를 정의하는 것과 값을 전달하는 것은 별개입니다. 이 코드는 `count`의 저장 범위를 선언하고, `elementCount`의 실제 값은 10장에서 커맨드 버퍼에 기록합니다. 푸시 상수의 `offset`과 `size`는 4바이트 배수여야 하며, 사용할 범위는 장치의 `maxPushConstantsSize` 안에 있어야 합니다. 여기서는 `uint` 하나만 사용합니다.

## 셋 0의 레이아웃 연결하기

같은 함수에서 앞 장의 RAII 디스크립터 셋 레이아웃 핸들을 꺼내 생성 정보에 연결합니다.

```cpp
const vk::DescriptorSetLayout layout = *descriptorSetLayout;
vk::PipelineLayoutCreateInfo layoutInfo{};
layoutInfo.setLayoutCount = 1;
layoutInfo.pSetLayouts = &layout;
```

`pSetLayouts`는 셋 번호 순서대로 놓인 레이아웃 배열입니다. 원소 하나를 넘기면 그 레이아웃이 셋 0에 대응합니다. 이는 `add.comp`에 적은 `layout(set = 0, binding = ...)`와 맞아야 합니다.

이어서 앞에서 만든 푸시 상수 범위를 연결합니다.

```cpp
layoutInfo.pushConstantRangeCount = 1;
layoutInfo.pPushConstantRanges = &pushConstantRange;
```

`setLayoutCount`는 셋 레이아웃의 개수입니다. 바인딩 세 개를 사용한다고 여기에 3을 넣지 않습니다. `pushConstantRangeCount` 역시 바이트 수가 아니라 범위 정보의 개수입니다.

## 파이프라인 레이아웃 생성하기

생성 정보를 완성했으니 논리 디바이스에서 레이아웃을 생성합니다.

```cpp
pipelineLayout = vk::raii::PipelineLayout(device, layoutInfo); // C API: vkCreatePipelineLayout
```

이 호출이 끝나면 지역 변수 `layout`과 `pushConstantRange`는 더 보관할 필요가 없습니다. Vulkan은 생성 시 전달한 구조를 사용합니다.

함수 마지막에 성공을 반환합니다.

```cpp
return true;
```

## 실행할 컴퓨트 셰이더 지정하기

방금 작성한 함수 아래에 컴퓨트 파이프라인 생성 함수의 틀을 추가합니다.

```cpp
bool createComputePipeline()
{
}
```

함수 안에서 셰이더 단계 정보를 작성합니다.

```cpp
vk::PipelineShaderStageCreateInfo stageInfo{};
stageInfo.stage = vk::ShaderStageFlagBits::eCompute;
stageInfo.module = *shaderModule;
stageInfo.pName = "main";
```

`stage`는 컴퓨트 단계, `module`은 이전 장에서 만든 셰이더 모듈의 핸들입니다. `pName`은 SPIR-V 안의 진입 함수 이름으로, `add.comp`의 `void main()`에 대응합니다. 셰이더 파일명이나 C++의 `main()`을 지정하는 필드가 아닙니다.

이어서 셰이더 단계와 파이프라인 레이아웃을 하나의 생성 정보로 묶습니다.

```cpp
vk::ComputePipelineCreateInfo pipelineInfo{};
pipelineInfo.stage = stageInfo;
pipelineInfo.layout = *pipelineLayout;
```

컴퓨트 파이프라인에는 컴퓨트 셰이더 단계 하나를 지정합니다. 배열 연산을 직접 실행하므로 그래픽스 파이프라인의 정점 입력과 래스터라이제이션 설정이 필요하지 않습니다. 이 예제에서 쓰는 스토리지 버퍼 바인딩과 푸시 상수는 모두 `pipelineInfo.layout`이 설명하는 구조와 일치해야 합니다.

이제 RAII 생성자에 논리 디바이스와 생성 정보를 전달합니다.

```cpp
computePipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo); // C API: vkCreateComputePipelines
```

두 번째 인수는 파이프라인 캐시입니다. 이 예제에서는 캐시 객체를 사용하지 않으므로 `nullptr`를 전달합니다. 캐시는 파이프라인 생성 결과를 재사용하는 데 쓰이며, 명령을 실행하거나 동기화하는 객체는 아닙니다.

함수 마지막에 생성 결과를 출력하고 성공을 반환합니다.

```cpp
std::cout << "Compute pipeline created.\n";
return true;
```

셰이더 모듈은 파이프라인 생성에 사용됩니다. 생성이 끝난 뒤에는 해제할 수 있지만, 이 예제에서는 앞 장의 RAII 멤버 구조를 유지해 프로그램 종료 때 함께 정리합니다.

## 초기화 순서에 연결하기

`initVulkan()` 끝의 `&& createShaderModule();` 한 줄을 다음 세 줄로 바꿉니다.

```cpp
    && createShaderModule()
    && createPipelineLayout()
    && createComputePipeline();
```

셰이더 모듈과 파이프라인 레이아웃이 준비된 다음 파이프라인을 생성합니다. 여기까지는 실행할 프로그램과 리소스 구조를 정의한 것입니다. 다음 장에서 이 파이프라인, 디스크립터 셋, 원소 수를 커맨드 버퍼에 기록하겠습니다.

## 빌드와 실행

[환경 설정](../00-env-settings/README.md)을 마친 뒤, 저장소 루트에서 예제 폴더로 이동합니다.

```sh
cd examples/09-compute-pipeline
```

### Windows

Visual Studio 생성기를 기준으로 빌드하고 실행합니다.

```powershell
cmake -S . -B build
cmake --build build --config Debug --target compute_pipeline_example
.\build\Debug\main.exe
```

완성본이 있다면 같은 구성에서 다음 명령으로 빌드하고 실행합니다.

```powershell
cmake --build build --config Debug --target compute_pipeline_completed
.\build\completed\Debug\main.exe
```

### Linux

Makefiles 또는 Ninja를 기준으로 빌드하고 실행합니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target compute_pipeline_example
./build/main
```

완성본이 있다면 같은 구성에서 다음 명령으로 빌드하고 실행합니다.

```sh
cmake --build build --target compute_pipeline_completed
./build/completed/main
```

### 실행 결과

`Shader module created: ... bytes.` 다음에 `Compute pipeline created.`가 출력되는지 확인합니다. 검증 레이어가 셰이더의 바인딩이나 푸시 상수 범위 불일치를 보고한다면, 06장의 바인딩과 이 장의 레이아웃을 `add.comp` 선언에 대조합니다.

파이프라인 생성은 연산 실행과 별개입니다. 이번 장에서는 명령을 큐에 제출하지 않으므로 출력 버퍼는 초기값 그대로입니다. 다음 장에서 이 파이프라인을 사용한 명령을 기록합니다.

이 장의 코드를 완성한 뒤, 실행 결과가 보이도록 터미널을 캡처해 제출합니다.

---

참고: [VkPipelineLayoutCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineLayoutCreateInfo.html), [VkPushConstantRange](https://docs.vulkan.org/refpages/latest/refpages/source/VkPushConstantRange.html), [Vulkan Guide: Push Constants](https://docs.vulkan.org/guide/latest/push_constants.html), [VkComputePipelineCreateInfo](https://docs.vulkan.org/refpages/latest/refpages/source/VkComputePipelineCreateInfo.html).
