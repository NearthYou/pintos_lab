윈도우 PowerShell용으로 /C:/Users/fhrhd/바탕 화면/Jungle/Week09/pintos_lab/scripts/run-schd-tests.ps1 만들었습니다.

사용 예시는 repo 루트에서:

powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd1
powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd2
powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd12
powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd3
powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage mlfqs

컨테이너 자동 탐색이 안 되면:

docker ps --format "{{.Names}}"
powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd1 -ContainerName <컨테이너이름>

컨테이너 안의 Pintos 경로가 특이하면:

powershell -ExecutionPolicy Bypass -File .\scripts\run-schd-tests.ps1 -Stage schd1 -PintosRoot /workspace/pintos

기본 동작은 make clean 후 테스트 실행, .result 출력까지 합니다. clean 없이 돌리고 싶으면 -NoClean 붙이면 됩니다.

여기서는 Docker API 권한이 막혀 실제 테스트 실행까지는 못 했습니다. PowerShell 스크립트는 Docker 호출 지점까지 파싱되
는 것만 확인했습니다.