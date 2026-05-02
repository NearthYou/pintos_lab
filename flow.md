# Pintos Project 2 User Programs Flow

이 문서는 Project 2 User Programs의 전체 흐름을 테스트 요구사항과 연결해서 이해하기 위한 문서다. D 역할인 file syscall / fd table 담당자가 어디에 붙는지도 함께 정리한다.

구현 코드는 적지 않는다. 흐름과 책임만 설명한다.

## 1. 전체 큰 그림

Project 2는 커널 내부 함수만 테스트하던 Project 1과 다르게, 실제 유저 프로그램을 파일시스템에서 찾아 메모리에 올리고 user mode에서 실행한다.

전체 흐름은 다음과 같다.

```text
테스트 실행
-> Pintos 커널 부팅
-> run "유저프로그램 인자..."
-> 유저 프로그램 load
-> argument passing
-> user main(argc, argv) 실행
-> 유저 코드가 syscall 호출
-> syscall_handler가 커널 기능 수행
-> exit/wait로 종료 확인
```

이 흐름이 제대로 이어져야 `args-*`, `create-*`, `open-*`, `read-*`, `write-*`, `fork-*`, `exec-*`, `rox-*` 테스트가 의미 있게 돌아간다.

## 2. 테스트가 유저 프로그램을 실행시키는 흐름

테스트는 C 함수를 직접 부르는 방식이 아니다. Pintos 안에서 유저 프로그램을 실제로 실행한다.

```text
make check
-> tests/Make.tests
-> pintos ... -- -q -f run '프로그램 인자...'
-> threads/init.c의 run_task()
-> process_create_initd(task)
-> process_wait(...)
```

예를 들어 `args-multiple` 테스트는 대략 이런 명령으로 실행된다.

```text
run 'args-multiple some arguments for you!'
```

여기서 `task`는 `"args-multiple some arguments for you!"` 전체 문자열이다.

테스트 요구사항:

| 테스트 묶음 | 이 단계에서 요구하는 것 |
|---|---|
| `args-*` | 전체 명령 문자열이 유저 프로그램의 `argc/argv`로 잘 전달되어야 한다. |
| `exec-*` | 부모나 자식이 새 유저 프로그램을 실행할 수 있어야 한다. |
| `wait-*` | 부모가 자식의 종료 상태를 받을 수 있어야 한다. |

## 3. 유저 프로그램을 메모리에 올리는 흐름

유저 프로그램 실행은 `process.c`에서 이어진다.

```text
process_create_initd()
-> thread_create()
-> initd()
-> process_exec()
-> load()
-> setup_stack()
-> argument passing
-> do_iret()
-> user main(argc, argv)
```

주요 함수:

| 함수 | 역할 |
|---|---|
| `process_create_initd()` | 첫 유저 프로그램용 thread를 만든다. |
| `initd()` | 새 thread에서 `process_exec()`를 호출한다. |
| `process_exec()` | 현재 thread를 새 유저 프로그램으로 바꾼다. |
| `load()` | ELF 실행 파일을 열고 메모리에 올린다. |
| `setup_stack()` | 유저 stack 한 페이지를 준비한다. |
| `do_iret()` | kernel mode에서 user mode로 넘어간다. |

중요한 점:

`load()`는 실행 파일 이름으로 파일을 연다. 따라서 `"echo hello"` 전체를 load에 넘기면 안 되고, 실행 파일 이름인 `"echo"`만 넘겨야 한다.

테스트 요구사항:

| 테스트 묶음 | 필요한 흐름 |
|---|---|
| `args-none`, `args-single`, `args-multiple`, `args-many`, `args-dbl-space` | command line을 쪼개고 user stack에 `argc/argv`를 올려야 한다. |
| `exec-arg` | `exec("child-args childarg")`도 argument passing이 필요하다. |
| `exec-missing` | 없는 실행 파일이면 실패해야 한다. |
| `exec-boundary`, `exec-bad-ptr` | 실행 파일명 포인터 검증이 필요하다. |

## 4. Argument Passing 흐름

argument passing은 전체 명령 문자열을 유저 프로그램의 `main(argc, argv)`가 이해할 수 있게 만드는 작업이다.

예시:

