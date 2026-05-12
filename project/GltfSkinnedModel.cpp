#include "GltfSkinnedModel.h"
#include "Model.h"
#include "ModelCommon.h"
#include "Skeleton.h"
#include "TextureManager.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    struct GltfBufferData {
        std::string uri;
    };

    struct GltfBufferViewData {
        int buffer = 0;
        size_t byteOffset = 0;
        size_t byteLength = 0;
        size_t byteStride = 0;
    };

    struct GltfAccessorData {
        int bufferView = -1;
        size_t byteOffset = 0;
        uint32_t componentType = 0;
        size_t count = 0;
        std::string type;
    };

    struct GltfPrimitiveData {
        int positionAccessor = -1;
        int normalAccessor = -1;
        int texcoordAccessor = -1;
        int jointsAccessor = -1;
        int weightsAccessor = -1;
        int indicesAccessor = -1;
    };

    struct GltfMeshData {
        std::string name;
        std::vector<GltfPrimitiveData> primitives;
    };

    struct GltfImageData {
        std::string uri;
    };

    struct GltfSkinData {
        std::string name;
        int inverseBindMatricesAccessor = -1;
    };

    class JsonReader {
    public:
        explicit JsonReader(const std::string& source)
            : source_(source) {
        }

        bool Parse(
            std::vector<GltfBufferData>& buffers,
            std::vector<GltfBufferViewData>& bufferViews,
            std::vector<GltfAccessorData>& accessors,
            std::vector<GltfMeshData>& meshes,
            std::vector<GltfImageData>& images,
            std::vector<GltfSkinData>& skins) {
            buffers.clear();
            bufferViews.clear();
            accessors.clear();
            meshes.clear();
            images.clear();
            skins.clear();

            SkipWhitespace();
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "buffers") {
                    if (!ParseBuffers(buffers)) {
                        return false;
                    }
                } else if (key == "bufferViews") {
                    if (!ParseBufferViews(bufferViews)) {
                        return false;
                    }
                } else if (key == "accessors") {
                    if (!ParseAccessors(accessors)) {
                        return false;
                    }
                } else if (key == "meshes") {
                    if (!ParseMeshes(meshes)) {
                        return false;
                    }
                } else if (key == "images") {
                    if (!ParseImages(images)) {
                        return false;
                    }
                } else if (key == "skins") {
                    if (!ParseSkins(skins)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

    private:
        bool ParseBuffers(std::vector<GltfBufferData>& buffers) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfBufferData buffer{};
                if (!ParseBuffer(buffer)) {
                    return false;
                }
                buffers.push_back(std::move(buffer));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseBuffer(GltfBufferData& buffer) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "uri") {
                    if (!ParseString(buffer.uri)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseBufferViews(std::vector<GltfBufferViewData>& bufferViews) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfBufferViewData bufferView{};
                if (!ParseBufferView(bufferView)) {
                    return false;
                }
                bufferViews.push_back(bufferView);

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseBufferView(GltfBufferViewData& bufferView) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "buffer") {
                    if (!ParseInt(bufferView.buffer)) {
                        return false;
                    }
                } else if (key == "byteOffset") {
                    if (!ParseSize(bufferView.byteOffset)) {
                        return false;
                    }
                } else if (key == "byteLength") {
                    if (!ParseSize(bufferView.byteLength)) {
                        return false;
                    }
                } else if (key == "byteStride") {
                    if (!ParseSize(bufferView.byteStride)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseAccessors(std::vector<GltfAccessorData>& accessors) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfAccessorData accessor{};
                if (!ParseAccessor(accessor)) {
                    return false;
                }
                accessors.push_back(std::move(accessor));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseAccessor(GltfAccessorData& accessor) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "bufferView") {
                    if (!ParseInt(accessor.bufferView)) {
                        return false;
                    }
                } else if (key == "byteOffset") {
                    if (!ParseSize(accessor.byteOffset)) {
                        return false;
                    }
                } else if (key == "componentType") {
                    size_t componentType = 0;
                    if (!ParseSize(componentType)) {
                        return false;
                    }
                    accessor.componentType = static_cast<uint32_t>(componentType);
                } else if (key == "count") {
                    if (!ParseSize(accessor.count)) {
                        return false;
                    }
                } else if (key == "type") {
                    if (!ParseString(accessor.type)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseMeshes(std::vector<GltfMeshData>& meshes) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfMeshData mesh{};
                if (!ParseMesh(mesh)) {
                    return false;
                }
                meshes.push_back(std::move(mesh));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseMesh(GltfMeshData& mesh) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "name") {
                    if (!ParseString(mesh.name)) {
                        return false;
                    }
                } else if (key == "primitives") {
                    if (!ParsePrimitives(mesh.primitives)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParsePrimitives(std::vector<GltfPrimitiveData>& primitives) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfPrimitiveData primitive{};
                if (!ParsePrimitive(primitive)) {
                    return false;
                }
                primitives.push_back(primitive);

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParsePrimitive(GltfPrimitiveData& primitive) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "attributes") {
                    if (!ParsePrimitiveAttributes(primitive)) {
                        return false;
                    }
                } else if (key == "indices") {
                    if (!ParseInt(primitive.indicesAccessor)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParsePrimitiveAttributes(GltfPrimitiveData& primitive) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                int accessorIndex = -1;
                if (!ParseString(key) || !Consume(':') || !ParseInt(accessorIndex)) {
                    return false;
                }

                if (key == "POSITION") {
                    primitive.positionAccessor = accessorIndex;
                } else if (key == "NORMAL") {
                    primitive.normalAccessor = accessorIndex;
                } else if (key == "TEXCOORD_0") {
                    primitive.texcoordAccessor = accessorIndex;
                } else if (key == "JOINTS_0") {
                    primitive.jointsAccessor = accessorIndex;
                } else if (key == "WEIGHTS_0") {
                    primitive.weightsAccessor = accessorIndex;
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseImages(std::vector<GltfImageData>& images) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfImageData image{};
                if (!ParseImage(image)) {
                    return false;
                }
                images.push_back(std::move(image));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseImage(GltfImageData& image) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "uri") {
                    if (!ParseString(image.uri)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseSkins(std::vector<GltfSkinData>& skins) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfSkinData skin{};
                if (!ParseSkin(skin)) {
                    return false;
                }
                skins.push_back(skin);

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseSkin(GltfSkinData& skin) {
            if (!Consume('{')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }

                std::string key;
                if (!ParseString(key) || !Consume(':')) {
                    return false;
                }

                if (key == "name") {
                    if (!ParseString(skin.name)) {
                        return false;
                    }
                } else if (key == "inverseBindMatrices") {
                    if (!ParseInt(skin.inverseBindMatricesAccessor)) {
                        return false;
                    }
                } else {
                    if (!SkipValue()) {
                        return false;
                    }
                }

                SkipWhitespace();
                if (Consume('}')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseString(std::string& outValue) {
            SkipWhitespace();
            if (cursor_ >= source_.size() || source_[cursor_] != '"') {
                return false;
            }

            ++cursor_;
            std::ostringstream builder;
            while (cursor_ < source_.size()) {
                char ch = source_[cursor_++];
                if (ch == '\\') {
                    if (cursor_ >= source_.size()) {
                        return false;
                    }
                    char escaped = source_[cursor_++];
                    switch (escaped) {
                    case '"': builder << '"'; break;
                    case '\\': builder << '\\'; break;
                    case '/': builder << '/'; break;
                    case 'b': builder << '\b'; break;
                    case 'f': builder << '\f'; break;
                    case 'n': builder << '\n'; break;
                    case 'r': builder << '\r'; break;
                    case 't': builder << '\t'; break;
                    default: return false;
                    }
                    continue;
                }

                if (ch == '"') {
                    outValue = builder.str();
                    return true;
                }
                builder << ch;
            }
            return false;
        }

        bool ParseInt(int& outValue) {
            SkipWhitespace();
            const char* begin = source_.c_str() + cursor_;
            char* end = nullptr;
            long value = std::strtol(begin, &end, 10);
            if (end == begin) {
                return false;
            }
            cursor_ = static_cast<size_t>(end - source_.c_str());
            outValue = static_cast<int>(value);
            return true;
        }

        bool ParseSize(size_t& outValue) {
            SkipWhitespace();
            const char* begin = source_.c_str() + cursor_;
            char* end = nullptr;
            unsigned long long value = std::strtoull(begin, &end, 10);
            if (end == begin) {
                return false;
            }
            cursor_ = static_cast<size_t>(end - source_.c_str());
            outValue = static_cast<size_t>(value);
            return true;
        }

        bool ParseNumberToken() {
            SkipWhitespace();
            const char* begin = source_.c_str() + cursor_;
            char* end = nullptr;
            std::strtod(begin, &end);
            if (end == begin) {
                return false;
            }
            cursor_ = static_cast<size_t>(end - source_.c_str());
            return true;
        }

        bool SkipValue() {
            SkipWhitespace();
            if (cursor_ >= source_.size()) {
                return false;
            }

            char ch = source_[cursor_];
            if (ch == '"') {
                std::string ignored;
                return ParseString(ignored);
            }
            if (ch == '{') {
                ++cursor_;
                while (true) {
                    SkipWhitespace();
                    if (Consume('}')) {
                        return true;
                    }
                    std::string key;
                    if (!ParseString(key) || !Consume(':') || !SkipValue()) {
                        return false;
                    }
                    SkipWhitespace();
                    if (Consume('}')) {
                        return true;
                    }
                    if (!Consume(',')) {
                        return false;
                    }
                }
            }
            if (ch == '[') {
                ++cursor_;
                while (true) {
                    SkipWhitespace();
                    if (Consume(']')) {
                        return true;
                    }
                    if (!SkipValue()) {
                        return false;
                    }
                    SkipWhitespace();
                    if (Consume(']')) {
                        return true;
                    }
                    if (!Consume(',')) {
                        return false;
                    }
                }
            }
            if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+') {
            return ParseNumberToken();
        }
            if (source_.compare(cursor_, 4, "true") == 0) {
                cursor_ += 4;
                return true;
            }
            if (source_.compare(cursor_, 5, "false") == 0) {
                cursor_ += 5;
                return true;
            }
            if (source_.compare(cursor_, 4, "null") == 0) {
                cursor_ += 4;
                return true;
            }
            return false;
        }

        bool Consume(char expected) {
            SkipWhitespace();
            if (cursor_ >= source_.size() || source_[cursor_] != expected) {
                return false;
            }
            ++cursor_;
            return true;
        }

        void SkipWhitespace() {
            while (cursor_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[cursor_]))) {
                ++cursor_;
            }
        }

        const std::string& source_;
        size_t cursor_ = 0;
    };

    struct AccessorView {
        const uint8_t* data = nullptr;
        size_t count = 0;
        size_t stride = 0;
        size_t componentCount = 0;
        uint32_t componentType = 0;
    };

    size_t GetComponentCount(const std::string& type) {
        if (type == "SCALAR") {
            return 1;
        }
        if (type == "VEC2") {
            return 2;
        }
        if (type == "VEC3") {
            return 3;
        }
        if (type == "VEC4") {
            return 4;
        }
        if (type == "MAT4") {
            return 16;
        }
        return 0;
    }

    size_t GetComponentSize(uint32_t componentType) {
        switch (componentType) {
        case 5120:
        case 5121:
            return 1;
        case 5122:
        case 5123:
            return 2;
        case 5125:
        case 5126:
            return 4;
        default:
            return 0;
        }
    }

    std::string ResolveRelativePath(const std::string& baseFilePath, const std::string& relativePath) {
        return (std::filesystem::path(baseFilePath).parent_path() / std::filesystem::path(relativePath)).generic_string();
    }

    bool LoadFileToString(const std::string& filePath, std::string& outText) {
        std::ifstream stream{ std::filesystem::path(filePath) };
        if (!stream.is_open()) {
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        outText = buffer.str();
        return true;
    }

    bool LoadBinaryFile(const std::string& filePath, std::vector<uint8_t>& outBinary) {
        std::ifstream stream{ std::filesystem::path(filePath), std::ios::binary };
        if (!stream.is_open()) {
            return false;
        }

        stream.seekg(0, std::ios::end);
        std::streamoff byteLength = stream.tellg();
        if (byteLength <= 0) {
            outBinary.clear();
            return true;
        }
        stream.seekg(0, std::ios::beg);

        outBinary.resize(static_cast<size_t>(byteLength));
        stream.read(reinterpret_cast<char*>(outBinary.data()), byteLength);
        return stream.good() || stream.eof();
    }

    bool MakeAccessorView(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        AccessorView& outView) {
        if (accessorIndex < 0 || accessorIndex >= static_cast<int>(accessors.size())) {
            return false;
        }

        const GltfAccessorData& accessor = accessors[static_cast<size_t>(accessorIndex)];
        if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(bufferViews.size())) {
            return false;
        }

        const GltfBufferViewData& bufferView = bufferViews[static_cast<size_t>(accessor.bufferView)];
        size_t componentCount = GetComponentCount(accessor.type);
        size_t componentSize = GetComponentSize(accessor.componentType);
        if (componentCount == 0 || componentSize == 0) {
            return false;
        }

        size_t stride = bufferView.byteStride;
        if (stride == 0) {
            stride = componentCount * componentSize;
        }

        size_t startOffset = bufferView.byteOffset + accessor.byteOffset;
        if (startOffset >= binary.size()) {
            return false;
        }

        outView.data = binary.data() + startOffset;
        outView.count = accessor.count;
        outView.stride = stride;
        outView.componentCount = componentCount;
        outView.componentType = accessor.componentType;
        return true;
    }

    float ReadFloat(const uint8_t* bytes) {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof(float));
        return value;
    }

    Vector3 TransformPosition(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2]
        };
    }

    Vector3 TransformDirection(const Vector3& value, const Matrix4x4& matrix) {
        return {
            value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0],
            value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1],
            value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2]
        };
    }

    float VectorLength(const Vector3& value) {
        return std::sqrt((value.x * value.x) + (value.y * value.y) + (value.z * value.z));
    }

    Vector3 NormalizeVector(const Vector3& value) {
        float length = VectorLength(value);
        if (length <= 0.000001f) {
            return { 0.0f, 1.0f, 0.0f };
        }

        float invLength = 1.0f / length;
        return { value.x * invLength, value.y * invLength, value.z * invLength };
    }

    bool ReadVector3Accessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<Vector3>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentType != 5126 || view.componentCount != 3) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            outValues[elementIndex] = {
                ReadFloat(source + sizeof(float) * 0),
                ReadFloat(source + sizeof(float) * 1),
                ReadFloat(source + sizeof(float) * 2)
            };
        }
        return true;
    }

    bool ReadVector2Accessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<Vector2>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentType != 5126 || view.componentCount != 2) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            outValues[elementIndex] = {
                ReadFloat(source + sizeof(float) * 0),
                ReadFloat(source + sizeof(float) * 1)
            };
        }
        return true;
    }

    bool ReadJointAccessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<std::array<uint32_t, 4>>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentCount != 4) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            std::array<uint32_t, 4> values{};
            for (size_t componentIndex = 0; componentIndex < 4; ++componentIndex) {
                switch (view.componentType) {
                case 5121:
                    values[componentIndex] = static_cast<uint32_t>(*(source + componentIndex));
                    break;
                case 5123: {
                    uint16_t value = 0;
                    std::memcpy(&value, source + sizeof(uint16_t) * componentIndex, sizeof(uint16_t));
                    values[componentIndex] = static_cast<uint32_t>(value);
                    break;
                }
                default:
                    return false;
                }
            }
            outValues[elementIndex] = values;
        }
        return true;
    }

    bool ReadWeightAccessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<std::array<float, 4>>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentType != 5126 || view.componentCount != 4) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            std::array<float, 4> values{
                ReadFloat(source + sizeof(float) * 0),
                ReadFloat(source + sizeof(float) * 1),
                ReadFloat(source + sizeof(float) * 2),
                ReadFloat(source + sizeof(float) * 3)
            };

            float weightSum = values[0] + values[1] + values[2] + values[3];
            if (weightSum > 0.000001f) {
                float invWeightSum = 1.0f / weightSum;
                for (float& weight : values) {
                    weight *= invWeightSum;
                }
            }
            outValues[elementIndex] = values;
        }
        return true;
    }

    bool ReadIndexAccessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<uint32_t>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentCount != 1) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            switch (view.componentType) {
            case 5121:
                outValues[elementIndex] = static_cast<uint32_t>(*source);
                break;
            case 5123: {
                uint16_t value = 0;
                std::memcpy(&value, source, sizeof(uint16_t));
                outValues[elementIndex] = static_cast<uint32_t>(value);
                break;
            }
            case 5125: {
                uint32_t value = 0;
                std::memcpy(&value, source, sizeof(uint32_t));
                outValues[elementIndex] = value;
                break;
            }
            default:
                return false;
            }
        }
        return true;
    }

    bool ReadMatrixAccessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<Matrix4x4>& outMatrices) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentType != 5126 || view.componentCount != 16) {
            return false;
        }

        outMatrices.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            float values[16]{};
            std::memcpy(values, source, sizeof(values));

            Matrix4x4 matrix{};
            // glTF is column-major. The engine uses row-vector matrices, so
            // reading the raw sequence into row-major slots gives the transposed matrix.
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 4; ++column) {
                    matrix.m[row][column] = values[row * 4 + column];
                }
            }
            outMatrices[elementIndex] = matrix;
        }
        return true;
    }
}

