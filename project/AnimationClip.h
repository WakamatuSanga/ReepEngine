#pragma once
#include "Matrix4x4.h"
#include <string>
#include <vector>

struct Skeleton;

struct Keyframe {
    float time = 0.0f;
    Vector3 translate{ 0.0f, 0.0f, 0.0f };
    Vector3 rotate{ 0.0f, 0.0f, 0.0f };
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct JointTrack {
    std::string jointName;
    std::vector<Keyframe> keys;
};

struct AnimationClip {
    std::string name = "NewClip";
    float duration = 1.0f;
    std::vector<JointTrack> tracks;
};

JointTrack* FindJointTrack(AnimationClip& clip, const std::string& jointName);
const JointTrack* FindJointTrack(const AnimationClip& clip, const std::string& jointName);
void SortJointTrackKeys(JointTrack& track);

AnimationClip CaptureSkeletonPoseAsClip(const Skeleton& skeleton, const std::string& clipName, float duration);
void ApplyAnimationClipAtTime(const AnimationClip& clip, Skeleton& skeleton, float time);
bool SaveAnimationClipToJson(const AnimationClip& clip, const std::string& filePath);
bool LoadAnimationClipFromJson(const std::string& filePath, AnimationClip& clip);