```text
"args-multiple some arguments for you!"
-> 실행 파일 이름: "args-multiple"
-> argc = 5
-> argv[0] = "args-multiple"
-> argv[1] = "some"
-> argv[2] = "arguments"
-> argv[3] = "for"
-> argv[4] = "you!"
-> argv[5] = NULL
```

그림으로 보면 다음과 같다.

```text
전체 명령 문자열
-> 실행 파일 이름
-> argv 배열
-> user stack
-> main(argc, argv)
```

`args-*` 테스트는 이 값들이 정확한지 출력해서 확인한다.

## 5. Syscall 전체 흐름

유저 프로그램은 커널 함수를 직접 부를 수 없다. 그래서 syscall로 커널에게 부탁한다.

예시:

```text
write(1, buffer, size)
-> lib/user/syscall.c wrapper
-> CPU 레지스터에 syscall 번호와 인자 저장
-> syscall instruction
-> syscall-entry.S
-> syscall_handler(struct intr_frame *f)
```

레지스터 의미:

| 위치 | 의미 |
|---|---|
| `rax` | syscall 번호. 반환값도 다시 여기에 저장된다. |
| `rdi` | 1번째 인자 |
| `rsi` | 2번째 인자 |
| `rdx` | 3번째 인자 |
| `r10` | 4번째 인자 |
| `r8` | 5번째 인자 |
| `r9` | 6번째 인자 |

`syscall_handler()`가 해야 하는 일:

```text
syscall 번호 확인
-> 인자 꺼내기
-> user pointer 검증
-> 실제 커널 함수 호출
-> 반환값 저장
-> 유저 프로그램으로 복귀
```

테스트 요구사항:

| 테스트 묶음 | syscall 쪽 요구 |
|---|---|
| `halt`, `exit` | 기본 syscall dispatch와 종료 처리 |
| `create/open/read/write/close-*` | syscall 번호별 분기 |
| `*-bad-ptr` | 유저 포인터 검증 |
| `*-bad-fd` | 잘못된 fd 처리 |
| 대부분의 테스트 | `write(1)`이 되어야 테스트 로그가 보인다. |

## 6. printf와 write(1) 흐름

테스트의 `msg()`와 유저 프로그램의 `printf()`는 결국 `write(1, buffer, size)`로 내려온다.

```text
printf("hello")
-> lib/user/console.c
-> write(STDOUT_FILENO, buffer, size)
-> SYS_WRITE
-> syscall_handler()
-> fd == 1
-> console 출력
```

`fd 1`은 stdout이다. 파일 이름을 모르는 것이 아니라, `1`이라는 번호 자체가 "화면 출력"이라는 약속이다.

```text
fd 0 -> stdin
fd 1 -> stdout
fd 2 이상 -> open()으로 연 일반 파일
```

그래서 `write(1, buffer, size)`는 fd table 없이도 먼저 구현할 수 있는 특수 케이스다.

반대로 `write(3, buffer, size)`처럼 fd가 일반 파일이면 fd table이 필요하다.

```text
write(1, buffer, size)
-> stdout 특수 처리
-> console 출력

write(3, buffer, size)
-> fd table에서 3번 검색
-> struct file * 찾기
-> file_write()
```

결론:

`write` 전체가 fd table을 반드시 먼저 요구하는 것은 아니다. `write(1)`은 stdout 특수 처리로 먼저 만들 수 있고, `write(fd >= 2)`는 fd table이 있어야 한다.

## 7. D 역할의 file syscall 흐름

D는 syscall handler 안에서 파일 관련 syscall을 실제 파일시스템 함수와 fd table로 연결한다.

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

`open()` 흐름:

```text
open("sample.txt")
-> SYS_OPEN
-> syscall_handler()
-> filesys_open("sample.txt")
-> struct file *
-> fd table에 등록
-> fd 번호 반환
```

`read()` 흐름:

```text
read(fd, buffer, size)
-> fd == 0이면 stdin 입력
-> fd >= 2이면 fd table에서 파일 검색
-> file_read()
-> 읽은 byte 수 반환
```

`write()` 흐름:

```text
write(fd, buffer, size)
-> fd == 1이면 stdout 출력
-> fd >= 2이면 fd table에서 파일 검색
-> file_write()
-> 쓴 byte 수 반환
```

