# 수정한 함수 정리

지금까지 우리가 손본 함수들을 빠르게 확인하려고 정리한 문서입니다.

## 1. `pintos/devices/timer.c`

- `timer_sleep`
  - busy waiting 대신 스레드를 재우는 방식으로 변경
- `timer_interrupt`
  - 깨울 시간이 된 스레드를 깨우도록 변경

## 2. `pintos/threads/thread.c`

- `thread_create`
  - 더 높은 실제 우선순위의 스레드가 생기면 현재 스레드가 양보
- `thread_wakeup_less`
  - `sleep_list`를 `wakeup_tick` 기준으로 정렬하는 comparator
- `thread_priority_more`
  - ready queue와 waiter 정렬을 `effect_priority` 기준으로 수행
- `donation_priority_more`
  - donor를 `effect_priority` 기준으로 정렬하는 comparator
- `thread_refresh_priority`
  - base priority와 donor 최고 priority를 비교해 `effect_priority` 재계산
- `thread_unblock`
  - READY queue에 priority 순으로 삽입
- `thread_sleep`
  - 현재 스레드를 `sleep_list`에 넣고 block
- `thread_wake`
  - 깨울 시간이 된 sleeping thread를 unblock
- `thread_yield`
  - 현재 스레드를 ready queue에 다시 넣고 양보
- `thread_set_priority`
  - base priority를 바꾸고 `effect_priority`를 다시 계산
- `thread_get_priority`
  - 현재 스레드의 `effect_priority` 반환
- `init_thread`
  - donation 관련 필드 초기화

## 3. `pintos/threads/synch.c`

- `sema_down`
  - semaphore waiters를 priority 순으로 삽입
- `sema_up`
  - waiter를 깨우기 전에 다시 정렬해서 가장 높은 priority thread를 깨움
- `lock_acquire`
  - donation 시작, `waiting_lock` 기록, donor를 `donation_list`에 추가
- `propagate_donation`
  - nested/chain donation을 위해 `waiting_lock` 체인을 따라 priority 전파
- `lock_release`
  - 현재 lock과 관련된 donor만 제거하고 priority 재계산
- `thread_semaphore_more`
  - condition variable waiters 정렬용 comparator
- `cond_wait`
  - waiter priority를 저장하고 priority 순으로 대기
- `cond_signal`
  - 가장 높은 priority waiter를 깨움

## 4. `pintos/include/threads/thread.h`

함수는 아니지만 donation 구현을 위해 아래 필드를 추가하거나 확장했습니다.

- `effect_priority`
- `donation_list`
- `waiting_lock`
- `donation_elem`
