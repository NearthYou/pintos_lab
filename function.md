# Pintos Project 2 - D Role Function Map

이 문서는 Project 2 User Programs에서 D 역할인 file syscall / fd table 담당자가 알아야 하는 기존 함수와 흐름을 정리한 문서다. 구현 코드는 적지 않고, 어떤 함수가 어떤 역할을 하며 어떤 테스트 요구사항과 연결되는지만 설명한다.

## 1. D 역할의 큰 흐름

D 역할은 유저 프로그램이 파일 관련 syscall을 호출했을 때, 커널 안에서 실제 파일시스템 함수로 연결하고 fd table을 관리하는 일이다.

전체 흐름은 다음과 같다.

```text
user program
-> lib/user/syscall.c wrapper
-> syscall instruction
-> userprog/syscall-entry.S
-> userprog/syscall.c syscall_handler()
-> D의 file syscall 처리
-> filesys/filesys.c 또는 filesys/file.c 함수 호출
-> syscall 반환값을 f->R.rax에 저장
-> user program으로 복귀
```

예를 들면 `open("sample.txt")`는 다음 흐름이다.

```text
open("sample.txt")
-> SYS_OPEN
-> syscall_handler()
-> filesys_open("sample.txt")
-> struct file *
-> fd table에 저장
-> fd 번호 반환
```

`read(fd, buffer, size)`는 다음 흐름이다.

```text
read(fd, buffer, size)
-> SYS_READ
-> syscall_handler()
-> fd table에서 fd 검색
-> struct file * 획득
-> file_read(file, buffer, size)
-> 읽은 byte 수 반환
```

## 2. Syscall 입구에서 알아야 하는 함수와 구조체

| 파일 | 함수/구조체 | D가 알아야 하는 이유 |
|---|---|---|
| `pintos/userprog/syscall.c` | `syscall_init()` | CPU의 syscall 진입점을 등록한다. D가 직접 다루기보다는 syscall 흐름의 시작 설정이다. |
| `pintos/userprog/syscall.c` | `syscall_handler(struct intr_frame *f)` | 모든 syscall이 최종적으로 들어오는 핵심 함수다. D의 `create/open/read/write/close` 처리가 여기에 붙는다. |
| `pintos/userprog/syscall-entry.S` | `syscall_entry` | 유저 모드에서 커널 모드로 넘어올 때 레지스터들을 `intr_frame` 형태로 저장한다. |
| `pintos/include/threads/interrupt.h` | `struct intr_frame` | syscall 번호와 인자, 반환값이 들어 있는 구조체다. |
| `pintos/include/lib/syscall-nr.h` | `SYS_CREATE`, `SYS_OPEN` 등 | syscall 번호표다. `f->R.rax` 값으로 어떤 syscall인지 구분한다. |

`intr_frame`에서 D가 자주 보게 되는 레지스터 의미는 다음과 같다.

| 레지스터 | 의미 |
|---|---|
| `f->R.rax` | syscall 번호. 반환값도 여기에 넣는다. |
| `f->R.rdi` | 1번째 인자 |
| `f->R.rsi` | 2번째 인자 |
| `f->R.rdx` | 3번째 인자 |
| `f->R.r10` | 4번째 인자 |
| `f->R.r8` | 5번째 인자 |
| `f->R.r9` | 6번째 인자 |

예시 의미:

```text
write(fd, buffer, size)
-> rax = SYS_WRITE
-> rdi = fd
-> rsi = buffer
-> rdx = size
```

## 3. 유저 쪽 wrapper 함수

이 함수들은 커널에서 직접 호출하는 함수가 아니라, 유저 프로그램이 syscall을 부를 때 지나가는 함수다. 테스트가 어떤 syscall을 요청하는지 이해할 때 중요하다.

| 파일 | 함수 | 의미 |
|---|---|---|
| `pintos/lib/user/syscall.c` | `create(const char *file, unsigned initial_size)` | `SYS_CREATE` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `remove(const char *file)` | `SYS_REMOVE` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `open(const char *file)` | `SYS_OPEN` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `filesize(int fd)` | `SYS_FILESIZE` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `read(int fd, void *buffer, unsigned size)` | `SYS_READ` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `write(int fd, const void *buffer, unsigned size)` | `SYS_WRITE` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `seek(int fd, unsigned position)` | `SYS_SEEK` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `tell(int fd)` | `SYS_TELL` 호출 wrapper |
| `pintos/lib/user/syscall.c` | `close(int fd)` | `SYS_CLOSE` 호출 wrapper |

