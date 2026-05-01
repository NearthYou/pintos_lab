# 1. 슬라이드별 최종 발표 대본

## 슬라이드 1. Pintos Thread 구현 흐름
먼저 이번 주에 구현한 Pintos Thread의 전체 흐름입니다. 큰 방향은 스레드가 기다릴 때 CPU를 낭비하지 않도록 block시키고, 다시 깨어날 때는 우선순위 기준으로 실행되게 만드는 것이었습니다. Alarm clock은 busy waiting 대신 sleep_list를 사용해서, `timer_sleep()`에서는 스레드를 재우고 `timer_interrupt()`에서 깨울 시간이 된 스레드를 깨우도록 구현했습니다. 그다음에는 `ready_list`, `semaphore waiters`, `condition waiters`를 모두 priority 순으로 관리하도록 바꿨고, lock을 기다리는 상황에서는 priority donation이 일어나도록 구현했습니다.

## 슬라이드 2. Priority Donation 구현 구조
Priority Donation에서 가장 중요했던 건 priority를 단순히 숫자 하나로 보지 않는 것이었습니다. 저는 원래 우선순위인 `priority`와, donation까지 반영해서 실제 스케줄러가 보는 `effect_priority`를 분리했습니다. 그리고 donation을 처리하기 위해 `waiting_lock`과 `donation_list`를 추가했습니다. `waiting_lock`은 현재 스레드가 어떤 lock 때문에 기다리고 있는지 기록하는 값이고, `donation_list`는 나에게 donation한 donor 스레드들을 관리하는 역할을 합니다. 이 donor들이 어떤 lock 때문에 donation을 했는지는 donor의 `waiting_lock`을 같이 보면서 구분했습니다. 이렇게 구조를 나눈 이유는 lock을 release할 때 donation을 단순히 없애는 게 아니라, 남아 있는 donor를 기준으로 다시 계산해야 했기 때문입니다.

## 슬라이드 3. Troubleshooting: lock_release() 복구
가장 많이 막혔던 부분은 `lock_release()`에서 donation을 복구하는 방식이었습니다. 처음에는 lock을 놓으면 donation이 끝난다고 생각해서 `effect_priority = priority`처럼 원래 값으로 바로 돌리면 된다고 생각했습니다. 그런데 multiple donation 상황에서는 main thread가 lock A와 lock B를 동시에 잡고 있고, 각각 다른 스레드가 donation을 하고 있을 수 있어서 A를 release했다고 해서 B 때문에 받은 donation까지 사라지면 안 됐습니다. 그래서 수정한 방식은 현재 release하는 lock과 관련된 donor만 `donation_list`에서 제거하고, 남아 있는 donor들과 base priority를 비교해서 `effect_priority`를 다시 계산하는 방식입니다. 이 과정을 통해 donation 복구는 단순 원복이 아니라, 남아 있는 상태를 기준으로 다시 계산하는 문제라는 걸 이해하게 됐습니다.

## 슬라이드 4. 회고
이번에 Pintos의 thread 부분을 진행하면서 개인적으로 세운 목표는 구현 자체를 AI에 맡기지 않는 것이었습니다. 그래서 AI는 테스트 요구사항을 정리하거나, 필요한 개념을 학습하는 용도로만 활용했고 실제 구현과 디버깅은 직접 해보려고 했습니다. 그만큼 진행 속도는 조금 느렸지만, 대신 전보다 종적으로 파고들려고 노력했습니다. 
그런 점에서는 분명히 의미가 있었고, 아직 끝내지 못한 나머지 테스트들도 오늘 계속 도전해볼 예정입니다.


# 3. 질문/답변 세트 10개

## Q1. 왜 `priority`와 `effect_priority`를 나눴나요?
`priority`는 스레드가 원래 가지고 있는 base priority이고, `effect_priority`는 donation까지 반영된 실제 실행 우선순위입니다. donation 중에는 원래 우선순위와 실제 우선순위가 다를 수 있어서 두 값을 분리했습니다.

