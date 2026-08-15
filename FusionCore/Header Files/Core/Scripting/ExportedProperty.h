#pragma once
#include <string>
#include <variant>
#include <glm/glm.hpp>
#include <pybind11/pybind11.h>
#include <cstdint>

class Object; 

enum class ExportType {
    Default,
    Slider,
    AngleSlider,
    ColorEdit,
    ColorPicker,
    Drag,
    File,
};

struct ObjectRef {
    Object* ptr = nullptr;
    uint64_t id = 0;
};

using ExportedValue = std::variant<std::string, int, float, bool, glm::vec2, glm::vec3, glm::vec4, ObjectRef>;

struct ExportedProperty {
    std::string name;
    ExportedValue value;
    ExportType displayType = ExportType::Default;
    std::string prefix;
    std::string suffix;
    float min = 0.0f;
    float max = 1.0f;
    std::string fileFilter = "*.*";
    uint64_t objectRefDeleteCallbackId = -1;
};

struct ExportMarker {
    pybind11::object value;
    ExportType type = ExportType::Default;
    std::string prefix;
    std::string suffix;
    float min = 0.0f;
    float max = 1.0f;
    std::string fileFilter = "*.*";
};