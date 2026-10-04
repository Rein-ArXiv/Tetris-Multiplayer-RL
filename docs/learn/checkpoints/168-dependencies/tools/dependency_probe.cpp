#include <onnxruntime_cxx_api.h>
#include <exception>
#include <iostream>

int main() {
    try {
        Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "dependency-probe");
        std::cout << "ONNX Runtime " << OrtGetApiBase()->GetVersionString() << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
