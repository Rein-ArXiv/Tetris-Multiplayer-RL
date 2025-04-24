#include "python_interop.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

// 싱글톤 인스턴스 초기화
std::unique_ptr<PythonInterface> PythonInterface::instance = nullptr;

PythonInterface& PythonInterface::Instance() {
    if (!instance) {
        instance = std::unique_ptr<PythonInterface>(new PythonInterface());
    }
    return *instance;
}

PythonInterface::PythonInterface() 
    : initialized(false), pModule(nullptr), pLoadFunc(nullptr), pPredictFunc(nullptr) {
}

PythonInterface::~PythonInterface() {
    Finalize();
}

bool PythonInterface::Initialize() {
    if (initialized) {
        return true;
    }
    
    try {
        // Python 인터프리터 초기화
        Py_Initialize();
        if (!Py_IsInitialized()) {
            std::cerr << "Python 인터프리터 초기화 실패" << std::endl;
            return false;
        }
        
        // 현재 실행 경로를 Python 경로에 추가
        PyRun_SimpleString("import sys\nsys.path.append('.')\nsys.path.append('./python')");
        
        // Python 모듈 로드
        PyObject* pName = PyUnicode_DecodeFSDefault("tetris_ai_interface");
        pModule = PyImport_Import(pName);
        Py_DECREF(pName);
        
        if (!pModule) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "tetris_ai_interface 모듈을 로드할 수 없습니다." << std::endl;
            return false;
        }
        
        // 필요한 함수 참조 얻기
        pLoadFunc = PyObject_GetAttrString(pModule, "load_model");
        if (!pLoadFunc || !PyCallable_Check(pLoadFunc)) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "load_model 함수를 찾을 수 없습니다." << std::endl;
            Finalize();
            return false;
        }
        
        pPredictFunc = PyObject_GetAttrString(pModule, "predict");
        if (!pPredictFunc || !PyCallable_Check(pPredictFunc)) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "predict 함수를 찾을 수 없습니다." << std::endl;
            Finalize();
            return false;
        }
        
        initialized = true;
        std::cout << "Python 인터페이스 초기화 성공" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Python 인터페이스 초기화 중 예외 발생: " << e.what() << std::endl;
        Finalize();
        return false;
    }
}

void PythonInterface::Finalize() {
    if (!initialized) {
        return;
    }
    
    // Python 객체 참조 해제
    if (pLoadFunc) {
        Py_DECREF(pLoadFunc);
        pLoadFunc = nullptr;
    }
    
    if (pPredictFunc) {
        Py_DECREF(pPredictFunc);
        pPredictFunc = nullptr;
    }
    
    if (pModule) {
        Py_DECREF(pModule);
        pModule = nullptr;
    }
    
    // Python 인터프리터 종료
    if (Py_IsInitialized()) {
        Py_Finalize();
    }
    
    initialized = false;
    std::cout << "Python 인터페이스 종료" << std::endl;
}

bool PythonInterface::LoadModel(const std::string& modelPath, const std::string& modelType) {
    if (!initialized && !Initialize()) {
        std::cerr << "Python 인터페이스가 초기화되지 않았습니다." << std::endl;
        return false;
    }
    
    try {
        // 함수 인자 생성
        PyObject* pArgs = PyTuple_New(2);
        
        PyObject* pModelPath = PyUnicode_DecodeFSDefault(modelPath.c_str());
        PyObject* pModelType = PyUnicode_DecodeFSDefault(modelType.c_str());
        
        // 튜플에 인자 설정 (새로운 참조 전달)
        PyTuple_SetItem(pArgs, 0, pModelPath);  // pModelPath의 소유권 이전
        PyTuple_SetItem(pArgs, 1, pModelType);  // pModelType의 소유권 이전
        
        // 함수 호출
        PyObject* pResult = PyObject_CallObject(pLoadFunc, pArgs);
        Py_DECREF(pArgs);
        
        if (!pResult) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "모델 로드 중 Python 오류 발생" << std::endl;
            return false;
        }
        
        // 결과 해석
        bool success = PyObject_IsTrue(pResult) == 1;
        Py_DECREF(pResult);
        
        if (success) {
            std::cout << "모델 로드 성공: " << modelType << " 경로: " << modelPath << std::endl;
        } else {
            std::cerr << "모델 로드 실패: " << modelType << " 경로: " << modelPath << std::endl;
        }
        
        return success;
    }
    catch (const std::exception& e) {
        std::cerr << "모델 로드 중 예외 발생: " << e.what() << std::endl;
        return false;
    }
}

