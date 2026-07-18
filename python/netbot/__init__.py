"""Python-side bot utilities: protocol/입력 동등성 레이어 + ONNX export.

과거에는 TCP 로 C++ 게임에 접속하는 로컬 봇 클라이언트(client/session/
policy_runner)도 여기에 있었지만, 봇을 네트워크 플레이어로 띄우는 방향은
폐기되어 제거됐다. 남은 모듈:

- ``framing``        : C++ net/framing.h 와 동일한 프레임 인코딩/디코딩.
                       relay/room/meta 스모크 테스트의 하네스로 사용.
- ``input_expander`` : (col, rot) placement → INPUT 시퀀스 전개.
                       bot/placement.cpp 와의 동등성 테스트 + RL 환경에서 사용.
- ``export_onnx``    : ``.pt`` 체크포인트 → ``model/bots/*.onnx`` export CLI.
"""
