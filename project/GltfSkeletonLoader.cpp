#include "GltfSkeletonLoader.h"
#include "Skeleton.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    struct GltfNodeData {
        std::string name;
        std::vector<int> children;
        int parentIndex = -1;
        Vector3 translation{ 0.0f, 0.0f, 0.0f };
        std::array<float, 4> rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        Vector3 scale{ 1.0f, 1.0f, 1.0f };
    };

    struct GltfSkinData {
        std::string name;
        std::vector<int> joints;
    };

    class JsonReader {
    public:
        explicit JsonReader(const std::string& source)
            : source_(source) {
        }

        bool Parse(std::vector<GltfNodeData>& nodes, std::vector<GltfSkinData>& skins) {
            nodes.clear();
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

                if (key == "nodes") {
                    if (!ParseNodes(nodes)) {
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
                } else if (key == "children") {
                    if (!ParseIntArray(node.children)) {
                        return false;
                    }
                } else if (key == "translation") {
                    if (!ParseVector3(node.translation)) {
                        return false;
                    }
                } else if (key == "rotation") {
                    if (!ParseVector4(node.rotation)) {
                        return false;
                    }
                } else if (key == "scale") {
                    if (!ParseVector3(node.scale)) {
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

        bool ParseVector3(Vector3& value) {
            if (!Consume('[')) {
                return false;
            }

            if (!ParseFloat(value.x) || !Consume(',')) {
                return false;
            }
            if (!ParseFloat(value.y) || !Consume(',')) {
                return false;
            }
            if (!ParseFloat(value.z) || !Consume(']')) {
                return false;
            }
            return true;
        }

        bool ParseVector4(std::array<float, 4>& value) {
            if (!Consume('[')) {
                return false;
            }

            if (!ParseFloat(value[0]) || !Consume(',')) {
                return false;
            }
            if (!ParseFloat(value[1]) || !Consume(',')) {
                return false;
            }
            if (!ParseFloat(value[2]) || !Consume(',')) {
                return false;
            }
            if (!ParseFloat(value[3]) || !Consume(']')) {
                return false;
            }
            return true;
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

        bool ParseFloat(float& outValue) {
            SkipWhitespace();
            const char* begin = source_.c_str() + cursor_;
            char* end = nullptr;
            outValue = std::strtof(begin, &end);
            if (end == begin) {
                return false;
            }
            cursor_ = static_cast<size_t>(end - source_.c_str());
            return true;
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
                float ignored = 0.0f;
                return ParseFloat(ignored);
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

    Matrix4x4 MakeNodeLocalMatrix(const GltfNodeData& node) {
        Matrix4x4 scaleMatrix = MatrixMath::MakeScale(node.scale);
        Matrix4x4 rotationMatrix = MakeQuaternionRotationMatrix(node.rotation);
        Matrix4x4 translationMatrix = MatrixMath::MakeTranslate(node.translation);
        return MatrixMath::Multipty(MatrixMath::Multipty(scaleMatrix, rotationMatrix), translationMatrix);
    }

    Matrix4x4 NormalizeRotationRows(const Matrix4x4& matrix, const Vector3& scale) {
        Matrix4x4 result = MatrixMath::MakeIdentity4x4();

        float scaleX = (std::fabs(scale.x) > 0.000001f) ? scale.x : 1.0f;
        float scaleY = (std::fabs(scale.y) > 0.000001f) ? scale.y : 1.0f;
        float scaleZ = (std::fabs(scale.z) > 0.000001f) ? scale.z : 1.0f;

        result.m[0][0] = matrix.m[0][0] / scaleX;
        result.m[0][1] = matrix.m[0][1] / scaleX;
        result.m[0][2] = matrix.m[0][2] / scaleX;

        result.m[1][0] = matrix.m[1][0] / scaleY;
        result.m[1][1] = matrix.m[1][1] / scaleY;
        result.m[1][2] = matrix.m[1][2] / scaleY;

        result.m[2][0] = matrix.m[2][0] / scaleZ;
        result.m[2][1] = matrix.m[2][1] / scaleZ;
        result.m[2][2] = matrix.m[2][2] / scaleZ;
        return result;
    }

    Vector3 ExtractScale(const Matrix4x4& matrix) {
        auto RowLength = [&](int row) {
            return std::sqrt(
                (matrix.m[row][0] * matrix.m[row][0]) +
                (matrix.m[row][1] * matrix.m[row][1]) +
                (matrix.m[row][2] * matrix.m[row][2]));
        };

        return {
            RowLength(0),
            RowLength(1),
            RowLength(2)
        };
    }

    Vector3 ExtractEulerXYZ(const Matrix4x4& matrix) {
        Matrix4x4 rotationMatrix = matrix;
        float sinY = std::clamp(-rotationMatrix.m[0][2], -1.0f, 1.0f);
        float rotateY = std::asin(sinY);
        float cosY = std::cos(rotateY);

        float rotateX = 0.0f;
        float rotateZ = 0.0f;
        if (std::fabs(cosY) > 0.0001f) {
            rotateX = std::atan2(rotationMatrix.m[1][2], rotationMatrix.m[2][2]);
            rotateZ = std::atan2(rotationMatrix.m[0][1], rotationMatrix.m[0][0]);
        } else {
            rotateX = std::atan2(-rotationMatrix.m[2][1], rotationMatrix.m[1][1]);
            rotateZ = 0.0f;
        }

        return { rotateX, rotateY, rotateZ };
    }

    void DecomposeLocalMatrix(const Matrix4x4& matrix, Joint& joint) {
        joint.localTranslate = {
            matrix.m[3][0],
            matrix.m[3][1],
            matrix.m[3][2]
        };
        joint.localScale = ExtractScale(matrix);
        Matrix4x4 rotationMatrix = NormalizeRotationRows(matrix, joint.localScale);
        joint.localRotate = ExtractEulerXYZ(rotationMatrix);
    }
}

std::unique_ptr<Skeleton> GltfSkeletonLoader::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream stream{ std::filesystem::path(filePath) };
        if (!stream.is_open()) {
            return nullptr;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();

        std::vector<GltfNodeData> nodes;
        std::vector<GltfSkinData> skins;
        JsonReader reader(buffer.str());
        if (!reader.Parse(nodes, skins) || nodes.empty() || skins.empty() || skins.front().joints.empty()) {
            return nullptr;
        }

        for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            for (int childIndex : nodes[nodeIndex].children) {
                if (childIndex >= 0 && childIndex < static_cast<int>(nodes.size())) {
                    nodes[static_cast<size_t>(childIndex)].parentIndex = static_cast<int>(nodeIndex);
                }
            }
        }

        std::vector<Matrix4x4> nodeLocalMatrices(nodes.size(), MatrixMath::MakeIdentity4x4());
        std::vector<Matrix4x4> nodeWorldMatrices(nodes.size(), MatrixMath::MakeIdentity4x4());
        std::vector<bool> worldComputed(nodes.size(), false);
        for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
            nodeLocalMatrices[nodeIndex] = MakeNodeLocalMatrix(nodes[nodeIndex]);
        }

        std::function<const Matrix4x4&(int)> ComputeWorldMatrix = [&](int nodeIndex) -> const Matrix4x4& {
            size_t safeIndex = static_cast<size_t>(nodeIndex);
            if (worldComputed[safeIndex]) {
                return nodeWorldMatrices[safeIndex];
            }

            const GltfNodeData& node = nodes[safeIndex];
            if (node.parentIndex < 0 || node.parentIndex >= static_cast<int>(nodes.size())) {
                nodeWorldMatrices[safeIndex] = nodeLocalMatrices[safeIndex];
            } else {
                nodeWorldMatrices[safeIndex] = MatrixMath::Multipty(
                    nodeLocalMatrices[safeIndex],
                    ComputeWorldMatrix(node.parentIndex));
            }

            worldComputed[safeIndex] = true;
            return nodeWorldMatrices[safeIndex];
        };

        for (int jointNodeIndex : skins.front().joints) {
            if (jointNodeIndex >= 0 && jointNodeIndex < static_cast<int>(nodes.size())) {
                ComputeWorldMatrix(jointNodeIndex);
            }
        }

        auto skeleton = std::make_unique<Skeleton>();
        skeleton->name = skins.front().name.empty()
            ? std::filesystem::path(filePath).stem().string()
            : skins.front().name;
        skeleton->joints.resize(skins.front().joints.size());

        std::unordered_map<int, int> nodeToJointIndex;
        nodeToJointIndex.reserve(skins.front().joints.size());
        for (int jointIndex = 0; jointIndex < static_cast<int>(skins.front().joints.size()); ++jointIndex) {
            int nodeIndex = skins.front().joints[static_cast<size_t>(jointIndex)];
            nodeToJointIndex[nodeIndex] = jointIndex;
        }

        for (int jointIndex = 0; jointIndex < static_cast<int>(skins.front().joints.size()); ++jointIndex) {
            int nodeIndex = skins.front().joints[static_cast<size_t>(jointIndex)];
            if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
                continue;
            }

            Joint& joint = skeleton->joints[static_cast<size_t>(jointIndex)];
            const GltfNodeData& node = nodes[static_cast<size_t>(nodeIndex)];
            joint.name = node.name.empty()
                ? ("Joint_" + std::to_string(jointIndex))
                : node.name;

            int parentNodeIndex = node.parentIndex;
            int parentJointIndex = -1;
            while (parentNodeIndex >= 0) {
                auto found = nodeToJointIndex.find(parentNodeIndex);
                if (found != nodeToJointIndex.end()) {
                    parentJointIndex = found->second;
                    break;
                }
                parentNodeIndex = nodes[static_cast<size_t>(parentNodeIndex)].parentIndex;
            }
            joint.parentIndex = parentJointIndex;

            Matrix4x4 localMatrix = nodeWorldMatrices[static_cast<size_t>(nodeIndex)];
            if (parentJointIndex >= 0) {
                localMatrix = MatrixMath::Multipty(
                    nodeWorldMatrices[static_cast<size_t>(nodeIndex)],
                    MatrixMath::Inverse(nodeWorldMatrices[static_cast<size_t>(
                        skins.front().joints[static_cast<size_t>(parentJointIndex)])]));
            }
            DecomposeLocalMatrix(localMatrix, joint);
        }

        for (size_t jointIndex = 0; jointIndex < skeleton->joints.size(); ++jointIndex) {
            int parentIndex = skeleton->joints[jointIndex].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(skeleton->joints.size())) {
                skeleton->joints[static_cast<size_t>(parentIndex)].children.push_back(static_cast<int>(jointIndex));
            }
        }

        UpdateSkeletonWorldTransforms(*skeleton);
        return skeleton;
    } catch (...) {
        return nullptr;
    }
}
