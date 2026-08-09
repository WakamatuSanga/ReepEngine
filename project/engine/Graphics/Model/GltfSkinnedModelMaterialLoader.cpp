#include "GltfSkinnedModelMaterialLoader.h"
#include "Engine/Graphics/Texture/TextureManager.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <set>
#include <string_view>
#include <utility>
namespace {
    constexpr const char* kWhiteFallbackTexturePath = "resources/human/white.png";
    constexpr const char* kUvCheckerFallbackTexturePath = "resources/obj/axis/uvChecker.png";
    struct JsonValue {
        enum class Type { Null, Boolean, Number, String, Array, Object } type = Type::Null;
        bool boolean = false;
        double number = 0.0;
        std::string string;
        std::vector<JsonValue> array;
        std::vector<std::pair<std::string, JsonValue>> object;
    };
    class JsonParser {
    public:
        explicit JsonParser(const std::string& source) : source_(source) {}
        bool Parse(JsonValue& outValue, std::string& outError) {
            if (!ParseValue(outValue, 0)) {
                outError = error_;
                return false;
            }
            SkipWhitespace();
            if (position_ != source_.size()) {
                outError = "JSON末尾に解釈できない文字があります。";
                return false;
            }
            return true;
        }
    private:
        bool ParseValue(JsonValue& outValue, int depth) {
            if (depth > 128) return Fail("JSONの入れ子が深すぎます。");
            SkipWhitespace();
            if (position_ >= source_.size()) return Fail("JSON値が途中で終了しています。");
            const char token = source_[position_];
            if (token == '{') return ParseObject(outValue, depth + 1);
            if (token == '[') return ParseArray(outValue, depth + 1);
            if (token == '"') {
                outValue.type = JsonValue::Type::String;
                return ParseString(outValue.string);
            }
            if (token == '-' || (token >= '0' && token <= '9')) {
                outValue.type = JsonValue::Type::Number;
                return ParseNumber(outValue.number);
            }
            if (Match("true")) {
                outValue.type = JsonValue::Type::Boolean;
                outValue.boolean = true;
                return true;
            }
            if (Match("false")) {
                outValue.type = JsonValue::Type::Boolean;
                outValue.boolean = false;
                return true;
            }
            if (Match("null")) {
                outValue.type = JsonValue::Type::Null;
                return true;
            }
            return Fail("JSON値を解釈できません。");
        }
        bool ParseObject(JsonValue& outValue, int depth) {
            ++position_;
            outValue.type = JsonValue::Type::Object;
            SkipWhitespace();
            if (Consume('}')) return true;
            while (true) {
                SkipWhitespace();
                std::string key;
                if (!ParseString(key)) return false;
                for (const auto& member : outValue.object) {
                    if (member.first == key) return Fail("JSONオブジェクトに重複キーがあります: " + key);
                }
                SkipWhitespace();
                if (!Consume(':')) return Fail("JSONオブジェクトの':'がありません。");
                JsonValue value;
                if (!ParseValue(value, depth)) return false;
                outValue.object.emplace_back(std::move(key), std::move(value));
                SkipWhitespace();
                if (Consume('}')) return true;
                if (!Consume(',')) return Fail("JSONオブジェクトの','または'}'がありません。");
            }
        }
        bool ParseArray(JsonValue& outValue, int depth) {
            ++position_;
            outValue.type = JsonValue::Type::Array;
            SkipWhitespace();
            if (Consume(']')) return true;
            while (true) {
                JsonValue value;
                if (!ParseValue(value, depth)) return false;
                outValue.array.push_back(std::move(value));
                SkipWhitespace();
                if (Consume(']')) return true;
                if (!Consume(',')) return Fail("JSON配列の','または']'がありません。");
            }
        }
        bool ParseString(std::string& outString) {
            if (!Consume('"')) return Fail("JSON文字列の開始引用符がありません。");
            while (position_ < source_.size()) {
                const unsigned char value = static_cast<unsigned char>(source_[position_++]);
                if (value == '"') return true;
                if (value < 0x20) return Fail("JSON文字列に制御文字があります。");
                if (value != '\\') {
                    outString.push_back(static_cast<char>(value));
                    continue;
                }
                if (position_ >= source_.size()) return Fail("JSON文字列のエスケープが途中です。");
                const char escaped = source_[position_++];
                switch (escaped) {
                case '"': outString.push_back('"'); break;
                case '\\': outString.push_back('\\'); break;
                case '/': outString.push_back('/'); break;
                case 'b': outString.push_back('\b'); break;
                case 'f': outString.push_back('\f'); break;
                case 'n': outString.push_back('\n'); break;
                case 'r': outString.push_back('\r'); break;
                case 't': outString.push_back('\t'); break;
                case 'u': {
                    std::uint32_t codePoint = 0;
                    if (!ParseHex4(codePoint)) return false;
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        if (position_ + 2 > source_.size() || source_[position_] != '\\' || source_[position_ + 1] != 'u') return Fail("JSON文字列のサロゲートペアが不正です。");
                        position_ += 2;
                        std::uint32_t low = 0;
                        if (!ParseHex4(low) || low < 0xDC00 || low > 0xDFFF) return Fail("JSON文字列の下位サロゲートが不正です。");
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
                        return Fail("JSON文字列に単独の下位サロゲートがあります。");
                    }
                    AppendUtf8(codePoint, outString);
                    break;
                }
                default:
                    return Fail("JSON文字列に未対応のエスケープがあります。");
                }
            }
            return Fail("JSON文字列が閉じられていません。");
        }
        bool ParseHex4(std::uint32_t& outValue) {
            if (position_ + 4 > source_.size()) return Fail("JSONのUnicodeエスケープが途中です。");
            outValue = 0;
            for (int index = 0; index < 4; ++index) {
                const char value = source_[position_++];
                outValue <<= 4;
                if (value >= '0' && value <= '9') outValue += value - '0';
                else if (value >= 'a' && value <= 'f') outValue += value - 'a' + 10;
                else if (value >= 'A' && value <= 'F') outValue += value - 'A' + 10;
                else return Fail("JSONのUnicodeエスケープが不正です。");
            }
            return true;
        }
        static void AppendUtf8(std::uint32_t value, std::string& output) {
            if (value <= 0x7F) output.push_back(static_cast<char>(value));
            else if (value <= 0x7FF) {
                output.push_back(static_cast<char>(0xC0 | (value >> 6)));
                output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            } else if (value <= 0xFFFF) {
                output.push_back(static_cast<char>(0xE0 | (value >> 12)));
                output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            } else {
                output.push_back(static_cast<char>(0xF0 | (value >> 18)));
                output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
                output.push_back(static_cast<char>(0x80 | (value & 0x3F)));
            }
        }
        bool ParseNumber(double& outNumber) {
            const std::size_t start = position_;
            if (source_[position_] == '-') ++position_;
            if (position_ >= source_.size()) return Fail("JSON数値が途中です。");
            if (source_[position_] == '0') ++position_;
            else if (source_[position_] >= '1' && source_[position_] <= '9') {
                while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
            } else return Fail("JSON数値の整数部が不正です。");
            if (position_ < source_.size() && source_[position_] == '.') {
                ++position_;
                const std::size_t fractionStart = position_;
                while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
                if (fractionStart == position_) return Fail("JSON数値の小数部が不正です。");
            }
            if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
                ++position_;
                if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) ++position_;
                const std::size_t exponentStart = position_;
                while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9') ++position_;
                if (exponentStart == position_) return Fail("JSON数値の指数部が不正です。");
            }
            errno = 0;
            char* end = nullptr;
            outNumber = std::strtod(source_.c_str() + start, &end);
            if (errno == ERANGE || end != source_.c_str() + position_ || !std::isfinite(outNumber)) return Fail("JSON数値が有限値として解釈できません。");
            return true;
        }
        bool Match(std::string_view text) {
            if (source_.compare(position_, text.size(), text.data(), text.size()) != 0) return false;
            position_ += text.size();
            return true;
        }
        bool Consume(char value) {
            if (position_ >= source_.size() || source_[position_] != value) return false;
            ++position_;
            return true;
        }
        void SkipWhitespace() {
            while (position_ < source_.size()) {
                const char value = source_[position_];
                if (value != ' ' && value != '\t' && value != '\r' && value != '\n') break;
                ++position_;
            }
        }
        bool Fail(const std::string& message) {
            if (error_.empty()) error_ = message + " (offset=" + std::to_string(position_) + ")";
            return false;
        }
        const std::string& source_;
        std::size_t position_ = 0;
        std::string error_;
    };
    const JsonValue* Find(const JsonValue& object, const char* key) {
        if (object.type != JsonValue::Type::Object) return nullptr;
        for (const auto& member : object.object) {
            if (member.first == key) return &member.second;
        }
        return nullptr;
    }
    bool ReadString(const JsonValue& object, const char* key, std::string& value, std::string& error) {
        const JsonValue* member = Find(object, key);
        if (!member) return true;
        if (member->type != JsonValue::Type::String) {
            error = std::string(key) + " は文字列である必要があります。";
            return false;
        }
        value = member->string;
        return true;
    }
    bool ReadBool(const JsonValue& object, const char* key, bool& value, std::string& error) {
        const JsonValue* member = Find(object, key);
        if (!member) return true;
        if (member->type != JsonValue::Type::Boolean) {
            error = std::string(key) + " はbool値である必要があります。";
            return false;
        }
        value = member->boolean;
        return true;
    }
    bool ReadFloat(
        const JsonValue& object, const char* key, float minimum, float maximum,
        float& value, std::string& error) {
        const JsonValue* member = Find(object, key);
        if (!member) return true;
        if (member->type != JsonValue::Type::Number || !std::isfinite(member->number) ||
            member->number < minimum || member->number > maximum) {
            error = std::string(key) + " は有効範囲内の有限数である必要があります。";
            return false;
        }
        value = static_cast<float>(member->number);
        return std::isfinite(value);
    }
    bool ReadIndex(const JsonValue& object, const char* key, int& value, bool required, std::string& error) {
        const JsonValue* member = Find(object, key);
        if (!member) {
            if (required) error = std::string(key) + " がありません。";
            return !required;
        }
        if (member->type != JsonValue::Type::Number || !std::isfinite(member->number) ||
            member->number < 0.0 || member->number > static_cast<double>((std::numeric_limits<int>::max)()) ||
            std::floor(member->number) != member->number) {
            error = std::string(key) + " は0以上の整数である必要があります。";
            return false;
        }
        value = static_cast<int>(member->number);
        return true;
    }
    bool ReadFactorArray(
        const JsonValue& object, const char* key, std::size_t expectedCount,
        float minimum, float maximum, float* output, std::string& error) {
        const JsonValue* member = Find(object, key);
        if (!member) return true;
        if (member->type != JsonValue::Type::Array || member->array.size() != expectedCount) {
            error = std::string(key) + " は要素数" + std::to_string(expectedCount) + "の配列である必要があります。";
            return false;
        }
        for (std::size_t index = 0; index < expectedCount; ++index) {
            const JsonValue& component = member->array[index];
            if (component.type != JsonValue::Type::Number || !std::isfinite(component.number) ||
                component.number < minimum || component.number > maximum) {
                error = std::string(key) + " の各要素は有効範囲内の有限数である必要があります。";
                return false;
            }
            output[index] = static_cast<float>(component.number);
            if (!std::isfinite(output[index])) {
                error = std::string(key) + " をfloatへ変換できません。";
                return false;
            }
        }
        return true;
    }
    struct SourceTexture { int source = -1; };
    struct SourceImage { std::string uri; };
    struct SourceDocument {
        std::vector<GltfSkinnedMaterialData> materials;
        std::vector<SourceTexture> textures;
        std::vector<SourceImage> images;
    };
    bool ParseMaterial(const JsonValue& value, int index, GltfSkinnedMaterialData& material, std::string& error) {
        if (value.type != JsonValue::Type::Object) {
            error = "materials[" + std::to_string(index) + "] はobjectである必要があります。";
            return false;
        }
        material.sourceMaterialIndex = index;
        if (!ReadString(value, "name", material.name, error) ||
            !ReadBool(value, "doubleSided", material.doubleSided, error) ||
            !ReadFloat(value, "alphaCutoff", 0.0f, (std::numeric_limits<float>::max)(), material.alphaCutoff, error)) {
            return false;
        }
        std::string alphaMode = "OPAQUE";
        if (!ReadString(value, "alphaMode", alphaMode, error)) return false;
        if (alphaMode == "OPAQUE") material.alphaMode = GltfSkinnedAlphaMode::Opaque;
        else if (alphaMode == "MASK") material.alphaMode = GltfSkinnedAlphaMode::Mask;
        else if (alphaMode == "BLEND") {
            error = "materials[" + std::to_string(index) + "].alphaMode=BLEND はマテリアル別PSOが未対応のため読み込めません。";
            return false;
        } else {
            error = "materials[" + std::to_string(index) + "].alphaMode がglTF仕様外です。";
            return false;
        }
        float emissive[3] = { material.emissiveFactor.x, material.emissiveFactor.y, material.emissiveFactor.z };
        if (!ReadFactorArray(value, "emissiveFactor", 3, 0.0f, (std::numeric_limits<float>::max)(), emissive, error)) return false;
        material.emissiveFactor = { emissive[0], emissive[1], emissive[2] };
        const JsonValue* pbr = Find(value, "pbrMetallicRoughness");
        if (!pbr) return true;
        if (pbr->type != JsonValue::Type::Object) {
            error = "pbrMetallicRoughness はobjectである必要があります。";
            return false;
        }
        float color[4] = { material.baseColorFactor.x, material.baseColorFactor.y, material.baseColorFactor.z, material.baseColorFactor.w };
        if (!ReadFactorArray(*pbr, "baseColorFactor", 4, 0.0f, 1.0f, color, error) ||
            !ReadFloat(*pbr, "metallicFactor", 0.0f, 1.0f, material.metallicFactor, error) ||
            !ReadFloat(*pbr, "roughnessFactor", 0.0f, 1.0f, material.roughnessFactor, error)) {
            return false;
        }
        material.baseColorFactor = { color[0], color[1], color[2], color[3] };
        const JsonValue* texture = Find(*pbr, "baseColorTexture");
        if (!texture) return true;
        if (texture->type != JsonValue::Type::Object) {
            error = "baseColorTexture はobjectである必要があります。";
            return false;
        }
        return ReadIndex(*texture, "index", material.baseColorTextureIndex, true, error);
    }
    bool ValidateUri(const std::string& uri, std::string& error) {
        if (uri.empty()) {
            error = "image.uri が空です。";
            return false;
        }
        for (const unsigned char value : uri) {
            if (value == 0 || value < 0x20) {
                error = "image.uri に制御文字があります。";
                return false;
            }
        }
        if (uri.rfind("data:", 0) == 0 || uri.find('?') != std::string::npos || uri.find('#') != std::string::npos) {
            error = "data URI・query・fragment付きimage URIには対応していません: " + uri;
            return false;
        }
        try {
            if (std::filesystem::path(uri).is_absolute()) {
                error = "image.uri はglTFからの相対URIである必要があります: " + uri;
                return false;
            }
        } catch (const std::filesystem::filesystem_error&) {
            error = "image.uri をfilesystem pathへ変換できません: " + uri;
            return false;
        }
        return true;
    }
    bool ParseSourceDocument(const JsonValue& root, SourceDocument& document, std::string& error) {
        if (root.type != JsonValue::Type::Object) {
            error = "glTF JSONのルートはobjectである必要があります。";
            return false;
        }
        const JsonValue* materials = Find(root, "materials");
        if (materials && materials->type != JsonValue::Type::Array) {
            error = "materials は配列である必要があります。";
            return false;
        }
        if (materials) {
            document.materials.resize(materials->array.size());
            for (std::size_t index = 0; index < materials->array.size(); ++index) {
                if (!ParseMaterial(materials->array[index], static_cast<int>(index), document.materials[index], error)) return false;
            }
        }
        const JsonValue* textures = Find(root, "textures");
        if (textures && textures->type != JsonValue::Type::Array) {
            error = "textures は配列である必要があります。";
            return false;
        }
        if (textures) {
            document.textures.resize(textures->array.size());
            for (std::size_t index = 0; index < textures->array.size(); ++index) {
                if (textures->array[index].type != JsonValue::Type::Object ||
                    !ReadIndex(textures->array[index], "source", document.textures[index].source, true, error)) {
                    error = "textures[" + std::to_string(index) + "]: " + error;
                    return false;
                }
            }
        }
        const JsonValue* images = Find(root, "images");
        if (images && images->type != JsonValue::Type::Array) {
            error = "images は配列である必要があります。";
            return false;
        }
        if (images) {
            document.images.resize(images->array.size());
            for (std::size_t index = 0; index < images->array.size(); ++index) {
                if (images->array[index].type != JsonValue::Type::Object ||
                    !ReadString(images->array[index], "uri", document.images[index].uri, error) ||
                    !Find(images->array[index], "uri") || !ValidateUri(document.images[index].uri, error)) {
                    error = "images[" + std::to_string(index) + "]: " + error;
                    return false;
                }
            }
        }
        for (std::size_t index = 0; index < document.textures.size(); ++index) {
            if (document.textures[index].source < 0 || static_cast<std::size_t>(document.textures[index].source) >= document.images.size()) {
                error = "textures[" + std::to_string(index) + "].source がimages範囲外です。";
                return false;
            }
        }
        for (std::size_t index = 0; index < document.materials.size(); ++index) {
            const int textureIndex = document.materials[index].baseColorTextureIndex;
            if (textureIndex >= 0 && static_cast<std::size_t>(textureIndex) >= document.textures.size()) {
                error = "materials[" + std::to_string(index) + "].baseColorTexture.index がtextures範囲外です。";
                return false;
            }
        }
        return true;
    }
    GltfSkinnedMaterialData MakeDefaultMaterial() {
        GltfSkinnedMaterialData material{};
        material.name = "glTF Default Material";
        material.isDefaultMaterial = true;
        return material;
    }
    std::string NormalizePath(const std::filesystem::path& path) {
        return path.lexically_normal().generic_string();
    }
    bool IsRegularFile(const std::string& path) {
        std::error_code error;
        return std::filesystem::is_regular_file(std::filesystem::path(path), error) && !error;
    }
    bool ChooseFallbackTexture(std::string& path, std::string& error) {
        path = NormalizePath(std::filesystem::path(kWhiteFallbackTexturePath));
        if (IsRegularFile(path)) return true;
        path = NormalizePath(std::filesystem::path(kUvCheckerFallbackTexturePath));
        if (IsRegularFile(path)) return true;
        error = "baseColorTexture用のwhite.pngとuvChecker.pngの両fallbackが見つかりません。";
        return false;
    }
}

