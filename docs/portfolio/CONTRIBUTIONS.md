# Contributions — Phase 1

이 문서는 커밋 수를 기여량으로 환산하지 않고, 최종 `main`에 포함된 작업과 병합되지 않은 실험 브랜치를 구분합니다.

## NearthYou — 개인 주도

| Scope | Evidence | Main status |
| --- | --- | --- |
| exec/wait load·exit synchronization and lifecycle cleanup | [PR #166](https://github.com/NearthYou/pintos_lab/pull/166), [`98ea467`](https://github.com/NearthYou/pintos_lab/commit/98ea467a1e13d59a951c2aabde7cd11424c7aba7) | merged |
| argument-related regression fixes | [`d977178`](https://github.com/NearthYou/pintos_lab/commit/d977178665cf88c75659fb6b95b283b9eb56a443) | merged |
| kernel-thread `process_exit` guard and scheduling safety | [PR #171](https://github.com/NearthYou/pintos_lab/pull/171), [`9c87f94`](https://github.com/NearthYou/pintos_lab/commit/9c87f9431cf61a999935055a6c3b17f29b1a7691) | merged |

## 팀 통합

Alarm clock, priority scheduling/donation, MLFQS, argument passing, syscall dispatch와 user-memory validation은 여러 팀원의 PR과 리뷰를 거쳐 통합되었습니다. 전체 기능을 특정 개인의 단독 구현으로 표현하지 않습니다.

## 병합되지 않은 탐색 작업

NearthYou가 작성한 alarm/priority scheduling [PR #42](https://github.com/NearthYou/pintos_lab/pull/42)와 관련 커밋은 저장소 이력에는 남아 있지만 최종 `main`의 조상이 아닙니다. **PR #42는 병합되지 않았습니다.** 최종 팀 구현 근거는 [PR #43](https://github.com/NearthYou/pintos_lab/pull/43)을 따릅니다.
