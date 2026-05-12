#include "AnimationClip.h"
#include "Skeleton.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#if __has_include(<assimp/Importer.hpp>) && __has_include(<assimp/scene.h>) && __has_include(<assimp/postprocess.h>)
#define DIRECTXGAME_HAS_ASSIMP 1
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#else
#define DIRECTXGAME_HAS_ASSIMP 0
#endif

namespace {
    Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    Quaternion NormalizeQuaternion(const Quaternion& q) {
        float length = std::sqrt((q.x * q.x) + (q.y * q.y) + (q.z * q.z) + (q.w * q.w));
        if (length <= 0.000001f) {
            return {};
        }

        float invLength = 1.0f / length;
        return {
            q.x * invLength,
            q.y * invLength,
            q.z * invLength,
            q.w * invLength
        };
    }

    float DotQuaternion(const Quaternion& a, const Quaternion& b) {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z) + (a.w * b.w);
    }

    Quaternion SlerpQuaternion(const Quaternion& a, const Quaternion& b, float t) {
        Quaternion q0 = NormalizeQuaternion(a);
        Quaternion q1 = NormalizeQuaternion(b);
        float dot = DotQuaternion(q0, q1);

        if (dot < 0.0f) {
            q1.x = -q1.x;
            q1.y = -q1.y;
            q1.z = -q1.z;
            q1.w = -q1.w;
            dot = -dot;
        }

        if (dot > 0.9995f) {
            return NormalizeQuaternion({
                q0.x + ((q1.x - q0.x) * t),
                q0.y + ((q1.y - q0.y) * t),
                q0.z + ((q1.z - q0.z) * t),
                q0.w + ((q1.w - q0.w) * t)
                });
        }

        dot = std::clamp(dot, -1.0f, 1.0f);
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        if (std::fabs(sinTheta) <= 0.000001f) {
            return q0;
        }

        float weight0 = std::sin((1.0f - t) * theta) / sinTheta;
        float weight1 = std::sin(t * theta) / sinTheta;
        return NormalizeQuaternion({
            (q0.x * weight0) + (q1.x * weight1),
            (q0.y * weight0) + (q1.y * weight1),
            (q0.z * weight0) + (q1.z * weight1),
            (q0.w * weight0) + (q1.w * weight1)
            });
    }

    Matrix4x4 MakeQuaternionRotationMatrix(const Quaternion& q) {
        Quaternion quaternion = NormalizeQuaternion(q);
        float x = quaternion.x;
        float y = quaternion.y;
        float z = quaternion.z;
        float w = quaternion.w;

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

    Vector3 QuaternionToEulerXYZ(const Quaternion& quaternion) {
        Matrix4x4 matrix = MakeQuaternionRotationMatrix(quaternion);
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

    void AddUniqueKeyTime(std::vector<float>& times, float time) {
        for (float existingTime : times) {
            if (std::fabs(existingTime - time) <= 0.0001f) {
                return;
            }
        }
        times.push_back(time);
    }

    Keyframe SampleTrack(const JointTrack& track, float time) {
        const bool hasSeparateCurves =
            !track.translate.keyframes.empty() ||
            !track.rotate.keyframes.empty() ||
            !track.scale.keyframes.empty();
        if (hasSeparateCurves) {
            Keyframe sampled = track.keys.empty() ? Keyframe{} : track.keys.front();
            sampled.time = time;
            if (!track.translate.keyframes.empty()) {
                sampled.translate = CalculateValue(track.translate.keyframes, time);
            }
            if (!track.rotate.keyframes.empty()) {
                sampled.rotate = QuaternionToEulerXYZ(CalculateValue(track.rotate.keyframes, time));
            }
            if (!track.scale.keyframes.empty()) {
                sampled.scale = CalculateValue(track.scale.keyframes, time);
            }
            return sampled;
        }

        if (track.keys.empty()) {
            return {};
        }
        if (track.keys.size() == 1 || time <= track.keys.front().time) {
            return track.keys.front();
        }
        if (time >= track.keys.back().time) {
            return track.keys.back();
        }

        for (size_t keyIndex = 0; keyIndex + 1 < track.keys.size(); ++keyIndex) {
            const Keyframe& key0 = track.keys[keyIndex];
            const Keyframe& key1 = track.keys[keyIndex + 1];
            if (time < key0.time || time > key1.time) {
                continue;
            }

            float range = key1.time - key0.time;
            float t = (range > 0.0001f) ? ((time - key0.time) / range) : 0.0f;
            Keyframe sampled = key0;
            sampled.time = time;
            sampled.translate = LerpVector3(key0.translate, key1.translate, t);
            sampled.rotate = LerpVector3(key0.rotate, key1.rotate, t);
            sampled.scale = LerpVector3(key0.scale, key1.scale, t);
            return sampled;
        }

        return track.keys.back();
    }

    class JsonReader {
    public:
        explicit JsonReader(const std::string& source)
            : source_(source) {
        }

        bool ParseAnimationClip(AnimationClip& clip) {
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

                if (key == "name") {
                    if (!ParseString(clip.name)) {
                        return false;
                    }
                } else if (key == "duration") {
                    if (!ParseFloat(clip.duration)) {
                        return false;
                    }
                } else if (key == "tracks") {
                    if (!ParseTracks(clip.tracks)) {
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
        bool ParseTracks(std::vector<JointTrack>& tracks) {
            tracks.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                JointTrack track;
                if (!ParseTrack(track)) {
                    return false;
                }
                tracks.push_back(track);

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseTrack(JointTrack& track) {
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

                if (key == "joint") {
                    if (!ParseString(track.jointName)) {
                        return false;
                    }
                } else if (key == "keys") {
                    if (!ParseKeys(track.keys)) {
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

        bool ParseKeys(std::vector<Keyframe>& keys) {
            keys.clear();
            if (!Consume('[')) {
                return false;
            }

            while (true) {
                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }

                Keyframe keyframe;
                if (!ParseKeyframe(keyframe)) {
                    return false;
                }
                keys.push_back(keyframe);

                SkipWhitespace();
                if (Consume(']')) {
                    return true;
                }
                if (!Consume(',')) {
                    return false;
                }
            }
        }

        bool ParseKeyframe(Keyframe& keyframe) {
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

                if (key == "time") {
                    if (!ParseFloat(keyframe.time)) {
                        return false;
                    }
                } else if (key == "translate") {
                    if (!ParseVector3(keyframe.translate)) {
                        return false;
                    }
                } else if (key == "rotate") {
                    if (!ParseVector3(keyframe.rotate)) {
                        return false;
                    }
                } else if (key == "scale") {
                    if (!ParseVector3(keyframe.scale)) {
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
}

JointTrack* FindJointTrack(AnimationClip& clip, const std::string& jointName) {
    for (JointTrack& track : clip.tracks) {
        if (track.jointName == jointName) {
            return &track;
        }
    }
    return nullptr;
}

const JointTrack* FindJointTrack(const AnimationClip& clip, const std::string& jointName) {
    for (const JointTrack& track : clip.tracks) {
        if (track.jointName == jointName) {
            return &track;
        }
    }
    return nullptr;
}

void SortJointTrackKeys(JointTrack& track) {
    std::sort(track.keys.begin(), track.keys.end(), [](const Keyframe& a, const Keyframe& b) {
        return a.time < b.time;
    });
    std::sort(track.translate.keyframes.begin(), track.translate.keyframes.end(), [](const KeyframeVector3& a, const KeyframeVector3& b) {
        return a.time < b.time;
    });
    std::sort(track.rotate.keyframes.begin(), track.rotate.keyframes.end(), [](const KeyframeQuaternion& a, const KeyframeQuaternion& b) {
        return a.time < b.time;
    });
    std::sort(track.scale.keyframes.begin(), track.scale.keyframes.end(), [](const KeyframeVector3& a, const KeyframeVector3& b) {
        return a.time < b.time;
    });
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
    if (keyframes.empty()) {
        return {};
    }
    if (keyframes.size() == 1 || time <= keyframes.front().time) {
        return keyframes.front().value;
    }
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    for (size_t keyIndex = 0; keyIndex + 1 < keyframes.size(); ++keyIndex) {
        const KeyframeVector3& key0 = keyframes[keyIndex];
        const KeyframeVector3& key1 = keyframes[keyIndex + 1];
        if (time < key0.time || time > key1.time) {
            continue;
        }

        float range = key1.time - key0.time;
        float t = (range > 0.0001f) ? ((time - key0.time) / range) : 0.0f;
        return LerpVector3(key0.value, key1.value, t);
    }

    return keyframes.back().value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
    if (keyframes.empty()) {
        return {};
    }
    if (keyframes.size() == 1 || time <= keyframes.front().time) {
        return keyframes.front().value;
    }
    if (time >= keyframes.back().time) {
        return keyframes.back().value;
    }

    for (size_t keyIndex = 0; keyIndex + 1 < keyframes.size(); ++keyIndex) {
        const KeyframeQuaternion& key0 = keyframes[keyIndex];
        const KeyframeQuaternion& key1 = keyframes[keyIndex + 1];
        if (time < key0.time || time > key1.time) {
            continue;
        }

        float range = key1.time - key0.time;
        float t = (range > 0.0001f) ? ((time - key0.time) / range) : 0.0f;
        return SlerpQuaternion(key0.value, key1.value, t);
    }

    return keyframes.back().value;
}

Vector3 ConvertQuaternionToEulerXYZ(const Quaternion& quaternion) {
    Quaternion q = NormalizeQuaternion(quaternion);
    float x = q.x;
    float y = q.y;
    float z = q.z;
    float w = q.w;

    Matrix4x4 matrix = MatrixMath::MakeIdentity4x4();
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    matrix.m[0][0] = 1.0f - (2.0f * (yy + zz));
    matrix.m[0][1] = 2.0f * (xy + wz);
    matrix.m[0][2] = 2.0f * (xz - wy);
    matrix.m[1][0] = 2.0f * (xy - wz);
    matrix.m[1][1] = 1.0f - (2.0f * (xx + zz));
    matrix.m[1][2] = 2.0f * (yz + wx);
    matrix.m[2][0] = 2.0f * (xz + wy);
    matrix.m[2][1] = 2.0f * (yz - wx);
    matrix.m[2][2] = 1.0f - (2.0f * (xx + yy));

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

AnimationClip LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
    std::filesystem::path filePath = std::filesystem::path(directoryPath) / filename;
    AnimationClip clip{};
    clip.name = filePath.stem().string();
    clip.duration = 0.0001f;

#if DIRECTXGAME_HAS_ASSIMP
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filePath.string().c_str(), 0);
    if (!scene || scene->mNumAnimations == 0 || !scene->mAnimations[0]) {
        return clip;
    }

    const aiAnimation* animation = scene->mAnimations[0];
    double ticksPerSecond = animation->mTicksPerSecond != 0.0 ? animation->mTicksPerSecond : 1.0;
    clip.name = animation->mName.length > 0 ? animation->mName.C_Str() : clip.name;
    clip.duration = (std::max)(static_cast<float>(animation->mDuration / ticksPerSecond), 0.0001f);
    clip.tracks.reserve(animation->mNumChannels);

    for (uint32_t channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex) {
        const aiNodeAnim* nodeAnimation = animation->mChannels[channelIndex];
        if (!nodeAnimation) {
            continue;
        }

        JointTrack track{};
        track.jointName = nodeAnimation->mNodeName.C_Str();

        track.translate.keyframes.reserve(nodeAnimation->mNumPositionKeys);
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumPositionKeys; ++keyIndex) {
            const aiVectorKey& key = nodeAnimation->mPositionKeys[keyIndex];
            track.translate.keyframes.push_back({
                static_cast<float>(key.mTime / ticksPerSecond),
                { key.mValue.x, key.mValue.y, key.mValue.z }
                });
        }

        track.rotate.keyframes.reserve(nodeAnimation->mNumRotationKeys);
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumRotationKeys; ++keyIndex) {
            const aiQuatKey& key = nodeAnimation->mRotationKeys[keyIndex];
            track.rotate.keyframes.push_back({
                static_cast<float>(key.mTime / ticksPerSecond),
                { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w }
                });
        }

        track.scale.keyframes.reserve(nodeAnimation->mNumScalingKeys);
        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumScalingKeys; ++keyIndex) {
            const aiVectorKey& key = nodeAnimation->mScalingKeys[keyIndex];
            track.scale.keyframes.push_back({
                static_cast<float>(key.mTime / ticksPerSecond),
                { key.mValue.x, key.mValue.y, key.mValue.z }
                });
        }

        std::vector<float> keyTimes;
        for (const KeyframeVector3& key : track.translate.keyframes) { AddUniqueKeyTime(keyTimes, key.time); }
        for (const KeyframeQuaternion& key : track.rotate.keyframes) { AddUniqueKeyTime(keyTimes, key.time); }
        for (const KeyframeVector3& key : track.scale.keyframes) { AddUniqueKeyTime(keyTimes, key.time); }
        std::sort(keyTimes.begin(), keyTimes.end());

        track.keys.reserve(keyTimes.size());
        for (float keyTime : keyTimes) {
            Keyframe keyframe{};
            keyframe.time = keyTime;
            keyframe.translate = track.translate.keyframes.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : CalculateValue(track.translate.keyframes, keyTime);
            keyframe.rotate = track.rotate.keyframes.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : QuaternionToEulerXYZ(CalculateValue(track.rotate.keyframes, keyTime));
            keyframe.scale = track.scale.keyframes.empty() ? Vector3{ 1.0f, 1.0f, 1.0f } : CalculateValue(track.scale.keyframes, keyTime);
            track.keys.push_back(keyframe);
        }

        SortJointTrackKeys(track);
        clip.tracks.push_back(std::move(track));
    }
#endif

    return clip;
}

AnimationClip CaptureSkeletonPoseAsClip(const Skeleton& skeleton, const std::string& clipName, float duration) {
    AnimationClip clip{};
    clip.name = clipName.empty() ? "NewClip" : clipName;
    clip.duration = std::max(duration, 0.0001f);
    clip.tracks.reserve(skeleton.joints.size());

    for (const Joint& joint : skeleton.joints) {
        JointTrack track{};
        track.jointName = joint.name;
        track.keys.push_back({ 0.0f, joint.localTranslate, joint.localRotate, joint.localScale });
        track.keys.push_back({ clip.duration, joint.localTranslate, joint.localRotate, joint.localScale });
        clip.tracks.push_back(track);
    }

    return clip;
}

void ApplyAnimationClipAtTime(const AnimationClip& clip, Skeleton& skeleton, float time) {
    if (clip.tracks.empty()) {
        return;
    }

    float localTime = std::clamp(time, 0.0f, std::max(clip.duration, 0.0f));
    for (Joint& joint : skeleton.joints) {
        const JointTrack* track = FindJointTrack(clip, joint.name);
        if (!track || track->keys.empty()) {
            continue;
        }

        Keyframe sampled = SampleTrack(*track, localTime);
        joint.localTranslate = sampled.translate;
        joint.localRotate = sampled.rotate;
        joint.localScale = sampled.scale;
    }

    UpdateSkeletonWorldTransforms(skeleton);
}

bool SaveAnimationClipToJson(const AnimationClip& clip, const std::string& filePath) {
    try {
        std::filesystem::path path(filePath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream stream(path, std::ios::out | std::ios::trunc);
        if (!stream.is_open()) {
            return false;
        }

        stream << std::fixed << std::setprecision(6);
        stream << "{\n";
        stream << "  \"name\": \"" << clip.name << "\",\n";
        stream << "  \"duration\": " << clip.duration << ",\n";
        stream << "  \"tracks\": [\n";
        for (size_t trackIndex = 0; trackIndex < clip.tracks.size(); ++trackIndex) {
            const JointTrack& track = clip.tracks[trackIndex];
            stream << "    {\n";
            stream << "      \"joint\": \"" << track.jointName << "\",\n";
            stream << "      \"keys\": [\n";
            for (size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
                const Keyframe& key = track.keys[keyIndex];
                stream << "        {\n";
                stream << "          \"time\": " << key.time << ",\n";
                stream << "          \"translate\": [" << key.translate.x << ", " << key.translate.y << ", " << key.translate.z << "],\n";
                stream << "          \"rotate\": [" << key.rotate.x << ", " << key.rotate.y << ", " << key.rotate.z << "],\n";
                stream << "          \"scale\": [" << key.scale.x << ", " << key.scale.y << ", " << key.scale.z << "]\n";
                stream << "        }" << (keyIndex + 1 < track.keys.size() ? "," : "") << "\n";
            }
            stream << "      ]\n";
            stream << "    }" << (trackIndex + 1 < clip.tracks.size() ? "," : "") << "\n";
        }
        stream << "  ]\n";
        stream << "}\n";
        return true;
    } catch (...) {
        return false;
    }
}

bool LoadAnimationClipFromJson(const std::string& filePath, AnimationClip& clip) {
    try {
        std::ifstream stream{ std::filesystem::path(filePath) };
        if (!stream.is_open()) {
            return false;
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        JsonReader reader(buffer.str());
        AnimationClip loadedClip{};
        if (!reader.ParseAnimationClip(loadedClip)) {
            return false;
        }

        if (loadedClip.name.empty()) {
            loadedClip.name = "LoadedClip";
        }
        loadedClip.duration = std::max(loadedClip.duration, 0.0001f);
        for (JointTrack& track : loadedClip.tracks) {
            SortJointTrackKeys(track);
        }
        clip = std::move(loadedClip);
        return true;
    } catch (...) {
        return false;
    }
}
