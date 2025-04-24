#ifndef PYTHON_INTEROP_H
#define PYTHON_INTEROP_H

#include <string>
#include <vector>
#include <Python.h>
#include "game.h"

// Python 인터페이스 클래스
class PythonInterface {
public:
    // 싱글톤 인스턴스 얻기
    static PythonInterface& Instance();
    
    // 초기화 및 정리
    bool Initialize();
    void Finalize();
    
    // 모델 관리
    bool LoadModel(const std::string& modelPath, const std::string& modelType);
    std::vector<float> Predict(const std::string& modelType, const std::vector<float>& input);
    
private:
    PythonInterface();
    ~PythonInterface();
    
    // 싱글톤 인스턴스
    static std::unique_ptr<PythonInterface> instance;
    
    // Python 상태
    bool initialized;
    PyObject* pModule;
    PyObject* pLoadFunc;
    PyObject* pPredictFunc;
    
    // 게임 상태를 Python 리스트로 변환
    PyObject* CreatePythonInputFromGame(const Game& game);
};

#endif // PYTHON_INTEROP_H
