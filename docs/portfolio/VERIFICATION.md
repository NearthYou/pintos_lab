# Phase 1 검증

## Fresh verification

- **검증일:** 2026-07-18
- **검증 커밋:** [`739dc58`](https://github.com/NearthYou/pintos_lab/commit/739dc58d29f158af6aec87b853aaa3f2eb9c4ea2)
- **환경:** Docker Desktop Linux/amd64 engine에서 실행한 Ubuntu `22.04`, `linux/amd64` 이미지

이미지 준비와 두 테스트 스위트에는 다음 명령을 사용했습니다.

```powershell
docker build --platform linux/amd64 -t pintos-kaist-portfolio:ubuntu22.04 work/pintos-302-g1-full/.devcontainer
docker run --rm --platform linux/amd64 -v "${PWD}/work/pintos-lab-full:/workspace" -w /workspace/pintos pintos-kaist-portfolio:ubuntu22.04 bash -lc "source ./activate && make -C threads clean && make -C threads check"
docker run --rm --platform linux/amd64 -v "${PWD}/work/pintos-lab-full:/workspace" -w /workspace/pintos pintos-kaist-portfolio:ubuntu22.04 bash -lc "source ./activate && make -C userprog clean && make -C userprog check"
```

실제 실행 결과는 다음과 같습니다.

| 스위트 | 종료 코드 | 실제 요약 | 실패 테스트 |
| --- | ---: | --- | --- |
| Threads | 0 | `All 27 tests passed.` | 없음 |
| User Programs | 0 | `All 95 tests passed.` | 없음 |

Docker build client는 `904030 ms` 뒤 `exit 124`로 timeout됐습니다. 이후 `docker image inspect pintos-kaist-portfolio:ubuntu22.04`로 요청한 tag가 `linux/amd64`, Ubuntu `22.04` OCI label, image ID `sha256:3aba5c01b877f1e21988daf90835d8b0bf6db9437732b4c5d27d1d20acf04f18`로 존재함을 확인했습니다. 두 테스트 컨테이너는 이 이미지에서 끝까지 실행되어 각각 종료 코드 0을 반환했습니다.

## Historical evidence

검증한 `main` 이력에 병합 커밋으로 포함된 PR만 기록합니다.

- Threads 통합: [PR #43](https://github.com/NearthYou/pintos_lab/pull/43), [PR #46](https://github.com/NearthYou/pintos_lab/pull/46)
- argument passing과 syscall dispatch: [PR #49](https://github.com/NearthYou/pintos_lab/pull/49), [PR #51](https://github.com/NearthYou/pintos_lab/pull/51)
- user-memory 접근, process lifecycle, cleanup과 안전성 보완: [PR #162](https://github.com/NearthYou/pintos_lab/pull/162), [PR #163](https://github.com/NearthYou/pintos_lab/pull/163), [PR #164](https://github.com/NearthYou/pintos_lab/pull/164), [PR #165](https://github.com/NearthYou/pintos_lab/pull/165), [PR #166](https://github.com/NearthYou/pintos_lab/pull/166), [PR #167](https://github.com/NearthYou/pintos_lab/pull/167), [PR #168](https://github.com/NearthYou/pintos_lab/pull/168), [PR #169](https://github.com/NearthYou/pintos_lab/pull/169), [PR #170](https://github.com/NearthYou/pintos_lab/pull/170), [PR #171](https://github.com/NearthYou/pintos_lab/pull/171)

## Limitations

- 두 fresh suite에서 실패하거나 실행되지 않은 테스트는 없습니다. 위 결과는 해당 커밋의 전체 `threads check`와 `userprog check` 대상입니다.
- 두 빌드는 기존 컴파일 경고를 출력했습니다. 테스트 통과가 warning-free compilation을 의미하지는 않습니다.
- Docker build client는 `exit 124`로 timeout됐습니다. 이미지 tag 확인과 테스트 실행은 성공했지만 build client가 정상 종료됐다고 주장하지 않습니다.
- 브랜치 전용 작업은 최종 `main` 동작의 근거가 아닙니다. 특히 PR #42는 병합된 스케줄링 구현의 증거로 사용하지 않습니다.