테스트의 `msg()`와 유저 프로그램의 `printf()`도 결국 `write(1, buffer, size)`로 내려온다. 그래서 `SYS_WRITE`의 `fd == 1` 처리는 D가 초반에 꼭 이해해야 한다.

## 4. 표준 fd 번호

| 파일 | 정의 | 의미 |
|---|---|---|
| `pintos/include/lib/stdio.h` | `STDIN_FILENO 0` | fd 0은 표준 입력이다. 보통 키보드 입력에 연결된다. |
| `pintos/include/lib/stdio.h` | `STDOUT_FILENO 1` | fd 1은 표준 출력이다. 보통 화면 출력에 연결된다. |

Project 2 기본 테스트 기준으로 일반 파일 fd는 `open()` 성공 시 `2` 이상이어야 한다. `open-normal`, `open-twice`, `read-normal`, `write-normal` 같은 테스트들이 `fd > 1`을 기대한다.

## 5. 파일 이름 기반 filesys 함수

이 함수들은 fd table을 쓰기 전 단계에서 사용한다. 파일 이름 문자열을 받아 파일시스템에 직접 요청한다.

| 파일 | 함수 | 반환 | D에서 쓰는 syscall | 설명 |
|---|---|---|---|---|
| `pintos/filesys/filesys.c` | `filesys_create(const char *name, off_t initial_size)` | `bool` | `create` | 이름이 `name`인 파일을 `initial_size` 크기로 만든다. 성공하면 `true`, 실패하면 `false`. |
| `pintos/filesys/filesys.c` | `filesys_open(const char *name)` | `struct file *` 또는 `NULL` | `open`, `load` | 이름으로 파일을 열고 열린 파일 객체를 돌려준다. 실패하면 `NULL`. |
| `pintos/filesys/filesys.c` | `filesys_remove(const char *name)` | `bool` | `remove` | 이름에 해당하는 파일을 삭제한다. 이미 열린 fd가 있으면 그 fd로는 계속 읽고 쓸 수 있어야 한다. |

테스트 연결:

| 테스트 | 요구 |
|---|---|
| `create-normal` | `filesys_create()` 성공 |
| `create-exists` | 같은 이름 재생성 실패 |
| `open-normal` | `filesys_open()` 성공 후 fd 반환 |
| `open-missing` | 없는 파일은 `NULL`, syscall 반환은 `-1` |
| `syn-remove` | `remove()` 후에도 기존 열린 fd는 사용 가능 |

## 6. 열린 파일 객체용 file 함수

`open()`이 성공하면 `struct file *`가 생긴다. D의 fd table은 fd 번호와 이 `struct file *`를 연결하는 표라고 보면 된다.

| 파일 | 함수 | 반환 | D에서 쓰는 syscall | 설명 |
|---|---|---|---|---|
| `pintos/filesys/file.c` | `file_read(struct file *file, void *buffer, off_t size)` | 읽은 byte 수 | `read(fd >= 2)` | 파일의 현재 위치에서 `size`만큼 읽고, 읽은 만큼 파일 위치가 증가한다. |
| `pintos/filesys/file.c` | `file_write(struct file *file, const void *buffer, off_t size)` | 쓴 byte 수 | `write(fd >= 2)` | 파일의 현재 위치에 `size`만큼 쓰고, 쓴 만큼 파일 위치가 증가한다. |
| `pintos/filesys/file.c` | `file_length(struct file *file)` | 파일 크기 | `filesize` | 파일의 byte 크기를 반환한다. |
| `pintos/filesys/file.c` | `file_seek(struct file *file, off_t new_pos)` | 없음 | `seek` | 파일의 현재 위치를 `new_pos`로 이동한다. |
| `pintos/filesys/file.c` | `file_tell(struct file *file)` | 현재 위치 | `tell` | 파일의 현재 위치를 반환한다. |
| `pintos/filesys/file.c` | `file_close(struct file *file)` | 없음 | `close`, `process_exit` cleanup | 열린 파일 객체를 닫고 자원을 정리한다. |
| `pintos/filesys/file.c` | `file_duplicate(struct file *file)` | 새 `struct file *` | `fork` fd 복제 | 같은 inode를 가리키는 새 file 객체를 만든다. fork 시 fd table 복제에서 중요하다. |
| `pintos/filesys/file.c` | `file_deny_write(struct file *file)` | 없음 | `rox` | 실행 중인 파일에 write를 막을 때 필요하다. |
| `pintos/filesys/file.c` | `file_allow_write(struct file *file)` | 없음 | `process_exit` | deny write를 해제할 때 필요하다. `file_close()` 안에서도 allow가 호출된다. |

