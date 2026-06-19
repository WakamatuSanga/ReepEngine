#include "GltfSkinnedModel.h"
#include "Engine/Core/DirectXCommon.h"
#include "Engine/Utility/Logger.h"
#include "Model.h"
#include "ModelCommon.h"
#include "Engine/Animation/Skeleton.h"
#include "Engine/Core/SrvManager.h"
#include "Engine/Graphics/Texture/TextureManager.h"
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
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
    constexpr size_t AlignConstantBufferSize(size_t sizeInBytes) {
        return (sizeInBytes + 0xff) & ~static_cast<size_t>(0xff);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBufferResource(
        ID3D12Device* device,
        size_t sizeInBytes) {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = sizeInBytes;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        HRESULT hr = device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(resource.GetAddressOf()));
        if (FAILED(hr)) {
            resource.Reset();
        }
        return resource;
    }

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

    struct ResolvedGltfTexture {
        std::string materialTexturePath;
        std::string resolvedTexturePath;
        bool usingWhiteFallback = false;
        bool usingUvCheckerFallback = false;
        int missingTextureCount = 0;
    };

    ResolvedGltfTexture ResolveGltfBaseColorTexture(
        const std::string& gltfPath,
        const std::vector<GltfImageData>& images) {
        constexpr const char* kWhiteFallbackTexturePath = "resources/human/white.png";
        constexpr const char* kUvCheckerFallbackTexturePath = "resources/obj/axis/uvChecker.png";

        ResolvedGltfTexture result{};
        if (!images.empty() && !images.front().uri.empty()) {
            result.materialTexturePath = images.front().uri;
            result.resolvedTexturePath = ResolveRelativePath(gltfPath, images.front().uri);
            if (std::filesystem::exists(std::filesystem::path(result.resolvedTexturePath))) {
                return result;
            }
            ++result.missingTextureCount;
        } else {
            result.materialTexturePath = "(missing)";
            ++result.missingTextureCount;
        }

        result.resolvedTexturePath = kWhiteFallbackTexturePath;
        result.usingWhiteFallback = true;
        if (!std::filesystem::exists(std::filesystem::path(result.resolvedTexturePath))) {
            result.resolvedTexturePath = kUvCheckerFallbackTexturePath;
            result.usingUvCheckerFallback = true;
        }
        return result;
    }

    void ApplyTextureToModelData(
        Model::ModelData& modelData,
        const ResolvedGltfTexture& texture,
        GltfSkinnedModel::TextureDebugInfo& textureDebugInfo) {
        TextureManager::GetInstance()->LoadTexture(texture.resolvedTexturePath);
        const uint32_t textureIndex = TextureManager::GetInstance()->GetTextureIndexByFilePath(texture.resolvedTexturePath);
        modelData.material.materialName = "gltf_base_color";
        modelData.material.textureFilePath = texture.resolvedTexturePath;
        modelData.material.baseColorTexturePath = texture.resolvedTexturePath;
        modelData.material.textureIndex = textureIndex;
        modelData.material.baseColorTextureIndex = textureIndex;
        modelData.material.usePBR = false;

        textureDebugInfo.materialTexturePath = texture.materialTexturePath;
        textureDebugInfo.resolvedTexturePath = texture.resolvedTexturePath;
        textureDebugInfo.textureIndex = textureIndex;
        textureDebugInfo.usingWhiteFallback = texture.usingWhiteFallback;
        textureDebugInfo.usingUvCheckerFallback = texture.usingUvCheckerFallback;
        textureDebugInfo.missingTextureCount = texture.missingTextureCount;
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

    void ExpandBounds(GltfSkinnedModel::Bounds& bounds, const Vector3& position) {
        if (!bounds.isValid) {
            bounds.isValid = true;
            bounds.min = position;
            bounds.max = position;
            return;
        }

        bounds.min.x = (std::min)(bounds.min.x, position.x);
        bounds.min.y = (std::min)(bounds.min.y, position.y);
        bounds.min.z = (std::min)(bounds.min.z, position.z);
        bounds.max.x = (std::max)(bounds.max.x, position.x);
        bounds.max.y = (std::max)(bounds.max.y, position.y);
        bounds.max.z = (std::max)(bounds.max.z, position.z);
    }

    void FinalizeBounds(GltfSkinnedModel::Bounds& bounds) {
        if (!bounds.isValid) {
            return;
        }

        bounds.size = {
            bounds.max.x - bounds.min.x,
            bounds.max.y - bounds.min.y,
            bounds.max.z - bounds.min.z
        };
        bounds.center = {
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f,
            (bounds.min.z + bounds.max.z) * 0.5f
        };
    }

    GltfSkinnedModel::Bounds ComputeBounds(const std::vector<Vector3>& positions) {
        GltfSkinnedModel::Bounds bounds{};
        for (const Vector3& position : positions) {
            ExpandBounds(bounds, position);
        }
        FinalizeBounds(bounds);
        return bounds;
    }

    std::string FormatVector3(const Vector3& value) {
        return
            "(" + std::to_string(value.x) +
            ", " + std::to_string(value.y) +
            ", " + std::to_string(value.z) + ")";
    }

    void LogBounds(const std::string& label, const GltfSkinnedModel::Bounds& bounds) {
        if (!bounds.isValid) {
            Logger::Log("[GltfSkinnedModel] " + label + ": invalid");
            return;
        }

        Logger::Log(
            "[GltfSkinnedModel] " + label +
            " min=" + FormatVector3(bounds.min) +
            " max=" + FormatVector3(bounds.max) +
            " size=" + FormatVector3(bounds.size) +
            " center=" + FormatVector3(bounds.center));
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
    textureDebugInfo_ = {};

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

    sourceBounds_ = ComputeBounds(positions);
    skinnedBounds_ = sourceBounds_;

    Model::ModelData modelData{};
    modelData.vertices.reserve(positions.size());
    modelData.indices = indices;
    for (uint32_t index : modelData.indices) {
        if (index >= positions.size()) {
            return false;
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < positions.size(); ++vertexIndex) {
        modelData.vertices.push_back({
            { positions[vertexIndex].x, positions[vertexIndex].y, positions[vertexIndex].z, 1.0f },
            texcoords[vertexIndex],
            normals[vertexIndex]
            });
    }

    ApplyTextureToModelData(
        modelData,
        ResolveGltfBaseColorTexture(gltfPath, images),
        textureDebugInfo_);

    model_ = std::make_unique<Model>();
    model_->Initialize(modelCommon, modelData);
    skeleton_ = nullptr;
    sourceVertices_.clear();
    inverseBindMatrices_.clear();
    jointPalette_.clear();
    LogBounds(gltfPath + " source local bounds", sourceBounds_);
    return true;
}

bool GltfSkinnedModel::Initialize(ModelCommon* modelCommon, Skeleton* skeleton, const std::string& gltfPath) {
    if (!modelCommon || !skeleton) {
        return false;
    }
    textureDebugInfo_ = {};
    if (!InitializeComputeSkinningPipeline(modelCommon)) {
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

    sourceBounds_ = ComputeBounds(positions);

    sourceVertices_.clear();
    sourceVertices_.reserve(positions.size());

    Model::ModelData modelData{};
    modelData.vertices.reserve(positions.size());
    modelData.indices = indices;
    for (uint32_t index : modelData.indices) {
        if (index >= positions.size()) {
            return false;
        }
    }

    for (size_t vertexIndex = 0; vertexIndex < positions.size(); ++vertexIndex) {
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

    ApplyTextureToModelData(
        modelData,
        ResolveGltfBaseColorTexture(gltfPath, images),
        textureDebugInfo_);

    model_ = std::make_unique<Model>();
    model_->Initialize(modelCommon, modelData);

    skeleton_ = skeleton;
    inverseBindMatrices_ = std::move(inverseBindMatrices);
    jointPalette_.resize((std::max)(inverseBindMatrices_.size(), skeleton_->joints.size()), MatrixMath::MakeIdentity4x4());
    if (!InitializeComputeSkinningResources(modelCommon)) {
        return false;
    }
    UpdateSkinning();
    LogBounds(gltfPath + " source local bounds", sourceBounds_);
    LogBounds(gltfPath + " skinned bounds", skinnedBounds_);
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
    if (matrixPaletteData_ && !jointPalette_.empty()) {
        std::memcpy(
            matrixPaletteData_,
            jointPalette_.data(),
            sizeof(Matrix4x4) * jointPalette_.size());
    }

    std::vector<Model::VertexData> skinnedVertices;
    skinnedVertices.resize(sourceVertices_.size());
    Bounds updatedSkinnedBounds{};

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
        ExpandBounds(updatedSkinnedBounds, skinnedPosition);
    }

    FinalizeBounds(updatedSkinnedBounds);
    skinnedBounds_ = updatedSkinnedBounds;
    model_->SetVertices(skinnedVertices);
}

bool GltfSkinnedModel::InitializeComputeSkinningPipeline(ModelCommon* modelCommon) {
    if (!modelCommon) {
        return false;
    }

    DirectXCommon* dxCommon = modelCommon->GetDxCommon();
    if (!dxCommon) {
        return false;
    }

    return CreateComputeRootSignature(dxCommon) && CreateComputePipelineState(dxCommon);
}

bool GltfSkinnedModel::CreateComputeRootSignature(DirectXCommon* dxCommon) {
    if (!dxCommon) {
        return false;
    }

    D3D12_DESCRIPTOR_RANGE descriptorRanges[4]{};
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].RegisterSpace = 0;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].BaseShaderRegister = 0;
    descriptorRanges[1].RegisterSpace = 0;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[2].NumDescriptors = 1;
    descriptorRanges[2].BaseShaderRegister = 1;
    descriptorRanges[2].RegisterSpace = 0;
    descriptorRanges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    descriptorRanges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[3].NumDescriptors = 1;
    descriptorRanges[3].BaseShaderRegister = 2;
    descriptorRanges[3].RegisterSpace = 0;
    descriptorRanges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[5]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].Descriptor.ShaderRegister = 0;
    rootParameters[2].Descriptor.RegisterSpace = 0;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[2];

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &descriptorRanges[3];

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumParameters = _countof(rootParameters);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        signatureBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Log(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = dxCommon->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(skinningComputeRootSignature_.ReleaseAndGetAddressOf()));
    return SUCCEEDED(hr);
}