GltfSkinnedModel::~GltfSkinnedModel() = default;

bool GltfSkinnedModel::InitializeStatic(ModelCommon* modelCommon, const std::string& gltfPath) {
    if (!modelCommon) {
        return false;
    }

    std::string gltfText;
    if (!LoadFileToString(gltfPath, gltfText)) {
        return false;
    }

    std::vector<GltfBufferData> buffers;
    std::vector<GltfBufferViewData> bufferViews;
    std::vector<GltfAccessorData> accessors;
    std::vector<GltfMeshData> meshes;
    std::vector<GltfImageData> images;
    std::vector<GltfSkinData> skins;
    JsonReader reader(gltfText);
    if (!reader.Parse(buffers, bufferViews, accessors, meshes, images, skins) ||
        buffers.empty() || meshes.empty() || meshes.front().primitives.empty()) {
        return false;
    }

    std::vector<uint8_t> binary;
    if (!LoadBinaryFile(ResolveRelativePath(gltfPath, buffers.front().uri), binary)) {
        return false;
    }

    const GltfPrimitiveData& primitive = meshes.front().primitives.front();

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<uint32_t> indices;

    if (!ReadVector3Accessor(binary, bufferViews, accessors, primitive.positionAccessor, positions) ||
        !ReadVector3Accessor(binary, bufferViews, accessors, primitive.normalAccessor, normals) ||
        !ReadVector2Accessor(binary, bufferViews, accessors, primitive.texcoordAccessor, texcoords) ||
        !ReadIndexAccessor(binary, bufferViews, accessors, primitive.indicesAccessor, indices)) {
        return false;
    }

    if (positions.size() != normals.size() || positions.size() != texcoords.size()) {
        return false;
    }

    Model::ModelData modelData{};
    modelData.vertices.reserve(indices.size());
    for (uint32_t vertexIndex : indices) {
        if (vertexIndex >= positions.size()) {
            return false;
        }

        modelData.vertices.push_back({
            { positions[vertexIndex].x, positions[vertexIndex].y, positions[vertexIndex].z, 1.0f },
            texcoords[vertexIndex],
            normals[vertexIndex]
            });
    }

    const std::string texturePath = !images.empty() && !images.front().uri.empty()
        ? ResolveRelativePath(gltfPath, images.front().uri)
        : std::string("resources/obj/axis/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture(texturePath);
    modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);

    model_ = std::make_unique<Model>();
    model_->Initialize(modelCommon, modelData);
    skeleton_ = nullptr;
    sourceVertices_.clear();
    inverseBindMatrices_.clear();
    jointPalette_.clear();
    return true;
}