중요한 감각:

```text
filesys_open("a.txt")
-> struct file *
-> fd table에 2번으로 저장

read(2, buffer, size)
-> fd table에서 2번 검색
-> struct file * 획득
-> file_read()
```

## 7. console/stdin 함수

fd 0과 fd 1은 일반 파일이 아니라 특수 번호다.

| 파일 | 함수 | D에서 쓰는 경우 | 설명 |
|---|---|---|---|
| `pintos/lib/kernel/console.c` | `putbuf(const char *buffer, size_t n)` | `write(fd == 1)` | 유저 buffer 내용을 화면에 출력할 때 사용한다. |
| `pintos/devices/input.c` | `input_getc(void)` | `read(fd == 0)` | 키보드/입력 버퍼에서 한 글자를 읽을 때 사용한다. |

테스트 연결:

| 테스트 | 요구 |
|---|---|
| 대부분의 userprog 테스트 | `msg()`가 `write(1)`을 쓰므로 stdout 출력이 필요하다. |
| `write-stdin` | `write(0, ...)`은 실패하거나 `exit(-1)` 처리 가능하다. |
| `read-stdout` | `read(1, ...)`은 실패하거나 `exit(-1)` 처리 가능하다. |

## 8. 현재 프로세스와 fd table 관련 함수

D의 fd table은 보통 현재 실행 중인 thread/process에 붙는다. Pintos에서는 user process도 `struct thread`로 표현된다.

| 파일 | 함수/구조체 | D에서 중요한 이유 |
|---|---|---|
| `pintos/include/threads/thread.h` | `struct thread` | 프로세스별 fd table을 둘 가능성이 큰 구조체다. 현재는 fd table 필드가 없다. |
| `pintos/threads/thread.c` | `thread_current()` | 현재 syscall을 호출한 프로세스의 `struct thread *`를 얻는다. fd table 접근의 시작점이다. |
| `pintos/threads/thread.c` | `thread_exit()` | 프로세스 종료 흐름으로 이어진다. 내부에서 `process_exit()`가 호출된다. |
| `pintos/userprog/process.c` | `process_exit()` | 열린 fd를 모두 닫는 cleanup이 연결되어야 하는 위치다. |
| `pintos/userprog/process.c` | `process_fork()` / `__do_fork()` | fork 시 부모 fd table을 자식에게 어떻게 복제할지 연결되는 위치다. |
| `pintos/userprog/process.c` | `process_exec()` | exec 후 fd table을 유지할지 정해야 하는 흐름이다. |

테스트 연결:

| 테스트 | 요구 |
|---|---|
| `fork-close` | 자식이 fd를 close해도 부모 fd는 살아 있어야 한다. |
| `fork-read` | fork 후 부모/자식의 fd table과 file position 정책이 맞아야 한다. |
| `exec-read` | fork 후 exec된 child가 넘겨받은 fd를 사용할 수 있어야 한다. |
| `multi-child-fd` | child가 인자로 받은 fd 번호를 사용하고 close할 수 있어야 한다. |

## 9. user pointer 검증 관련 함수와 매크로

syscall 인자는 유저 프로그램이 넘기는 값이다. 커널이 그대로 믿으면 안 된다. 파일 이름 문자열, read buffer, write buffer 모두 검증 대상이다.

| 파일 | 함수/매크로 | 의미 |
|---|---|---|
| `pintos/include/threads/vaddr.h` | `is_user_vaddr(vaddr)` | 주소가 유저 영역인지 확인한다. |
| `pintos/include/threads/vaddr.h` | `is_kernel_vaddr(vaddr)` | 주소가 커널 영역인지 확인한다. |
| `pintos/threads/mmu.c` | `pml4_get_page(pml4, uaddr)` | 유저 가상주소가 실제 물리 페이지에 매핑되어 있는지 확인할 때 쓰인다. |
| `pintos/threads/thread.c` | `thread_current()` | 현재 프로세스의 `pml4`를 가져오기 위해 필요하다. |

주의:

`pml4_get_page()`는 내부에서 `is_user_vaddr()`를 ASSERT한다. 따라서 커널 주소를 그대로 넣기 전에 유저 주소인지 먼저 확인하는 감각이 필요하다.

테스트 연결:

| 테스트 | 요구 |
|---|---|
| `create-null`, `open-null` | `NULL` 파일명 처리 |
| `create-bad-ptr`, `open-bad-ptr` | 잘못된 파일명 포인터 처리 |
| `read-bad-ptr` | bad buffer pointer는 프로세스를 `exit(-1)`로 끝내야 한다. |
| `write-bad-ptr` | bad buffer pointer는 프로세스를 `exit(-1)`로 끝내야 한다. |
| `create-bound`, `open-boundary`, `read-boundary`, `write-boundary` | 페이지 경계에 걸친 정상 포인터는 통과해야 한다. |