## Q2. `thread_get_priority()`는 왜 `priority`가 아니라 `effect_priority`를 반환하나요?
스케줄러와 테스트가 실제로 확인해야 하는 값은 donation까지 반영된 현재 우선순위이기 때문입니다. 그래서 현재 실행 관점의 priority는 `effect_priority`가 맞습니다.

## Q3. `waiting_lock`은 왜 필요한가요?
waiting_lock은 현재 스레드가 어떤 lock 때문에 막혀 있는지를 기록하는 값입니다. 우리 구현에서는 특히 lock_release()에서 donor를 전부 지우지 않고, 현재 release하는 lock 때문에 donation한 donor만 제거하기 위해 사용했습니다. 그래서 multiple donation처럼 여러 lock이 동시에 얽히는 상황에서 donation을 정확히 정리하는 기준이 됩니다.`

## Q4. `donation_list`는 왜 필요한가요?
한 스레드가 여러 lock을 들고 있으면 여러 donor에게 동시에 donation을 받을 수 있습니다. 이때 lock 하나를 release한다고 donation 전체를 지우면 안 되기 때문에 donor들을 따로 관리할 구조가 필요했습니다.

## Q5. `lock_release()`에서 왜 그냥 `effect_priority = priority`로 복구하면 안 되나요?
single donation만 보면 맞아 보이지만, multiple donation에서는 아직 남아 있어야 하는 donation까지 같이 사라질 수 있습니다. 그래서 release한 lock과 관련된 donor만 제거하고 다시 계산해야 합니다.

## Q6. 왜 `ready_list`도 priority 순으로 관리해야 하나요?
깨어난 스레드가 READY 상태가 되었을 때, 가장 높은 우선순위의 스레드가 먼저 실행되어야 priority scheduling이 유지되기 때문입니다.

## Q7. 왜 `semaphore waiters`와 `condition waiters`도 priority 순으로 관리했나요?
ready queue만 priority 순이어도, waiters가 FIFO면 깨울 때 우선순위가 깨질 수 있습니다. 그래서 자원을 기다리는 순간부터 깨우는 순간까지 같은 기준을 유지하도록 맞췄습니다.

## Q8. `thread_set_priority()`는 어떤 역할을 하나요?
이 함수는 현재 스레드의 base priority를 바꾸는 함수입니다. 그리고 base priority를 바꾼 뒤에는 donor들을 포함해서 `effect_priority`를 다시 계산하도록 구현했습니다.

## Q9. `thread_create()` 비교를 왜 `effect_priority` 기준으로 바꿨나요?
donation을 받은 스레드는 base priority보다 실제 우선순위가 더 높을 수 있기 때문입니다. 생성 직후의 선점 여부도 실제 우선순위를 기준으로 판단해야 자연스럽습니다.

## Q10. 지금 어디까지 구현했나요?
현재는 donation 테스트 중 `donate-one`, `donate-lower`, `donate-multiple`, `donate-multiple2`까지 맞춘 상태입니다. 다음 단계는 `nest`, `chain`, `sema`처럼 donation 전파가 더 복잡한 케이스를 안정화하는 것입니다.

# 4. 테스트별 대응 정리

여기는 발표 뒤 질문 나오면 바로 말하기 좋게 정리한 버전이야.

## Alarm 계열

### `alarm-single`
- 한 스레드를 재웠다가 정확한 tick에 깨우는 기본 동작 확인
- 대응:
  - `timer_sleep()`에서 busy waiting 대신 `thread_sleep()`
  - `sleep_list`에 `wakeup_tick` 저장
  - `timer_interrupt()`에서 `thread_wake()` 호출

### `alarm-multiple`
- 여러 스레드가 각각 다른 tick에 자야 할 때 순서대로 깨어나는지 확인
- 대응:
  - `sleep_list`를 `wakeup_tick` 기준으로 정렬
  - `thread_wakeup_less()` comparator 사용

### `alarm-simultaneous`
- 같은 시각에 깨어나야 하는 여러 스레드를 처리하는지 확인
- 대응:
  - `thread_wake()`에서 현재 tick 이하인 thread들을 front부터 반복해서 깨움

### `alarm-zero`
- 0 tick sleep이면 바로 돌아오는지 확인
- 대응:
  - `timer_sleep()`에서 ticks가 0 이하이면 바로 return

### `alarm-negative`
- 음수 tick sleep도 실제로 block하지 않고 바로 return
- 대응:
  - `timer_sleep()`의 same guard 처리

### `alarm-priority`
- 동시에 깨우는 thread들이 priority 기준으로 실행되는지 확인
- 대응:
  - `thread_unblock()`이 READY queue에 priority 순 삽입
  - `thread_priority_more()`를 `effect_priority` 기준 comparator로 사용

## Priority Scheduling 계열

### `priority-preempt`
- 더 높은 priority thread가 READY 되면 현재 thread가 양보하는지 확인
- 대응:
  - `thread_create()` 후 새 thread가 더 높으면 `thread_yield()`

### `priority-change`
- 현재 thread의 priority를 낮췄을 때 더 높은 READY thread에게 양보하는지 확인
- 대응:
  - `thread_set_priority()` 후 ready_list front와 비교
  - 현재 thread보다 높으면 yield

### `priority-sema`
- semaphore waiters 중 가장 높은 priority thread를 먼저 깨우는지 확인
- 대응:
  - `sema_down()`에서 `sema->waiters`를 priority 순으로 삽입
  - `sema_up()`에서 front waiter를 깨움
  - 필요하면 yield

### `priority-condvar`
- condition variable waiters도 priority 순으로 signal되는지 확인
- 대응:
  - `semaphore_elem`에 waiter priority 저장
  - `cond->waiters`를 priority 순 정렬
  - `cond_signal()`에서 front waiter 깨움

## Donation 계열

### `priority-donate-one`
- 한 donor가 한 holder에게 donation하는 기본 케이스
- 대응:
  - `priority` / `effect_priority` 분리
  - `lock_acquire()`에서 donor 등록
  - `thread_get_priority()`가 `effect_priority` 반환

### `priority-donate-lower`
- donation 중 base priority를 낮춰도 effective priority가 유지되는지 확인
- 대응:
  - `thread_set_priority()`는 base priority 변경
  - 이후 `thread_refresh_priority()`로 donor 포함 재계산

### `priority-donate-multiple`
- 여러 donor 중 가장 높은 donation을 유지하는지 확인
- 대응:
  - `donation_list`, `donation_elem` 추가
  - `thread_refresh_priority()`가 donor 최고 priority 기준으로 계산

### `priority-donate-multiple2`
- lock release 순서가 달라도 donation이 정확히 남고 사라지는지 확인
- 대응:
  - `lock_release()`에서 donation 전체 삭제 금지
  - 현재 release하는 lock과 관련된 donor만 제거
  - 남은 donor와 base priority를 비교해 다시 계산

# 5. 발표 때 외우기 좋은 핵심 문장 5개

1. 기다리는 스레드는 CPU를 낭비하지 않도록 block시키고, 깨어날 때는 priority 기준으로 실행되게 했습니다.  
2. priority donation은 단순히 숫자를 바꾸는 게 아니라, lock 대기 관계에 따라 우선순위를 임시로 반영하는 구조입니다.  
3. `priority`는 원래 우선순위이고, `effect_priority`는 donation까지 반영된 실제 실행 우선순위입니다.  
4. `lock_release()`에서 donation을 한 번에 원복하면, 아직 남아 있어야 할 다른 donation까지 사라질 수 있습니다.  
5. 그래서 release한 lock과 관련된 donor만 제거하고, 남아 있는 donor 기준으로 `effect_priority`를 다시 계산했습니다.  
