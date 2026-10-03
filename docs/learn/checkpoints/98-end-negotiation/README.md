# 98 종료 협상과 판정의 경계

97-hash-observation에 EndNegotiation과 두 실행 타깃을 추가한다.
[HTML 강의](../../index.html#lesson-98)에서 의사·통신·규칙·저장 상태를 구분한다.

```sh
cmake -S docs/learn/checkpoints/98-end-negotiation -B out/study98 -DSTUDY_PLATFORM=SCRIPTED -DCMAKE_BUILD_TYPE=Release
cmake --build out/study98 -j3
ctest --test-dir out/study98 --output-on-failure
./out/study98/end_timeline
python3 scripts/check_learning_end_negotiation.py
```

end_timeline은 두 RoundPlay에 동일한 입력을 제공해 실제 terminal에 도달한다.
상대의 선택을 선보관한 뒤 각 소유자가 로컬 종료를 관측해 협상을 활성화한다.
양쪽 Restart를 확인하면 실험 드라이버가 동일한 새 설정을 공급한다.
새 설정의 네트워크 합의·보상 저장은 이 실행기의 책임이 아니다.
Windows 다중 구성에서는 --config Release와 CTest -C Release를 사용한다.
