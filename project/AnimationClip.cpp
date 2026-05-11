#include "AnimationClip.h"
#include "Skeleton.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
    Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    Keyframe SampleTrack(const JointTrack& track, float time) {
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
