# Phase 1 verification

## Fresh verification

- **Date:** 2026-07-18
- **Commit:** [`739dc58`](https://github.com/NearthYou/pintos_lab/commit/739dc58d29f158af6aec87b853aaa3f2eb9c4ea2)
- **Environment:** Ubuntu 22.04 image targeting `linux/amd64`, run through Docker Desktop’s Linux/amd64 engine.

Image preparation and both test runners used these commands from the orchestration workspace:

```powershell
docker build --platform linux/amd64 -t pintos-kaist-portfolio:ubuntu22.04 work/pintos-302-g1-full/.devcontainer
docker run --rm --platform linux/amd64 -v "${PWD}/work/pintos-lab-full:/workspace" -w /workspace/pintos pintos-kaist-portfolio:ubuntu22.04 bash -lc "source ./activate && make -C threads clean && make -C threads check"
docker run --rm --platform linux/amd64 -v "${PWD}/work/pintos-lab-full:/workspace" -w /workspace/pintos pintos-kaist-portfolio:ubuntu22.04 bash -lc "source ./activate && make -C userprog clean && make -C userprog check"
```

Concrete runner results:

| Suite | Exit | Exact summary | Failing tests |
| --- | ---: | --- | --- |
| Threads | 0 | `All 27 tests passed.` | none |
| User Programs | 0 | `All 95 tests passed.` | none |

After the build client timed out, `docker image inspect pintos-kaist-portfolio:ubuntu22.04` confirmed that the requested tag existed as `linux/amd64`, with image ID `sha256:3aba5c01b877f1e21988daf90835d8b0bf6db9437732b4c5d27d1d20acf04f18` and Ubuntu `22.04` OCI label. Both test containers then completed against that image.

## Historical evidence

Only PRs represented by merge commits in the verified `main` history are listed here:

- Threads integration: [PR #43](https://github.com/NearthYou/pintos_lab/pull/43) and [PR #46](https://github.com/NearthYou/pintos_lab/pull/46).
- Argument passing and syscall dispatch: [PR #49](https://github.com/NearthYou/pintos_lab/pull/49) and [PR #51](https://github.com/NearthYou/pintos_lab/pull/51).
- User-memory access, process lifecycle, cleanup, and safety follow-ups: [PR #162](https://github.com/NearthYou/pintos_lab/pull/162), [PR #163](https://github.com/NearthYou/pintos_lab/pull/163), [PR #164](https://github.com/NearthYou/pintos_lab/pull/164), [PR #165](https://github.com/NearthYou/pintos_lab/pull/165), [PR #166](https://github.com/NearthYou/pintos_lab/pull/166), [PR #167](https://github.com/NearthYou/pintos_lab/pull/167), [PR #168](https://github.com/NearthYou/pintos_lab/pull/168), [PR #169](https://github.com/NearthYou/pintos_lab/pull/169), [PR #170](https://github.com/NearthYou/pintos_lab/pull/170), and [PR #171](https://github.com/NearthYou/pintos_lab/pull/171).

## Limitations

- No test failed or was left unrun in the two fresh suites; the totals above cover the complete `threads check` and `userprog check` targets at the verified commit.
- Both builds emitted existing compiler warnings. The passing test totals therefore do not claim warning-free compilation.
- The Docker build client returned exit 124 after `904030` ms without buffered output, even though subsequent inspection confirmed the requested image tag and both test containers ran successfully.
- Branch-only work is not evidence of final `main` behavior. In particular, PR #42 is not treated as proof of the merged scheduling implementation.
