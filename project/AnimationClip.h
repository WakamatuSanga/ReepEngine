#pragma once
#include "Matrix4x4.h"
#include <string>
#include <vector>

struct Skeleton;

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

template <typename TValue>
struct ValueKeyframe {
    float time = 0.0f;
    TValue value{};
};

using KeyframeVector3 = ValueKeyframe<Vector3>;
using KeyframeQuaternion = ValueKeyframe<Quaternion>;

template <typename TValue>
struct AnimationCurve {
    std::vector<ValueKeyframe<TValue>> keyframes;
};

struct Keyframe {
    float time = 0.0f;
    Vector3 translate{ 0.0f, 0.0f, 0.0f };
    Vector3 rotate{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct JointTrack {
    std::string jointName;
    std::vector<Keyframe> keys;
    AnimationCurve<Vector3> translate;
    AnimationCurve<Quaternion> rotate;
    AnimationCurve<Vector3> scale;
};

struct AnimationClip {
    std::string name = "NewClip";
    float duration = 1.0f;
    std::vector<JointTrack> tracks;
};

JointTrack* FindJointTrack(AnimationClip& clip, const std::string& jointName);
const JointTrack* FindJointTrack(const AnimationClip& clip, const std::string& jointName);
void SortJointTrackKeys(JointTrack& track);

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time);
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);
Vector3 ConvertQuaternionToEulerXYZ(const Quaternion& quaternion);
AnimationClip LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

AnimationClip CaptureSkeletonPoseAsClip(const Skeleton& skeleton, const std::string& clipName, float duration);
void ApplyAnimationClipAtTime(const AnimationClip& clip, Skeleton& skeleton, float time);
bool SaveAnimationClipToJson(const AnimationClip& clip, const std::string& filePath);
bool LoadAnimationClipFromJson(const std::string& filePath, AnimationClip& clip);