## 10. 동기화 lock 함수

파일시스템은 여러 프로세스가 동시에 접근할 수 있다. `syn-read`, `syn-write` 같은 테스트는 동시 접근에서 파일 내용이 깨지지 않는지 본다.

| 파일 | 함수/구조체 | 의미 |
|---|---|---|
| `pintos/include/threads/synch.h` | `struct lock` | 임계구역 보호용 lock 구조체 |
| `pintos/threads/synch.c` | `lock_init()` | lock 초기화 |
| `pintos/threads/synch.c` | `lock_acquire()` | 파일시스템 접근 전 lock 획득 |
| `pintos/threads/synch.c` | `lock_release()` | 파일시스템 접근 후 lock 해제 |

D가 조심할 점:

파일시스템 lock을 어디서 잡을지는 팀 합의가 필요하다. 예를 들어 syscall handler의 파일 작업 주변에서 잡을 수도 있고, D가 만든 file syscall helper 내부에서 잡을 수도 있다. 중요한 건 모든 filesys/file 접근이 같은 기준으로 보호되어야 한다는 점이다.

## 11. fd table 구현에 도움이 되는 자료구조 함수

fd table은 배열이나 list로 만들 수 있다. 어떤 구조를 쓸지는 팀 스타일에 맞추면 된다.

| 파일 | 함수/구조체 | 쓰는 경우 |
|---|---|---|
| `pintos/include/threads/malloc.h` | `malloc()`, `calloc()`, `free()` | fd entry를 동적 할당할 때 |
| `pintos/include/lib/kernel/list.h` | `struct list`, `struct list_elem` | fd table을 list로 관리할 때 |
| `pintos/include/lib/kernel/list.h` | `list_init()` | list fd table 초기화 |
| `pintos/include/lib/kernel/list.h` | `list_push_back()` | fd entry 추가 |
| `pintos/include/lib/kernel/list.h` | `list_remove()` | fd entry 제거 |
| `pintos/include/lib/kernel/list.h` | `list_entry()` | `list_elem`에서 fd entry 구조체 얻기 |

배열 방식이면 list 함수가 필요 없고, list 방식이면 위 함수들을 자주 쓰게 된다.

## 12. D가 직접 설계할 가능성이 큰 helper

아래 함수들은 기존 함수가 아니라 D가 구현할 때 생길 수 있는 helper 역할이다. 이름은 팀 스타일에 맞춰 바꿔도 된다.

| helper 역할 | 설명 |
|---|---|
| fd table 초기화 | 새 프로세스가 시작할 때 fd table과 next fd를 준비한다. |
| fd 할당 | `open()`이 성공한 `struct file *`에 새 fd 번호를 붙인다. |
| fd 검색 | `read/write/filesize/seek/tell/close`에서 fd 번호로 `struct file *`를 찾는다. |
| fd 닫기 | 특정 fd를 fd table에서 제거하고 `file_close()`를 호출한다. |
| fd 전체 cleanup | `process_exit()`에서 모든 열린 파일을 닫는다. |
| fd table 복제 | `fork`에서 부모의 fd table을 자식에게 복제한다. |

중요:

`dup2` extra는 하지 않기로 했으므로 stdout redirect나 fd 0/1을 일반 fd table에 완전히 포함하는 고급 설계는 당장 필수는 아니다. 하지만 `fork/exec` 기본 테스트 때문에 fd table의 생명주기는 여전히 중요하다.

## 13. syscall별 기존 함수 연결표

| syscall | 유저 wrapper | 커널에서 주로 연결할 기존 함수 | fd table 사용 | 주요 테스트 |
|---|---|---|---|---|
| `create` | `create()` | `filesys_create()` | 안 씀 | `create-*` |
| `remove` | `remove()` | `filesys_remove()` | 안 씀 | `syn-remove`, filesys/base |
| `open` | `open()` | `filesys_open()` | 새 fd 등록 | `open-*` |
| `filesize` | `filesize()` | `file_length()` | fd 검색 | `read-normal`, `check_file_handle`, filesys/base |
| `read` | `read()` | `input_getc()` 또는 `file_read()` | fd 0 특수, fd >= 2 검색 | `read-*`, filesys/base |
| `write` | `write()` | `putbuf()` 또는 `file_write()` | fd 1 특수, fd >= 2 검색 | `write-*`, 모든 로그 |
| `seek` | `seek()` | `file_seek()` | fd 검색 | filesys/base random/seq |
| `tell` | `tell()` | `file_tell()` | fd 검색 | 기본에서는 약함, extra에서 강함 |
| `close` | `close()` | `file_close()` | fd 제거 | `close-*`, process exit cleanup |