bool GltfSkinnedModel::CreateComputePipelineState(DirectXCommon* dxCommon) {
    if (!dxCommon || !skinningComputeRootSignature_) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> computeShaderBlob =
        dxCommon->CompileShader(L"resources/shaders/Skinning.CS.hlsl", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc{};
    pipelineStateDesc.pRootSignature = skinningComputeRootSignature_.Get();
    pipelineStateDesc.CS = {
        computeShaderBlob->GetBufferPointer(),
        computeShaderBlob->GetBufferSize()
    };

    HRESULT hr = dxCommon->GetDevice()->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(skinningComputePipelineState_.ReleaseAndGetAddressOf()));
    return SUCCEEDED(hr);
}

bool GltfSkinnedModel::InitializeComputeSkinningResources(ModelCommon* modelCommon) {
    if (!modelCommon || sourceVertices_.empty()) {
        return false;
    }

    DirectXCommon* dxCommon = modelCommon->GetDxCommon();
    SrvManager* srvManager = SrvManager::GetInstance();
    if (!dxCommon || !srvManager) {
        return false;
    }

    const UINT vertexCount = static_cast<UINT>(sourceVertices_.size());
    const UINT vertexStride = sizeof(Model::VertexData);
    const size_t vertexBufferSize = static_cast<size_t>(vertexStride) * vertexCount;
    const UINT influenceStride = sizeof(VertexInfluence);
    const size_t influenceBufferSize = static_cast<size_t>(influenceStride) * vertexCount;
    const UINT paletteCount = static_cast<UINT>(jointPalette_.size());
    const UINT paletteStride = sizeof(Matrix4x4);
    const size_t paletteBufferSize = static_cast<size_t>(paletteStride) * paletteCount;
    const size_t skinningInformationBufferSize = AlignConstantBufferSize(sizeof(SkinningInformation));

    skinningInformationResource_ = dxCommon->CreateBufferResource(skinningInformationBufferSize);
    HRESULT hr = skinningInformationResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&skinningInformationData_));
    if (FAILED(hr) || !skinningInformationData_) {
        return false;
    }
    skinningInformationData_->numVertices = vertexCount;

    inputVerticesResource_ = dxCommon->CreateBufferResource(vertexBufferSize);
    std::vector<Model::VertexData> inputVertices;
    inputVertices.reserve(sourceVertices_.size());
    for (const SourceVertex& sourceVertex : sourceVertices_) {
        inputVertices.push_back({
            { sourceVertex.position.x, sourceVertex.position.y, sourceVertex.position.z, 1.0f },
            sourceVertex.texcoord,
            sourceVertex.normal
            });
    }

    Model::VertexData* mappedInputVertices = nullptr;
    hr = inputVerticesResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInputVertices));
    if (FAILED(hr) || !mappedInputVertices) {
        return false;
    }
    std::memcpy(mappedInputVertices, inputVertices.data(), vertexBufferSize);
    inputVerticesResource_->Unmap(0, nullptr);

    vertexInfluenceResource_ = dxCommon->CreateBufferResource(influenceBufferSize);
    std::vector<VertexInfluence> vertexInfluences;
    vertexInfluences.reserve(sourceVertices_.size());
    for (const SourceVertex& sourceVertex : sourceVertices_) {
        VertexInfluence influence{};
        influence.weights = sourceVertex.weights;
        influence.indices = sourceVertex.joints;
        vertexInfluences.push_back(influence);
    }

    VertexInfluence* mappedVertexInfluences = nullptr;
    hr = vertexInfluenceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexInfluences));
    if (FAILED(hr) || !mappedVertexInfluences) {
        return false;
    }
    std::memcpy(mappedVertexInfluences, vertexInfluences.data(), influenceBufferSize);
    vertexInfluenceResource_->Unmap(0, nullptr);

    matrixPaletteResource_ = dxCommon->CreateBufferResource(paletteBufferSize);
    hr = matrixPaletteResource_->Map(0, nullptr, reinterpret_cast<void**>(&matrixPaletteData_));
    if (FAILED(hr) || !matrixPaletteData_) {
        return false;
    }
    std::memcpy(matrixPaletteData_, jointPalette_.data(), paletteBufferSize);

    outputVerticesResource_ = CreateUAVBufferResource(dxCommon->GetDevice(), vertexBufferSize);
    if (!outputVerticesResource_) {
        return false;
    }
    outputVerticesVertexBufferView_.BufferLocation = outputVerticesResource_->GetGPUVirtualAddress();
    outputVerticesVertexBufferView_.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    outputVerticesVertexBufferView_.StrideInBytes = vertexStride;
    outputVerticesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    if (!srvManager->CanAllocate()) {
        return false;
    }
    skinningInformationCBVIndex_ = srvManager->Allocate();
    skinningInformationCBVHandleCPU_ = srvManager->GetCPUDescriptorHandle(skinningInformationCBVIndex_);
    skinningInformationCBVHandleGPU_ = srvManager->GetGPUDescriptorHandle(skinningInformationCBVIndex_);
    srvManager->CreateCBV(
        skinningInformationCBVIndex_,
        skinningInformationResource_.Get(),
        static_cast<UINT>(skinningInformationBufferSize));

    if (!srvManager->CanAllocate()) {
        return false;
    }
    inputVerticesSRVIndex_ = srvManager->Allocate();
    inputVerticesSRVHandleCPU_ = srvManager->GetCPUDescriptorHandle(inputVerticesSRVIndex_);
    inputVerticesSRVHandleGPU_ = srvManager->GetGPUDescriptorHandle(inputVerticesSRVIndex_);
    srvManager->CreateSRVforStructuredBuffer(
        inputVerticesSRVIndex_,
        inputVerticesResource_.Get(),
        vertexCount,
        vertexStride);

    if (!srvManager->CanAllocate()) {
        return false;
    }
    vertexInfluenceSRVIndex_ = srvManager->Allocate();
    vertexInfluenceSRVHandleCPU_ = srvManager->GetCPUDescriptorHandle(vertexInfluenceSRVIndex_);
    vertexInfluenceSRVHandleGPU_ = srvManager->GetGPUDescriptorHandle(vertexInfluenceSRVIndex_);
    srvManager->CreateSRVforStructuredBuffer(
        vertexInfluenceSRVIndex_,
        vertexInfluenceResource_.Get(),
        vertexCount,
        influenceStride);

    if (!srvManager->CanAllocate()) {
        return false;
    }
    matrixPaletteSRVIndex_ = srvManager->Allocate();
    matrixPaletteSRVHandleCPU_ = srvManager->GetCPUDescriptorHandle(matrixPaletteSRVIndex_);
    matrixPaletteSRVHandleGPU_ = srvManager->GetGPUDescriptorHandle(matrixPaletteSRVIndex_);
    srvManager->CreateSRVforStructuredBuffer(
        matrixPaletteSRVIndex_,
        matrixPaletteResource_.Get(),
        paletteCount,
        paletteStride);

    if (!srvManager->CanAllocate()) {
        return false;
    }
    outputVerticesUAVIndex_ = srvManager->Allocate();
    outputVerticesUAVHandleCPU_ = srvManager->GetCPUDescriptorHandle(outputVerticesUAVIndex_);
    outputVerticesUAVHandleGPU_ = srvManager->GetGPUDescriptorHandle(outputVerticesUAVIndex_);
    srvManager->CreateUAVforStructuredBuffer(
        outputVerticesUAVIndex_,
        outputVerticesResource_.Get(),
        vertexCount,
        vertexStride);

    return true;
}

