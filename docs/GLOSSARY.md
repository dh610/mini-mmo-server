# 용어

이전 세션에서 사용자가 이해를 확인한 용어들. 다시 설명할 필요는 없지만 참조용.

## 네트워크

| 용어 | 뜻 |
|---|---|
| **framing** | 바이트 스트림을 패킷 경계로 자르는 것. TCP가 경계를 안 알려주므로 길이 헤더가 필요 |
| **partial read / write** | `recv`/`send`가 한 번에 전부 처리하지 않는 것. 루프로 감싸야 함 |
| **backpressure** | 수신자가 느려서 송신 큐가 쌓이는 현상 |
| **batching** | 여러 entity 정보를 한 패킷에 묶어 보내기. syscall 수는 줄지만 **바이트는 안 줄어든다** |

## AOI

| 용어 | 뜻 |
|---|---|
| **AOI (Area of Interest)** | 이 entity에게 지금 의미 있는 정보의 공간적 범위. 분야 전체를 interest management라고도 함 |
| **broad phase** | 공간 자료구조로 후보를 대충 좁히는 단계. 싸고 부정확. **false positive는 허용, false negative는 금지** |
| **narrow phase** | 실제 좌표 비교로 판정하는 단계. 정확하지만 개당 비쌈 |
| **precision** | K / candidates. broad phase가 얼마나 정확했나 |
| **K** | tick당 평균 수신자 수. **설정값이 아니라 측정값.** 맵·N·AOI 크기에서 결과로 나온다 |
| **cell transition** | entity가 셀 경계를 넘어 소속 셀이 바뀌는 것 |
| **border thrashing** | 셀 경계를 오가며 transition이 폭증하는 현상. 격자 AOI 고유의 약점 |
| **entity cap** | AOI 안 인원에 상한을 두고 가까운 M명만 보내는 기법. hotspot 대응 |
| **spatial hashing** | 셀을 미리 할당하지 않고 해시맵에 저장. 거대·희소 맵용. 이 프로젝트에서는 기각 |

## 동시성

| 용어 | 뜻 |
|---|---|
| **atomicity** | 전부 되거나 전혀 안 되거나. **락은 이걸 얻는 수단이지 목적이 아니다** |
| **lock ordering** | 락을 항상 같은 순서로 잡아 데드락을 회피하는 것 |
| **deadlock** | A가 1번 잡고 2번을 기다리고, B가 2번 잡고 1번을 기다리는 교착 |

## 게임 루프

| 용어 | 뜻 |
|---|---|
| **tick** | 상태를 한 번 갱신하는 주기 |
| **tick budget** | tick 하나에 허용된 시간. 20Hz면 50ms |
| **tick overrun** | 예산 초과. 게임이 밀리는 것 |
| **snapshot** | 한 클라이언트에게 보낼 "지금 네 주변 상태" |

## 핵심 관계식

```
naive 전송량   = N × (N-1) × 상태크기          O(N²)
AOI 전송량     = N × K × 상태크기               K가 상수면 O(N)

K              = N × AOI면적 / 맵면적           (uniform 분포 가정)
맵 한 변       = sqrt(N × AOI면적 / K)
셀당 평균 인원  = N / (맵한변/cell)²
candidates     = 셀당 인원 × 9                  (3×3 순회)
precision      = AOI면적 / (3×cell)²            정사각 AOI면 44.4%
```

**저장은 O(N)이고 배포가 O(N²)이다.** 단일 프로세스에서는 참조 하나로 공유하면 공짜지만,
분산 환경에서는 수신자마다 물리적 복사본을 밀어 넣어야 한다. 그게 이 프로젝트가 다루는 비용.