std::string ResolveGltfSkinnedRelativePath(
    const std::string& baseFilePath,
    const std::string& relativePath) {
    return (std::filesystem::path(baseFilePath).parent_path() /
        std::filesystem::path(relativePath)).lexically_normal().generic_string();
}

bool LoadGltfSkinnedModelMaterials(
    const std::string& gltfPath,
    const std::string& gltfJson,
    const std::vector<SkinnedPrimitiveRange>& primitiveRanges,
    GltfSkinnedMaterialState& outState) {
    outState = {};
    GltfSkinnedMaterialDiagnostics& diagnostics = outState.diagnostics;
    diagnostics.sourcePath = NormalizePath(std::filesystem::path(gltfPath));
    diagnostics.primitiveCount = primitiveRanges.size();
    JsonValue root;
    std::string error;
    JsonParser parser(gltfJson);
    auto fail = [&](const std::string& message) {
        outState.materials.clear();
        outState.defaultMaterialRuntimeIndex = -1;
        diagnostics.loadedMaterialCount = 0;
        diagnostics.validMaterialCount = 0;
        diagnostics.invalidMaterialCount = diagnostics.sourceMaterialCount;
        diagnostics.loadSucceeded = false;
        diagnostics.errorMessage = message;
        return false;
    };
    if (!parser.Parse(root, error)) return fail("glTF material JSON構文エラー: " + error);
    if (root.type == JsonValue::Type::Object) {
        const JsonValue* materials = Find(root, "materials");
        const JsonValue* textures = Find(root, "textures");
        if (materials && materials->type == JsonValue::Type::Array) diagnostics.sourceMaterialCount = materials->array.size();
        if (textures && textures->type == JsonValue::Type::Array) diagnostics.sourceTextureCount = textures->array.size();
    }
    SourceDocument document;
    if (!ParseSourceDocument(root, document, error)) return fail("glTF material解析エラー: " + error);
    bool needsDefaultMaterial = document.materials.empty();
    for (std::size_t index = 0; index < primitiveRanges.size(); ++index) {
        const int materialIndex = primitiveRanges[index].materialIndex;
        if (materialIndex < -1) return fail("Primitive " + std::to_string(index) + " のmaterial indexが-1未満です。");
        if (materialIndex == -1) needsDefaultMaterial = true;
        else if (static_cast<std::size_t>(materialIndex) >= document.materials.size()) {
            return fail("Primitive " + std::to_string(index) + " のmaterial indexがmaterials範囲外です。");
        }
    }
    if (needsDefaultMaterial) {
        outState.defaultMaterialRuntimeIndex = static_cast<int>(document.materials.size());
        document.materials.push_back(MakeDefaultMaterial());
    }
    std::string fallbackPath;
    for (GltfSkinnedMaterialData& material : document.materials) {
        if (material.baseColorTextureIndex >= 0) {
            const SourceTexture& texture = document.textures[static_cast<std::size_t>(material.baseColorTextureIndex)];
            material.baseColorImageIndex = texture.source;
            material.baseColorTextureUri = document.images[static_cast<std::size_t>(texture.source)].uri;
            try {
                material.resolvedTexturePath = ResolveGltfSkinnedRelativePath(
                    gltfPath,
                    material.baseColorTextureUri);
            } catch (const std::filesystem::filesystem_error&) {
                return fail("baseColorTexture URIをfilesystem pathへ解決できません: " + material.baseColorTextureUri);
            }
        }
        if (material.resolvedTexturePath.empty() || !IsRegularFile(material.resolvedTexturePath)) {
            if (fallbackPath.empty() && !ChooseFallbackTexture(fallbackPath, error)) return fail(error);
            material.resolvedTexturePath = fallbackPath;
            material.usingFallbackTexture = true;
            material.usingWhiteFallbackTexture = fallbackPath ==
                NormalizePath(std::filesystem::path(kWhiteFallbackTexturePath));
            material.usingUvCheckerFallbackTexture =
                !material.usingWhiteFallbackTexture;
            ++diagnostics.fallbackTextureCount;
        }
        material.textureResolved = !material.usingFallbackTexture;
    }
    std::set<std::string> uniqueTexturePaths;
    TextureManager* textureManager = TextureManager::GetInstance();
    for (GltfSkinnedMaterialData& material : document.materials) {
        if (uniqueTexturePaths.insert(material.resolvedTexturePath).second) {
            textureManager->LoadTexture(material.resolvedTexturePath);
        }
        material.textureHandle = textureManager->GetTextureIndexByFilePath(material.resolvedTexturePath);
        material.textureHandleValid = true;
        material.valid = true;
        diagnostics.hasShaderUnusedMaterialItems = true;
        GltfSkinnedMaterialRuntime runtime{};
        runtime.data = material;
        outState.materials.push_back(std::move(runtime));
    }
    diagnostics.loadedMaterialCount = outState.materials.size();
    diagnostics.validMaterialCount = outState.materials.size();
    diagnostics.invalidMaterialCount = 0;
    diagnostics.uniqueTextureResourceCount = uniqueTexturePaths.size();
    diagnostics.multiPrimitiveSupported = true;
    diagnostics.multiMaterialSupported = true;
    diagnostics.multiMeshSupported = false;
    diagnostics.loadSucceeded = true;
    diagnostics.materials.reserve(outState.materials.size());
    for (const GltfSkinnedMaterialRuntime& runtime : outState.materials) diagnostics.materials.push_back(runtime.data);
    return true;
}
bool ApplyPrimaryGltfSkinnedMaterial(
    const GltfSkinnedMaterialState& state,
    Model::ModelData& modelData) {
    if (state.materials.empty()) return false;
    const GltfSkinnedMaterialData& material = state.materials.front().data;
    if (!material.valid || !material.textureHandleValid || material.resolvedTexturePath.empty()) return false;
    Model::MaterialData& destination = modelData.material;
    destination.materialName = material.name;
    destination.textureFilePath = material.resolvedTexturePath;
    destination.baseColorTexturePath = material.resolvedTexturePath;
    destination.textureIndex = material.textureHandle;
    destination.baseColorTextureIndex = material.textureHandle;
    destination.normalTexturePath.clear();
    destination.metallicRoughnessTexturePath.clear();
    destination.specularF0TexturePath.clear();
    destination.normalTextureIndex = 0;
    destination.metallicRoughnessTextureIndex = 0;
    destination.specularF0TextureIndex = 0;
    destination.usePBR = false;
    destination.hasNormalMap = false;
    destination.hasMetallicRoughnessMap = false;
    destination.hasSpecularF0Map = false;
    destination.metallicFactor = material.metallicFactor;
    destination.roughnessFactor = material.roughnessFactor;
    destination.normalScale = 1.0f;
    return true;
}