bool GltfSkinnedModel::Initialize(ModelCommon* modelCommon, Skeleton* skeleton, const std::string& gltfPath) {
    if (!modelCommon || !skeleton) {
        return false;
    }

    std::string gltfText;
    if (!LoadFileToString(gltfPath, gltfText)) {
        return false;
    }

    std::vector<GltfBufferData> buffers;
    std::vector<GltfBufferViewData> bufferViews;
    std::vector<GltfAccessorData> accessors;
    std::vector<GltfMeshData> meshes;
    std::vector<GltfImageData> images;
    std::vector<GltfSkinData> skins;
    JsonReader reader(gltfText);
    if (!reader.Parse(buffers, bufferViews, accessors, meshes, images, skins) ||
        buffers.empty() || meshes.empty() || meshes.front().primitives.empty() || skins.empty()) {
        return false;
    }

    std::vector<uint8_t> binary;
    const std::string binaryPath = ResolveRelativePath(gltfPath, buffers.front().uri);
    if (!LoadBinaryFile(binaryPath, binary)) {
        return false;
    }

    const GltfPrimitiveData& primitive = meshes.front().primitives.front();

    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::vector<std::array<uint32_t, 4>> joints;
    std::vector<std::array<float, 4>> weights;
    std::vector<uint32_t> indices;
    std::vector<Matrix4x4> inverseBindMatrices;

    if (!ReadVector3Accessor(binary, bufferViews, accessors, primitive.positionAccessor, positions) ||
        !ReadVector3Accessor(binary, bufferViews, accessors, primitive.normalAccessor, normals) ||
        !ReadVector2Accessor(binary, bufferViews, accessors, primitive.texcoordAccessor, texcoords) ||
        !ReadJointAccessor(binary, bufferViews, accessors, primitive.jointsAccessor, joints) ||
        !ReadWeightAccessor(binary, bufferViews, accessors, primitive.weightsAccessor, weights) ||
        !ReadIndexAccessor(binary, bufferViews, accessors, primitive.indicesAccessor, indices) ||
        !ReadMatrixAccessor(binary, bufferViews, accessors, skins.front().inverseBindMatricesAccessor, inverseBindMatrices)) {
        return false;
    }

    if (positions.size() != normals.size() ||
        positions.size() != texcoords.size() ||
        positions.size() != joints.size() ||
        positions.size() != weights.size() ||
        inverseBindMatrices.empty()) {
        return false;
    }

    sourceVertices_.clear();
    sourceVertices_.reserve(indices.size());

    Model::ModelData modelData{};
    modelData.vertices.reserve(indices.size());

    for (uint32_t vertexIndex : indices) {
        if (vertexIndex >= positions.size()) {
            return false;
        }

        SourceVertex sourceVertex{};
        sourceVertex.position = positions[vertexIndex];
        sourceVertex.normal = normals[vertexIndex];
        sourceVertex.texcoord = texcoords[vertexIndex];
        sourceVertex.joints = joints[vertexIndex];
        sourceVertex.weights = weights[vertexIndex];
        sourceVertices_.push_back(sourceVertex);

        modelData.vertices.push_back({
            { sourceVertex.position.x, sourceVertex.position.y, sourceVertex.position.z, 1.0f },
            sourceVertex.texcoord,
            sourceVertex.normal
            });
    }

    const std::string texturePath = ResolveRelativePath(gltfPath, "white.png");
    TextureManager::GetInstance()->LoadTexture(texturePath);
    modelData.material.textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(texturePath);

    model_ = std::make_unique<Model>();
    model_->Initialize(modelCommon, modelData);

    skeleton_ = skeleton;
    inverseBindMatrices_ = std::move(inverseBindMatrices);
    jointPalette_.resize((std::max)(inverseBindMatrices_.size(), skeleton_->joints.size()), MatrixMath::MakeIdentity4x4());
    UpdateSkinning();
    return true;
}

