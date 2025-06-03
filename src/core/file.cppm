module;

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>

export module core.file;

export namespace core::file {

inline std::vector<char> loadBinaryFile(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + std::string(path));

    size_t size = file.tellg();
    std::vector<char> buffer(size);

    file.seekg(0);
    file.read(buffer.data(), size);

    return buffer;
}
}
