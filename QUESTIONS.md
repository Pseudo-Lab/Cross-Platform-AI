# 튜토리얼 질문과 답변

## GLSL에서 여러 진입점을 만들 수 있나요?

질문 출처: [09. 컴퓨트 파이프라인 — 실행할 컴퓨트 셰이더 지정하기](examples/09-compute-pipeline/README.md#실행할-컴퓨트-셰이더-지정하기)

**답변:** GLSL 셰이더 실행 단위의 진입점은 `void main()` 하나입니다. 보조 함수를 여러 개 정의할 수 있지만, `pName`에 그 함수 이름을 넣는다고 진입점이 되지는 않습니다.

SPIR-V 모듈은 여러 진입점을 담을 수 있습니다. Vulkan의 `pName`은 그중 사용할 `OpEntryPoint` 이름을 고르며, 해당 진입점의 실행 모델은 `stage`와 일치해야 합니다. 따라서 GLSL의 진입점 규칙과 SPIR-V 모듈의 다중 진입점 지원을 구분해야 합니다. 이 예제는 `add.comp`의 `main`을 컴파일했으므로 `stageInfo.pName = "main"`을 사용합니다.

답변 근거: [GLSL 명세 — Function Definitions](https://docs.vulkan.org/glsl/latest/chapters/statements.html#function-definitions), [Vulkan Guide — Ways to Provide SPIR-V](https://docs.vulkan.org/guide/latest/ways_to_provide_spirv.html), [VkPipelineShaderStageCreateInfo — pName](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineShaderStageCreateInfo.html).

## 워크그룹의 인보케이션 수는 개발자가 정하고, warp 크기는 하드웨어가 정하나요?

질문 출처: [10. 커맨드 버퍼 — 기록 시작하기](examples/10-command-buffers/README.md#기록-시작하기)

**답변:** 워크그룹 크기는 개발자가 정합니다. 이 예제의 `local_size_x = 64`는 Y와 Z가 각각 1이므로 그룹당 인보케이션 64개를 뜻합니다. 각 축은 `maxComputeWorkGroupSize`를, 세 축의 곱은 `maxComputeWorkGroupInvocations`를 넘으면 안 됩니다. C++의 `workgroupSize`는 이 크기에 맞춰 디스패치할 그룹 수를 계산하는 값입니다.

CUDA의 block에 대응하는 개념은 워크그룹이며, warp에 대응하는 Vulkan 개념은 **서브그룹(subgroup)**입니다. 서브그룹 크기는 장치와 드라이버 구현에 의존합니다. 워크그룹을 64개로 정했다고 서브그룹도 64개가 되는 것은 아닙니다.

다만 서브그룹 크기가 장치마다 항상 하나로 고정된다는 설명은 정확하지 않습니다. `subgroupSizeControl` 기능을 지원하고 활성화했으며 해당 셰이더 단계가 크기 지정을 지원하면, 지원 범위 안의 크기를 `VkPipelineShaderStageRequiredSubgroupSizeCreateInfo`로 요청할 수 있습니다. 이 튜토리얼은 크기를 요청하지 않으므로 구현이 정한 서브그룹 크기를 사용합니다.

답변 근거: [VkPhysicalDeviceLimits — 워크그룹 한계](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceLimits.html), [Vulkan Tutorial — Workgroups and Invocations](https://docs.vulkan.org/tutorial/latest/Advanced_Vulkan_Compute/02_Compute_Architecture/02_workgroups_and_invocations.html), [VK_EXT_subgroup_size_control](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_subgroup_size_control.html), [VkPipelineShaderStageCreateInfo — 크기 지정 조건](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineShaderStageCreateInfo.html).

## 범용 큐를 사용하면 다른 bind point를 쓰나요? eGeneral도 있나요?

질문 출처: [10. 커맨드 버퍼 — 파이프라인과 디스크립터 셋 바인딩하기](examples/10-command-buffers/README.md#파이프라인과-디스크립터-셋-바인딩하기)

**답변:** 그래픽스와 컴퓨트를 모두 지원하는 큐에서도 컴퓨트 파이프라인은 `vk::PipelineBindPoint::eCompute`로 바인딩합니다. `vk::PipelineBindPoint::eGeneral`은 없습니다. Bind point는 명령이 사용할 파이프라인의 종류를 지정합니다.

같은 커맨드 버퍼에서 그래픽스 파이프라인은 `eGraphics`, 컴퓨트 파이프라인은 `eCompute`에 각각 바인딩할 수 있으며, 한쪽 바인딩이 다른 쪽을 덮어쓰지 않습니다. 큐 패밀리는 기록하고 제출할 연산을 지원해야 합니다. 이 예제에서는 컴퓨트 지원이 필요하고, 파이프라인과 디스크립터 셋 모두 `eCompute`에 바인딩합니다.

답변 근거: [VkPipelineBindPoint](https://docs.vulkan.org/refpages/latest/refpages/source/VkPipelineBindPoint.html), [vkCmdBindPipeline — 바인딩과 큐 지원 조건](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBindPipeline.html).

## eva에서는 synchronization2를 사용하나요?

질문 출처: [11. 디스패치와 동기화 — 결과를 읽기 전에 필요한 것](examples/11-dispatch-synchronization/README.md#결과를-읽기-전에-필요한-것)

**답변:** 이 프로젝트의 eva 서브모듈 커밋 `2a31c83`에서는 `synchronization2`를 활성화하며, 지원하지 않는 장치에서는 오류를 냅니다. `CommandBuffer::barrier()`는 활성화된 Vulkan 1.3 코드 경로에서 `VkDependencyInfo`를 구성해 `vkCmdPipelineBarrier2()`를 호출합니다. 튜토리얼은 eva를 거치지 않고 Vulkan-Hpp를 직접 사용하며, 04장에서 이 기능을 활성화하지 않았으므로 기존 API를 사용합니다.

답변 근거: [eva의 기능 활성화, 필수 기능 검사 및 배리어 구현](external/eva/src/eva-runtime.cpp), [Vulkan 1.3 코드 경로 설정](external/eva/src/eva-runtime.h), [04. 논리 디바이스와 큐](examples/04-logical-device/README.md).

## AccessMask는 무엇인가요?

질문 출처: [11. 디스패치와 동기화 — 호스트 읽기를 위한 배리어 기록하기](examples/11-dispatch-synchronization/README.md#호스트-읽기를-위한-배리어-기록하기)

**답변:** 동기화 대상으로 삼을 메모리 접근 종류를 선택하는 비트 마스크입니다. 접근 권한을 부여하는 설정은 아닙니다. 여러 접근 종류를 `|`로 묶을 수 있습니다.

| 설정 | 이 예제의 의미 |
|---|---|
| `srcStageMask = eComputeShader` | 앞선 컴퓨트 셰이더 단계 |
| `srcAccessMask = eShaderWrite` | 그 단계에서 수행한 쓰기 |
| `dstStageMask = eHost` | 이후 호스트 접근 |
| `dstAccessMask = eHostRead` | CPU의 읽기 |

단계 마스크는 어느 단계의 작업을 연결할지, 접근 마스크는 그중 어떤 메모리 접근을 연결할지 지정합니다. 여기서는 `output`에 대한 컴퓨트 셰이더의 쓰기를 CPU의 읽기에 연결합니다.

답변 근거: [VkBufferMemoryBarrier — 접근 범위](https://docs.vulkan.org/refpages/latest/refpages/source/VkBufferMemoryBarrier.html), [vkCmdPipelineBarrier — 단계와 접근 마스크](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier.html).

## 배리어는 왜 필요하며, 펜스 및 세마포어와 어떻게 다른가요? 종류도 있나요?

질문 출처: [11. 디스패치와 동기화 — 호스트 읽기를 위한 배리어 기록하기](examples/11-dispatch-synchronization/README.md#호스트-읽기를-위한-배리어-기록하기)

**답변:** 배리어는 지정한 앞선 작업과 뒤의 작업 사이에 실행 및 메모리 의존성을 만듭니다. 예를 들어 디스패치 A가 쓴 버퍼를 B가 읽는다면, 순서대로 기록하는 것만으로는 필요한 의존성이 모두 보장되지 않습니다. A의 쓰기와 B의 읽기를 연결해야 합니다. 마지막에 CPU가 펜스를 기다려도 A와 B 사이에 빠진 의존성이 생기지는 않습니다.

```text
같은 큐: 디스패치 A의 쓰기 → 배리어 → 디스패치 B의 읽기
```

배리어가 큐에서 동작한다는 것은 커맨드 버퍼에 기록한 배리어를 제출하면 그 큐의 작업에 동기화 규칙이 적용된다는 뜻입니다. 배리어 기록 함수 자체가 CPU를 기다리게 하지는 않습니다. 11장은 결과를 CPU가 읽으므로 다음과 같이 연결합니다.

```text
GPU: 디스패치 → 호스트 읽기용 배리어 → 제출 완료, 펜스 신호
CPU: 제출 → 펜스 대기 → output 읽기
```

배리어는 GPU 쓰기를 호스트 읽기에 연결하고, 펜스 대기는 CPU가 완료 전에 읽지 않도록 합니다. `HOST_COHERENT` 메모리에서는 읽기 전 명시적인 캐시 무효화 호출을 생략할 수 있습니다.

| 수단 | 의존성을 연결하는 방식 |
|---|---|
| 배리어 | 커맨드 버퍼에 기록해 같은 큐의 앞뒤 작업을 연결하며, 호스트 접근과의 메모리 의존성도 지정 |
| 세마포어 | 큐 제출의 signal/wait로 의존성을 연결하며, 다른 큐의 작업도 연결 가능 |
| 펜스 | 제출 완료를 CPU가 확인하거나 기다림 |

세마포어도 실행 순서와 메모리 의존성을 제공합니다. 다른 큐에 배리어만 넣어서는 두 큐 사이의 대기 관계가 생기지 않으므로 세마포어로 연결합니다. 적절한 signal/wait가 있으면 단순 버퍼 전달에 추가 메모리 배리어가 필요 없을 수 있습니다. 이미지 레이아웃 전환이나 서로 다른 큐 패밀리 사이의 exclusive 리소스 소유권 이전이 필요하면 배리어를 함께 사용합니다. 세마포어는 같은 큐의 제출 사이에서도 사용할 수 있으며, 타임라인 세마포어는 호스트의 signal/wait도 지원합니다.

파이프라인 배리어에 전달하는 메모리 배리어는 대상에 따라 구분합니다.

| 종류 | 대상과 기능 |
|---|---|
| `MemoryBarrier` | 특정 리소스 없이 접근 종류로 메모리 범위 지정 |
| `BufferMemoryBarrier` | 버퍼의 바이트 범위 지정, 필요하면 큐 패밀리 소유권 이전 |
| `ImageMemoryBarrier` | 이미지의 하위 리소스 범위 지정, 레이아웃 전환 및 큐 패밀리 소유권 이전 |

11장은 `output` 버퍼 전체를 대상으로 하므로 `BufferMemoryBarrier`를 사용합니다.

답변 근거: [vkCmdPipelineBarrier](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier.html), [CPU readback 및 세마포어 동기화 예제](https://docs.vulkan.org/guide/latest/synchronization_examples.html), [Vulkan 동기화 명세](https://docs.vulkan.org/spec/latest/chapters/synchronization.html).
