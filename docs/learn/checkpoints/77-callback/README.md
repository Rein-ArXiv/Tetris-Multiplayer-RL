# 77 SDL 콜백과 잠금 범위

76-playback에서 누적. Player::replace/unload에서 연결 해제와 소유권 이동은 잠금 안,
retired PCM 파괴는 잠금 밖에서 수행한다. 콜백과 게임 main, Voice의 출력 계약은 유지한다.

```sh
cmake -S docs/learn/checkpoints/77-callback -B out/study77 -DSTUDY_PLATFORM=SDL -DCMAKE_BUILD_TYPE=Release
cmake --build out/study77 -j3
ctest --test-dir out/study77 --output-on-failure
SDL_AUDIODRIVER=dummy ./out/study77/callback_contract
```

예상 `callback_contract: all checks passed`. 진단은 실제 SDL 장치를 paused로 유지하고
등록된 실제 Player callback을 직접 구동한다. 출력·일반 C++ new/delete를 관찰하며
SDL/C malloc, 실제 스케줄링과 스피커 지연을 측정하지 않는다.
playback_contract는 별도로 실제 SDL dummy 콜백을 실행한다.

callback_contract는 player.cpp를 대역과 함께 포함하므로 study_playback을 중복 링크하지 않는다.
study_pcm은 INTERFACE 타깃이다. SCRIPTED 빌드는 SDL 진단 타깃을 제외한다.