void GltfSkinnedModel::UpdateSkinning() {
    if (!model_ || !skeleton_ || sourceVertices_.empty()) {
        return;
    }

    const size_t paletteSize = (std::min)(inverseBindMatrices_.size(), skeleton_->joints.size());
    for (size_t jointIndex = 0; jointIndex < paletteSize; ++jointIndex) {
        jointPalette_[jointIndex] = MatrixMath::Multipty(
            inverseBindMatrices_[jointIndex],
            skeleton_->joints[jointIndex].worldMatrix);
    }
    for (size_t jointIndex = paletteSize; jointIndex < jointPalette_.size(); ++jointIndex) {
        jointPalette_[jointIndex] = MatrixMath::MakeIdentity4x4();
    }

    std::vector<Model::VertexData> skinnedVertices;
    skinnedVertices.resize(sourceVertices_.size());

    for (size_t vertexIndex = 0; vertexIndex < sourceVertices_.size(); ++vertexIndex) {
        const SourceVertex& sourceVertex = sourceVertices_[vertexIndex];
        Vector3 skinnedPosition{ 0.0f, 0.0f, 0.0f };
        Vector3 skinnedNormal{ 0.0f, 0.0f, 0.0f };
        float accumulatedWeight = 0.0f;

        for (size_t influenceIndex = 0; influenceIndex < sourceVertex.joints.size(); ++influenceIndex) {
            uint32_t jointIndex = sourceVertex.joints[influenceIndex];
            float weight = sourceVertex.weights[influenceIndex];
            if (weight <= 0.000001f || jointIndex >= jointPalette_.size()) {
                continue;
            }

            const Matrix4x4& jointMatrix = jointPalette_[jointIndex];
            Vector3 transformedPosition = TransformPosition(sourceVertex.position, jointMatrix);
            Vector3 transformedNormal = TransformDirection(sourceVertex.normal, jointMatrix);

            skinnedPosition.x += transformedPosition.x * weight;
            skinnedPosition.y += transformedPosition.y * weight;
            skinnedPosition.z += transformedPosition.z * weight;

            skinnedNormal.x += transformedNormal.x * weight;
            skinnedNormal.y += transformedNormal.y * weight;
            skinnedNormal.z += transformedNormal.z * weight;
            accumulatedWeight += weight;
        }

        if (accumulatedWeight <= 0.000001f) {
            skinnedPosition = sourceVertex.position;
            skinnedNormal = sourceVertex.normal;
        } else {
            skinnedNormal = NormalizeVector(skinnedNormal);
        }

        skinnedVertices[vertexIndex] = {
            { skinnedPosition.x, skinnedPosition.y, skinnedPosition.z, 1.0f },
            sourceVertex.texcoord,
            skinnedNormal
        };
    }

    model_->SetVertices(skinnedVertices);
}