## 14. 테스트 묶음과 필요한 함수

| 테스트 묶음 | 필요한 기존 함수 |
|---|---|
| `create-*` | `filesys_create()`, pointer 검증 함수/정책 |
| `open-*` | `filesys_open()`, fd table helper, pointer 검증 |
| `close-*` | fd 검색/제거 helper, `file_close()` |
| `read-*` | `input_getc()`, `file_read()`, fd 검색, buffer 검증 |
| `write-*` | `putbuf()`, `file_write()`, fd 검색, buffer 검증 |
| `filesys/base/sm-*`, `lg-*` | `filesys_create()`, `filesys_open()`, `file_read()`, `file_write()`, `file_length()`, `file_seek()`, `file_close()` |
| `filesys/base/syn-*` | 위 파일 함수들 + `lock_acquire()`, `lock_release()` |
| `fork-read`, `fork-close` | `file_duplicate()`, fd table 복제, `file_close()` |
| `exec-read`, `multi-child-fd` | exec 후 fd table 유지, fd 검색, `file_read()` |
| `rox-*` | `file_deny_write()`, `file_allow_write()`, `file_close()` |

## 15. D 담당자가 특히 헷갈리면 안 되는 구분

파일 이름을 받는 syscall:

```text
create(file_name, size)
remove(file_name)
open(file_name)
```

fd 번호를 받는 syscall:

```text
filesize(fd)
read(fd, buffer, size)
write(fd, buffer, size)
seek(fd, position)
tell(fd)
close(fd)
```

차이:

```text
open("sample.txt")
-> 파일 이름으로 파일을 찾는다.
-> 성공하면 fd 번호를 만든다.

read(2, buffer, size)
-> 파일 이름을 모른다.
-> fd table에서 2번이 어떤 파일인지 찾는다.
```

## 16. 구현 순서 기준으로 보는 함수 사용 흐름

1. `write(1)`:
   `SYS_WRITE` -> `fd == 1` 확인 -> `putbuf()`

2. `create/remove`:
   pointer 검증 -> `filesys_create()` 또는 `filesys_remove()`

3. `open/close`:
   `filesys_open()` -> fd table 등록 -> fd 반환
   fd 검색 -> `file_close()` -> fd table 제거

4. `filesize/read/write`:
   fd 검색 -> `file_length()` / `file_read()` / `file_write()`

5. `seek/tell`:
   fd 검색 -> `file_seek()` / `file_tell()`

6. `process_exit` cleanup:
   현재 thread fd table 순회 -> 모든 `file_close()`

7. `fork` fd 복제:
   부모 fd table 순회 -> `file_duplicate()` -> 자식 fd table에 같은 fd 번호로 등록

8. `rox`:
   실행 파일 open 후 `file_deny_write()`
   종료 시 `file_close()` 또는 `file_allow_write()` 흐름으로 write 허용 복구

## 17. 회의에서 확인해야 할 함수/정책 질문

| 질문 | 이유 |
|---|---|
| fd table은 `struct thread` 안에 둘까요? | fd table은 프로세스마다 달라야 하기 때문이다. |
| fd table은 배열로 할까요, list로 할까요? | fd 검색/삭제 방식이 달라진다. |
| 일반 파일 fd는 2부터 시작할까요? | 테스트들이 `fd > 1`을 기대한다. |
| fd 0/1은 fd table에 넣을까요, syscall에서 특수 처리할까요? | `read(0)`, `write(1)` 정책과 연결된다. |
| bad fd는 실패 반환으로 둘까요, `exit(-1)`로 죽일까요? | `.ck` 파일 일부는 둘 다 허용하지만 팀 정책 통일이 필요하다. |
| pointer 검증 helper는 A가 만들까요? | D syscall은 filename/buffer 검증에 의존한다. |
| file system lock은 어디서 잡을까요? | `syn-read`, `syn-write` 동시성 테스트와 연결된다. |
| `process_exit()`에서 fd cleanup은 누가 호출할까요? | D cleanup helper와 B exit 흐름이 만난다. |
| fork 때 fd table 복제는 `file_duplicate()`로 할까요? | `fork-close`, `fork-read`와 연결된다. |
| exec 후 fd table은 유지할까요? | `exec-read`, `multi-child-fd`와 연결된다. |