## 8. fd table이 필요한 이유

fd table은 프로세스마다 가지고 있는 "파일 번호표"다.

```text
프로세스 A fd table
0 -> stdin
1 -> stdout
2 -> sample.txt
3 -> test.txt

프로세스 B fd table
0 -> stdin
1 -> stdout
2 -> other.txt
```

중요한 점:

fd 번호는 프로세스마다 따로 의미를 가진다. A의 fd 2와 B의 fd 2는 같은 파일일 수도 있고 아닐 수도 있다.

`open()`은 파일 이름으로 파일을 찾고 fd table에 등록한다. 그 뒤 `read/write/close/filesize/seek/tell`은 파일 이름을 받지 않고 fd 번호만 받는다.

```text
open("sample.txt")
-> fd 2 반환

read(2, buffer, size)
-> fd table에서 2번 검색
-> sample.txt의 struct file * 찾기
```

## 9. D 테스트 요구사항과 흐름 비교

| 테스트 묶음 | 요구하는 흐름 |
|---|---|
| `create-*` | 파일 이름 pointer 검증 후 `filesys_create()` 연결 |
| `open-*` | 파일 이름으로 `filesys_open()`, 성공 시 fd `> 1` 반환 |
| `close-*` | fd table에서 fd 제거, `file_close()` 호출 |
| `read-*` | fd 0 특수 처리, fd >= 2 파일 read, bad pointer/bad fd 처리 |
| `write-*` | fd 1 특수 처리, fd >= 2 파일 write, bad pointer/bad fd 처리 |
| `filesys/base/sm-*`, `lg-*` | `filesize`, `seek`, 파일 위치, read/write 정확성 |
| `filesys/base/syn-*` | 여러 프로세스 동시 접근에서 파일 내용이 깨지지 않아야 함 |
| `fork-read`, `fork-close` | fork 시 fd table 복제와 close 독립성 |
| `exec-read`, `multi-child-fd` | exec 후에도 fd table 유지 |
| `rox-*` | 실행 중인 파일에는 write가 막혀야 함 |

## 10. write(1)을 먼저 해야 하는 이유

대부분의 테스트는 `msg()`로 진행 상황을 출력한다.

```text
msg("begin")
-> write(1, buffer, size)
```

즉 `write(1)`이 안 되면 테스트가 실패했을 때 무엇이 어디까지 진행됐는지 보기 어렵다.

그래서 추천 순서는 다음과 같다.

```text
1. exit 최소 흐름
2. write(1) stdout
3. create/remove
4. open/close fd table
5. filesize/read/write 파일 처리
6. seek/tell
7. process_exit fd cleanup
8. fork/exec fd table 연동
9. rox
```

## 11. write와 fd table 관계

질문:

```text
write는 fd table이 정의되어야 할 수 있는 것 아닌가?
```

답:

```text
write(1)은 fd table 없이 가능하다.
write(fd >= 2)는 fd table이 필요하다.
```

이유:

`fd 1`은 파일이 아니라 stdout이라는 약속된 특수 번호다. 따라서 fd table에서 파일을 찾을 필요 없이 console 출력으로 바로 보내면 된다.

하지만 `fd >= 2`는 `open()`이 만들어 준 일반 파일 번호다. 이 경우에는 fd table에서 해당 fd가 어떤 `struct file *`인지 찾아야 한다.

현재 skeleton 기준으로 fd table은 이미 완성되어 있지 않다. D 역할에서 팀과 합의해서 `struct thread` 등에 추가하고 관리해야 하는 부분이다.

## 12. D가 회의에서 말할 수 있는 핵심

```text
write(1)은 stdout 특수 처리라 fd table 전에 먼저 구현할 수 있습니다.
그래서 테스트 로그를 보기 위해 초반에 필요합니다.
반면 open()이 반환하는 fd 2 이상은 fd table이 있어야 read/write/close/filesize/seek/tell에서 사용할 수 있습니다.
D는 fd table을 만들고 file syscall을 기존 filesys/file 함수로 연결하는 역할이고,
pointer 검증은 A, exit cleanup은 B, fork/exec fd 복제와 유지는 C/B와 맞춰야 합니다.
```