std::vector<float> PythonInterface::Predict(const std::string& modelType, const std::vector<float>& input) {
    std::vector<float> emptyResult;
    
    if (!initialized && !Initialize()) {
        std::cerr << "Python 인터페이스가 초기화되지 않았습니다." << std::endl;
        return emptyResult;
    }
    
    try {
        // 입력 리스트 생성
        PyObject* pInputList = PyList_New(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            PyObject* pValue = PyFloat_FromDouble(static_cast<double>(input[i]));
            if (!pValue) {
                Py_DECREF(pInputList);
                std::cerr << "PyFloat_FromDouble 실패" << std::endl;
                return emptyResult;
            }
            PyList_SetItem(pInputList, i, pValue);  // pValue의 소유권 이전
        }
        
        // 함수 인자 생성
        PyObject* pArgs = PyTuple_New(2);
        PyObject* pModelType = PyUnicode_DecodeFSDefault(modelType.c_str());
        
        PyTuple_SetItem(pArgs, 0, pModelType);    // pModelType의 소유권 이전
        PyTuple_SetItem(pArgs, 1, pInputList);    // pInputList의 소유권 이전
        
        // 함수 호출
        PyObject* pResult = PyObject_CallObject(pPredictFunc, pArgs);
        Py_DECREF(pArgs);
        
        if (!pResult) {
            if (PyErr_Occurred()) {
                PyErr_Print();
            }
            std::cerr << "예측 중 Python 오류 발생" << std::endl;
            return emptyResult;
        }
        
        // 결과가 리스트인지 확인
        if (!PyList_Check(pResult)) {
            Py_DECREF(pResult);
            std::cerr << "예측 결과가 리스트가 아닙니다." << std::endl;
            return emptyResult;
        }
        
        // Python 리스트를 C++ 벡터로 변환
        std::vector<float> result;
        Py_ssize_t size = PyList_Size(pResult);
        result.reserve(size);
        
        for (Py_ssize_t i = 0; i < size; ++i) {
            PyObject* pItem = PyList_GetItem(pResult, i);  // 빌려온 참조
            
            if (PyFloat_Check(pItem)) {
                result.push_back(static_cast<float>(PyFloat_AsDouble(pItem)));
            }
            else if (PyLong_Check(pItem)) {
                result.push_back(static_cast<float>(PyLong_AsLong(pItem)));
            }
            else {
                std::cerr << "예측 결과의 항목이 숫자가 아닙니다." << std::endl;
                result.push_back(0.0f);
            }
        }
        
        Py_DECREF(pResult);
        return result;
    }
    catch (const std::exception& e) {
        std::cerr << "예측 중 예외 발생: " << e.what() << std::endl;
        return emptyResult;
    }
}

PyObject* PythonInterface::CreatePythonInputFromGame(const Game& game) {
    // 게임 보드 상태 가져오기
    const auto& boardState = game.GetBoardState();
    
    // Python 리스트 생성 (2차원 배열)
    PyObject* pBoardList = PyList_New(boardState.size());
    if (!pBoardList) {
        std::cerr << "Python 보드 리스트 생성 실패" << std::endl;
        return nullptr;
    }
    
    for (size_t i = 0; i < boardState.size(); ++i) {
        PyObject* pRow = PyList_New(boardState[i].size());
        if (!pRow) {
            Py_DECREF(pBoardList);
            std::cerr << "Python 행 리스트 생성 실패" << std::endl;
            return nullptr;
        }
        
        for (size_t j = 0; j < boardState[i].size(); ++j) {
            PyObject* pCell = PyLong_FromLong(static_cast<long>(boardState[i][j]));
            if (!pCell) {
                Py_DECREF(pRow);
                Py_DECREF(pBoardList);
                std::cerr << "Python 셀 값 생성 실패" << std::endl;
                return nullptr;
            }
            PyList_SetItem(pRow, j, pCell);  // pCell의 소유권 이전
        }
        
        PyList_SetItem(pBoardList, i, pRow);  // pRow의 소유권 이전
    }
    
    // 현재 블록 정보 추가
    // 이 구현에서는 생략했지만, 필요하다면 블록 타입, 위치, 회전 상태 등의 정보를 추가할 수 있습니다.
    
    return pBoardList;
}

std::vector<float> PythonInterface::GameStateToModelInput(const Game& game) {
    std::vector<float> input;
    
    // 게임 보드 상태 변환
    const auto& boardState = game.GetBoardState();
    for (const auto& row : boardState) {
        for (int cell : row) {
            // 셀 값을 0 또는 1로 변환 (0=빈칸, 1=블록)
            input.push_back(cell > 0 ? 1.0f : 0.0f);
        }
    }
    
    // 현재 블록 정보 추가
    const Block& currentBlock = game.GetCurrentBlock();
    int blockType = static_cast<int>(currentBlock.type);
    
    // 원-핫 인코딩으로 블록 타입 표현
    for (int i = 1; i <= 7; ++i) {
        input.push_back(i == blockType ? 1.0f : 0.0f);
    }
    
    // 다음 블록 정보 추가
    const Block& nextBlock = game.GetNextBlock();
    int nextBlockType = static_cast<int>(nextBlock.type);
    
    // 원-핫 인코딩으로 다음 블록 타입 표현
    for (int i = 1; i <= 7; ++i) {
        input.push_back(i == nextBlockType ? 1.0f : 0.0f);
    }
    
    // 추가 정보: 현재 블록의 위치 및 회전 상태
    input.push_back(static_cast<float>(currentBlock.rotationState));
    
    // 현재 블록의 위치 정보
    std::vector<Position> positions = currentBlock.GetCellPositions();
    float avgRow = 0.0f;
    float avgCol = 0.0f;
    
    for (const auto& pos : positions) {
        avgRow += pos.row;
        avgCol += pos.column;
    }
    
    if (!positions.empty()) {
        avgRow /= positions.size();
        avgCol /= positions.size();
    }
    
    // 위치 정보 정규화 (0-1 범위로)
    input.push_back(avgRow / boardState.size());
    input.push_back(avgCol / boardState[0].size());
    
    return input;
}