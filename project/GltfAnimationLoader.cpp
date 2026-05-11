#include "GltfAnimationLoader.h"
#include "Skeleton.h"
#include "Matrix4x4.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    constexpr float kAnimationTimeEpsilon = 0.0001f;

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

    struct GltfNodeData {
        std::string name;
    };

    struct GltfSkinData {
        std::string name;
        std::vector<int> joints;
    };

    struct GltfAnimationSamplerData {
        int inputAccessor = -1;
        int outputAccessor = -1;
        std::string interpolation = "LINEAR";
    };

    struct GltfAnimationChannelData {
        int samplerIndex = -1;
        int targetNode = -1;
        std::string targetPath;
    };

    struct GltfAnimationData {
        std::string name;
        std::vector<GltfAnimationSamplerData> samplers;
        std::vector<GltfAnimationChannelData> channels;
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
            std::vector<GltfNodeData>& nodes,
            std::vector<GltfSkinData>& skins,
            std::vector<GltfAnimationData>& animations) {
            buffers.clear();
            bufferViews.clear();
            accessors.clear();
            nodes.clear();
            skins.clear();
            animations.clear();

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
                } else if (key == "nodes") {
                    if (!ParseNodes(nodes)) {
                        return false;
                    }
                } else if (key == "skins") {
                    if (!ParseSkins(skins)) {
                        return false;
                    }
                } else if (key == "animations") {
                    if (!ParseAnimations(animations)) {
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

        bool ParseNodes(std::vector<GltfNodeData>& nodes) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfNodeData node{};
                if (!ParseNode(node)) {
                    return false;
                }
                nodes.push_back(std::move(node));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseNode(GltfNodeData& node) {
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
                    if (!ParseString(node.name)) {
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
                skins.push_back(std::move(skin));

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
                } else if (key == "joints") {
                    if (!ParseIntArray(skin.joints)) {
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

        bool ParseAnimations(std::vector<GltfAnimationData>& animations) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfAnimationData animation{};
                if (!ParseAnimation(animation)) {
                    return false;
                }
                animations.push_back(std::move(animation));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseAnimation(GltfAnimationData& animation) {
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
                    if (!ParseString(animation.name)) {
                        return false;
                    }
                } else if (key == "samplers") {
                    if (!ParseAnimationSamplers(animation.samplers)) {
                        return false;
                    }
                } else if (key == "channels") {
                    if (!ParseAnimationChannels(animation.channels)) {
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

        bool ParseAnimationSamplers(std::vector<GltfAnimationSamplerData>& samplers) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfAnimationSamplerData sampler{};
                if (!ParseAnimationSampler(sampler)) {
                    return false;
                }
                samplers.push_back(std::move(sampler));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseAnimationSampler(GltfAnimationSamplerData& sampler) {
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

                if (key == "input") {
                    if (!ParseInt(sampler.inputAccessor)) {
                        return false;
                    }
                } else if (key == "output") {
                    if (!ParseInt(sampler.outputAccessor)) {
                        return false;
                    }
                } else if (key == "interpolation") {
                    if (!ParseString(sampler.interpolation)) {
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

        bool ParseAnimationChannels(std::vector<GltfAnimationChannelData>& channels) {
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                GltfAnimationChannelData channel{};
                if (!ParseAnimationChannel(channel)) {
                    return false;
                }
                channels.push_back(std::move(channel));

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseAnimationChannel(GltfAnimationChannelData& channel) {
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

                if (key == "sampler") {
                    if (!ParseInt(channel.samplerIndex)) {
                        return false;
                    }
                } else if (key == "target") {
                    if (!ParseAnimationTarget(channel)) {
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

        bool ParseAnimationTarget(GltfAnimationChannelData& channel) {
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

                if (key == "node") {
                    if (!ParseInt(channel.targetNode)) {
                        return false;
                    }
                } else if (key == "path") {
                    if (!ParseString(channel.targetPath)) {
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

        bool ParseIntArray(std::vector<int>& values) {
            values.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                int value = 0;
                if (!ParseInt(value)) {
                    return false;
                }
                values.push_back(value);

                SkipWhitespace();
                if (Consume(']')) {
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

    struct Vec3Curve {
        std::vector<float> times;
        std::vector<Vector3> values;
        std::string interpolation = "LINEAR";
        bool valid = false;
    };

    struct TrackCurves {
        Vec3Curve translation;
        Vec3Curve rotation;
        Vec3Curve scale;
    };

    size_t GetComponentCount(const std::string& type) {
        if (type == "SCALAR") { return 1; }
        if (type == "VEC2") { return 2; }
        if (type == "VEC3") { return 3; }
        if (type == "VEC4") { return 4; }
        if (type == "MAT4") { return 16; }
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

    bool ReadScalarFloatAccessor(
        const std::vector<uint8_t>& binary,
        const std::vector<GltfBufferViewData>& bufferViews,
        const std::vector<GltfAccessorData>& accessors,
        int accessorIndex,
        std::vector<float>& outValues) {
        AccessorView view{};
        if (!MakeAccessorView(binary, bufferViews, accessors, accessorIndex, view) ||
            view.componentType != 5126 || view.componentCount != 1) {
            return false;
        }

        outValues.resize(view.count);
        for (size_t elementIndex = 0; elementIndex < view.count; ++elementIndex) {
            const uint8_t* source = view.data + (view.stride * elementIndex);
            outValues[elementIndex] = ReadFloat(source);
        }
        return true;
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

    bool ReadQuaternionAccessor(
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
            outValues[elementIndex] = {
                ReadFloat(source + sizeof(float) * 0),
                ReadFloat(source + sizeof(float) * 1),
                ReadFloat(source + sizeof(float) * 2),
                ReadFloat(source + sizeof(float) * 3)
            };
        }
        return true;
    }

    Matrix4x4 MakeQuaternionRotationMatrix(const std::array<float, 4>& quaternion) {
        float x = quaternion[0];
        float y = quaternion[1];
        float z = quaternion[2];
        float w = quaternion[3];

        float length = std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
        if (length > 0.000001f) {
            float invLength = 1.0f / length;
            x *= invLength;
            y *= invLength;
            z *= invLength;
            w *= invLength;
        } else {
            x = 0.0f;
            y = 0.0f;
            z = 0.0f;
            w = 1.0f;
        }

        Matrix4x4 result = MatrixMath::MakeIdentity4x4();
        float xx = x * x;
        float yy = y * y;
        float zz = z * z;
        float xy = x * y;
        float xz = x * z;
        float yz = y * z;
        float wx = w * x;
        float wy = w * y;
        float wz = w * z;

        result.m[0][0] = 1.0f - (2.0f * (yy + zz));
        result.m[0][1] = 2.0f * (xy + wz);
        result.m[0][2] = 2.0f * (xz - wy);

        result.m[1][0] = 2.0f * (xy - wz);
        result.m[1][1] = 1.0f - (2.0f * (xx + zz));
        result.m[1][2] = 2.0f * (yz + wx);

        result.m[2][0] = 2.0f * (xz + wy);
        result.m[2][1] = 2.0f * (yz - wx);
        result.m[2][2] = 1.0f - (2.0f * (xx + yy));
        return result;
    }

    Vector3 ExtractEulerXYZ(const Matrix4x4& matrix) {
        float sinY = std::clamp(-matrix.m[0][2], -1.0f, 1.0f);
        float rotateY = std::asin(sinY);
        float cosY = std::cos(rotateY);

        float rotateX = 0.0f;
        float rotateZ = 0.0f;
        if (std::fabs(cosY) > 0.0001f) {
            rotateX = std::atan2(matrix.m[1][2], matrix.m[2][2]);
            rotateZ = std::atan2(matrix.m[0][1], matrix.m[0][0]);
        } else {
            rotateX = std::atan2(-matrix.m[2][1], matrix.m[1][1]);
            rotateZ = 0.0f;
        }

        return { rotateX, rotateY, rotateZ };
    }

    Vector3 QuaternionToEulerXYZ(const std::array<float, 4>& quaternion) {
        return ExtractEulerXYZ(MakeQuaternionRotationMatrix(quaternion));
    }

    Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    Vector3 SampleCurve(const Vec3Curve& curve, float time, const Vector3& defaultValue) {
        if (!curve.valid || curve.times.empty() || curve.values.empty()) {
            return defaultValue;
        }

        if (curve.times.size() == 1 || time <= curve.times.front()) {
            return curve.values.front();
        }
        if (time >= curve.times.back()) {
            return curve.values.back();
        }

        for (size_t keyIndex = 0; keyIndex + 1 < curve.times.size(); ++keyIndex) {
            float time0 = curve.times[keyIndex];
            float time1 = curve.times[keyIndex + 1];
            if (time < time0 || time > time1) {
                continue;
            }

            if (curve.interpolation == "STEP") {
                return curve.values[keyIndex];
            }

            float range = time1 - time0;
            float t = (range > kAnimationTimeEpsilon) ? ((time - time0) / range) : 0.0f;
            return LerpVector3(curve.values[keyIndex], curve.values[keyIndex + 1], t);
        }

        return curve.values.back();
    }

    void AddUniqueTime(std::vector<float>& times, float time) {
        for (float existingTime : times) {
            if (std::fabs(existingTime - time) <= kAnimationTimeEpsilon) {
                return;
            }
        }
        times.push_back(time);
    }
}

bool GltfAnimationLoader::LoadFirstClipFromFile(const std::string& filePath, const Skeleton& referenceSkeleton, AnimationClip& outClip) {
    std::string gltfText;
    if (!LoadFileToString(filePath, gltfText)) {
        return false;
    }

    std::vector<GltfBufferData> buffers;
    std::vector<GltfBufferViewData> bufferViews;
    std::vector<GltfAccessorData> accessors;
    std::vector<GltfNodeData> nodes;
    std::vector<GltfSkinData> skins;
    std::vector<GltfAnimationData> animations;
    JsonReader reader(gltfText);
    if (!reader.Parse(buffers, bufferViews, accessors, nodes, skins, animations) ||
        buffers.empty() || skins.empty() || animations.empty()) {
        return false;
    }

    std::vector<uint8_t> binary;
    if (!LoadBinaryFile(ResolveRelativePath(filePath, buffers.front().uri), binary)) {
        return false;
    }

    const GltfSkinData& skin = skins.front();
    const GltfAnimationData& animation = animations.front();

    if (skin.joints.empty() || referenceSkeleton.joints.empty()) {
        return false;
    }

    std::unordered_map<int, int> nodeToJointIndex;
    for (size_t jointIndex = 0; jointIndex < skin.joints.size() && jointIndex < referenceSkeleton.joints.size(); ++jointIndex) {
        nodeToJointIndex[skin.joints[jointIndex]] = static_cast<int>(jointIndex);
    }

    std::vector<TrackCurves> trackCurves(referenceSkeleton.joints.size());
    float maxTime = 0.0f;

    for (const GltfAnimationChannelData& channel : animation.channels) {
        if (channel.samplerIndex < 0 || channel.samplerIndex >= static_cast<int>(animation.samplers.size())) {
            continue;
        }

        auto foundJoint = nodeToJointIndex.find(channel.targetNode);
        if (foundJoint == nodeToJointIndex.end()) {
            continue;
        }

        const GltfAnimationSamplerData& sampler = animation.samplers[static_cast<size_t>(channel.samplerIndex)];
        std::vector<float> times;
        if (!ReadScalarFloatAccessor(binary, bufferViews, accessors, sampler.inputAccessor, times) || times.empty()) {
            continue;
        }
        maxTime = (std::max)(maxTime, times.back());

        TrackCurves& curves = trackCurves[static_cast<size_t>(foundJoint->second)];
        if (channel.targetPath == "translation") {
            std::vector<Vector3> values;
            if (!ReadVector3Accessor(binary, bufferViews, accessors, sampler.outputAccessor, values) ||
                values.size() != times.size()) {
                continue;
            }
            curves.translation.valid = true;
            curves.translation.interpolation = sampler.interpolation;
            curves.translation.times = std::move(times);
            curves.translation.values = std::move(values);
        } else if (channel.targetPath == "rotation") {
            std::vector<std::array<float, 4>> quaternions;
            if (!ReadQuaternionAccessor(binary, bufferViews, accessors, sampler.outputAccessor, quaternions) ||
                quaternions.size() != times.size()) {
                continue;
            }
            curves.rotation.valid = true;
            curves.rotation.interpolation = sampler.interpolation;
            curves.rotation.times = std::move(times);
            curves.rotation.values.resize(quaternions.size());
            for (size_t keyIndex = 0; keyIndex < quaternions.size(); ++keyIndex) {
                curves.rotation.values[keyIndex] = QuaternionToEulerXYZ(quaternions[keyIndex]);
            }
        } else if (channel.targetPath == "scale") {
            std::vector<Vector3> values;
            if (!ReadVector3Accessor(binary, bufferViews, accessors, sampler.outputAccessor, values) ||
                values.size() != times.size()) {
                continue;
            }
            curves.scale.valid = true;
            curves.scale.interpolation = sampler.interpolation;
            curves.scale.times = std::move(times);
            curves.scale.values = std::move(values);
        }
    }

    AnimationClip clip{};
    clip.name = animation.name.empty() ? std::filesystem::path(filePath).stem().string() : animation.name;
    clip.duration = (std::max)(maxTime, 0.0001f);
    clip.tracks.reserve(referenceSkeleton.joints.size());

    for (size_t jointIndex = 0; jointIndex < referenceSkeleton.joints.size(); ++jointIndex) {
        const Joint& referenceJoint = referenceSkeleton.joints[jointIndex];
        const TrackCurves& curves = trackCurves[jointIndex];

        std::vector<float> keyTimes;
        for (float time : curves.translation.times) { AddUniqueTime(keyTimes, time); }
        for (float time : curves.rotation.times) { AddUniqueTime(keyTimes, time); }
        for (float time : curves.scale.times) { AddUniqueTime(keyTimes, time); }

        JointTrack track{};
        track.jointName = referenceJoint.name;

        if (keyTimes.empty()) {
            track.keys.push_back({ 0.0f, referenceJoint.localTranslate, referenceJoint.localRotate, referenceJoint.localScale });
            track.keys.push_back({ clip.duration, referenceJoint.localTranslate, referenceJoint.localRotate, referenceJoint.localScale });
        } else {
            std::sort(keyTimes.begin(), keyTimes.end());
            for (float keyTime : keyTimes) {
                Keyframe keyframe{};
                keyframe.time = keyTime;
                keyframe.translate = SampleCurve(curves.translation, keyTime, referenceJoint.localTranslate);
                keyframe.rotate = SampleCurve(curves.rotation, keyTime, referenceJoint.localRotate);
                keyframe.scale = SampleCurve(curves.scale, keyTime, referenceJoint.localScale);
                track.keys.push_back(keyframe);
            }
            SortJointTrackKeys(track);
        }

        clip.tracks.push_back(std::move(track));
    }

    outClip = std::move(clip);
    return true;
}