void GltfSkinnedModel::DispatchComputeSkinning(ID3D12GraphicsCommandList* commandList) {
    if (!commandList ||
        !skinningComputeRootSignature_ ||
        !skinningComputePipelineState_ ||
        !skinningInformationResource_ ||
        !inputVerticesResource_ ||
        !vertexInfluenceResource_ ||
        !matrixPaletteResource_ ||
        !outputVerticesResource_ ||
        !skinningInformationData_ ||
        skinningInformationData_->numVertices == 0) {
        return;
    }

    if (outputVerticesState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER transitionToUAV{};
        transitionToUAV.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        transitionToUAV.Transition.pResource = outputVerticesResource_.Get();
        transitionToUAV.Transition.StateBefore = outputVerticesState_;
        transitionToUAV.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transitionToUAV.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &transitionToUAV);
        outputVerticesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        SrvManager::GetInstance()->GetSrvDescriptorHeap()
    };
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetComputeRootSignature(skinningComputeRootSignature_.Get());
    commandList->SetPipelineState(skinningComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, inputVerticesSRVHandleGPU_);
    commandList->SetComputeRootDescriptorTable(1, outputVerticesUAVHandleGPU_);
    commandList->SetComputeRootConstantBufferView(2, skinningInformationResource_->GetGPUVirtualAddress());
    commandList->SetComputeRootDescriptorTable(3, vertexInfluenceSRVHandleGPU_);
    commandList->SetComputeRootDescriptorTable(4, matrixPaletteSRVHandleGPU_);

    const UINT threadGroupCount = (skinningInformationData_->numVertices + 1023u) / 1024u;
    commandList->Dispatch(threadGroupCount, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = outputVerticesResource_.Get();
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER transitionToVertexBuffer{};
    transitionToVertexBuffer.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transitionToVertexBuffer.Transition.pResource = outputVerticesResource_.Get();
    transitionToVertexBuffer.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    transitionToVertexBuffer.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    transitionToVertexBuffer.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &transitionToVertexBuffer);
    outputVerticesState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

void GltfSkinnedModel::SetUseComputeOutputVertices(bool enabled) {
    useComputeOutputVertices_ = enabled;
    if (!model_) {
        return;
    }

    if (useComputeOutputVertices_) {
        model_->SetVertexBufferViewOverride(&outputVerticesVertexBufferView_);
    } else {
        model_->ClearVertexBufferViewOverride();
    }
}
