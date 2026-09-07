# Cross-Platform-AI



## Problem statement

현재 GPU 추론 스택은 CUDA(NVIDIA), ROCm(AMD)처럼 벤더에 묶여 있습니다. 이 프로젝트에서는 벤더를
가리지 않는 Vulkan으로 LLM/VLM 추론 코드를 직접 짜고 하나의 코드베이스를 러너마다 다른 GPU에서
빌드해 돌립니다. 그 결과를 공유하고 직접 짠 GPU 커널을 최적화해가며 격차를 좁히는 경험까지 합니다.

## Goal

1. Vulkan으로 GPU 컴퓨트 코드를 처음부터 짤 수 있게 됩니다.
2. GPU 커널을 직접 구현, 최적화하는 능력을 갖추게 됩니다.
3. 온디바이스 추론에서 병목, 오류가 생기는 지점을 스스로 진단하는 능력을 얻습니다.
4. GPU마다 다르게 동작하는 지점을 찾아 대응하는 법을 익히게 됩니다.
5. 오픈소스 프로젝트를 기획부터 공개까지 끌고 가는 경험을 얻습니다.

## Weekly Roadmap

방학 12/20~1/2. 방학 직후가 Grand Gathering 주이므로 산출물은 11주차까지 목표로 합니다.

| 주차 | 기간 | 미팅 내용 |
|---|---|---|
| 1 | 10/4~10/10 | 오리엔테이션, Vulkan 개발환경 구축 |
| 2 | 10/11~10/17 | 컴퓨트 파이프라인 최소 뼈대: Device, Queue, 버퍼, 메모리, 디스크립터셋 |
| 3 | 10/18~10/24 | 컴퓨트 셰이더: GLSL 벡터 덧셈, SPIR-V 컴파일, 디스패치 |
| 4 | 10/25~10/31 | llama.cpp Vulkan 백엔드로 각자 GPU에서 LLM/VLM 구동, 기록 양식 확정 |
| 5 | 11/1~11/7 | 러너 간 크로스 실행: 결과 수집, 능력별 결과 행렬 작성 |
| 6 | 11/8~11/14 | 커널(예: 행렬곱) 구현 |
| 7 | 11/15~11/21 | 자기 커널 vs 최적화 커널 비교, 최적화 적용, 결과 공개, PR |
| 8 | 11/22~11/28 | 실행 프로젝트 스코프 확정, 개발: 모델 로드, 다운로드 |
| 9 | 11/29~12/5 | 개발: 입력 전처리, 추론 연결 |
| 10 | 12/6~12/12 | 개발: 출력 표시, 명령 1~2개로 E2E 통합 |
| 11 | 12/13~12/19 | 측정 기록 병합, 레포 정리: README, 설치 안내, 기여 가이드 |
| - | 12/20~1/2 | 방학 |
| 12 | 1/3~1/9 | 최종 점검, 공개, Grand Gathering 발표 |

## Team

TBD

## Acknowledgement

이 프로젝트는 가짜연구소 Open Academy로 진행됩니다. 여러분의 참여와 기여가 '우연한 혁명(Serendipity Revolution)'을 가능하게 합니다. 모두에게 깊은 감사를 전합니다.

This project is developed as part of Pseudo-Lab's Open Research Initiative. Special thanks to our contributors and the open source community for their valuable insights and contributions.

### About Pseudo Lab

Pseudo-Lab is a non-profit organization focused on advancing machine learning and AI technologies. Our core values of Sharing, Motivation, and Collaborative Joy drive us to create impactful open-source projects. With over 5k+ researchers, we are committed to advancing machine learning and AI technologies.

## License

This project is licensed under the MIT License.
