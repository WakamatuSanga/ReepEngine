#include "GpuParticleEffectSerializer.h"

#include "GpuParticleEffectData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void WriteJsonString(std::ostream& stream, const std::string& value) {
	stream << '"';
	for (unsigned char ch : value) {
		switch (ch) {
		case '"':
			stream << "\\\"";
			break;
		case '\\':
			stream << "\\\\";
			break;
		case '\n':
			stream << "\\n";
			break;
		case '\r':
			stream << "\\r";
			break;
		case '\t':
			stream << "\\t";
			break;
		default:
			if (ch < 0x20) {
				stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec << std::setfill(' ');
			} else {
				stream << static_cast<char>(ch);
			}
			break;
		}
	}
	stream << '"';
}

void WriteVector3(std::ostream& stream, const Vector3& value) {
	stream << '[' << value.x << ", " << value.y << ", " << value.z << ']';
}

void WriteVector4(std::ostream& stream, const Vector4& value) {
	stream << '[' << value.x << ", " << value.y << ", " << value.z << ", " << value.w << ']';
}

const char* ToJsonBool(bool value) {
	return value ? "true" : "false";
}

const char* ToEmitterShapeJsonString(GpuParticle::EmitterShape shape) {
	switch (GpuParticle::ClampEmitterShape(shape)) {
	case GpuParticle::EmitterShape::Box:
		return "Box";
	case GpuParticle::EmitterShape::Cone:
		return "Cone";
	case GpuParticle::EmitterShape::Sphere:
	default:
		return "Sphere";
	}
}

class JsonReader {
public:
	explicit JsonReader(std::string_view source)
		: source_(source) {
	}

	bool Parse(GpuParticle::ParticleEffectData& effectData) {
		SkipWhitespace();
		if (!Consume('{')) {
			return false;
		}

		while (true) {
			SkipWhitespace();
			if (Consume('}')) {
				SkipWhitespace();
				return position_ == source_.size();
			}

			std::string key;
			if (!ParseString(key) || !Consume(':')) {
				return false;
			}

			if (key == "emitters") {
				if (!ParseEmitters(effectData.emitters)) {
					return false;
				}
			} else if (key == "particleTypes") {
				if (!ParseParticleTypes(effectData.particleTypes)) {
					return false;
				}
			} else if (key == "runtime") {
				if (!ParseRuntime(effectData.runtime)) {
					return false;
				}
			} else if (!SkipValue()) {
				return false;
			}

			SkipWhitespace();
			if (Consume('}')) {
				SkipWhitespace();
				return position_ == source_.size();
			}
			if (!Consume(',')) {
				return false;
			}
		}
	}

private:
	void SkipWhitespace() {
		while (position_ < source_.size() && std::isspace(static_cast<unsigned char>(source_[position_]))) {
			++position_;
		}
	}

	bool Consume(char expected) {
		SkipWhitespace();
		if (position_ >= source_.size() || source_[position_] != expected) {
			return false;
		}
		++position_;
		return true;
	}

	bool ConsumeKeyword(std::string_view keyword) {
		SkipWhitespace();
		if (source_.substr(position_, keyword.size()) != keyword) {
			return false;
		}
		position_ += keyword.size();
		return true;
	}

	bool ParseString(std::string& value) {
		value.clear();
		if (!Consume('"')) {
			return false;
		}

		while (position_ < source_.size()) {
			const char ch = source_[position_++];
			if (ch == '"') {
				return true;
			}
			if (ch != '\\') {
				value.push_back(ch);
				continue;
			}
			if (position_ >= source_.size()) {
				return false;
			}

			const char escaped = source_[position_++];
			switch (escaped) {
			case '"':
			case '\\':
			case '/':
				value.push_back(escaped);
				break;
			case 'b':
				value.push_back('\b');
				break;
			case 'f':
				value.push_back('\f');
				break;
			case 'n':
				value.push_back('\n');
				break;
			case 'r':
				value.push_back('\r');
				break;
			case 't':
				value.push_back('\t');
				break;
			case 'u':
				if (!SkipUnicodeEscape()) {
					return false;
				}
				value.push_back('?');
				break;
			default:
				return false;
			}
		}
		return false;
	}

	bool SkipUnicodeEscape() {
		if (position_ + 4 > source_.size()) {
			return false;
		}
		for (int count = 0; count < 4; ++count) {
			if (!std::isxdigit(static_cast<unsigned char>(source_[position_ + count]))) {
				return false;
			}
		}
		position_ += 4;
		return true;
	}

	bool ParseNumber(double& value) {
		SkipWhitespace();
		const size_t begin = position_;
		if (position_ < source_.size() && source_[position_] == '-') {
			++position_;
		}

		bool hasDigits = ConsumeDigits();
		if (position_ < source_.size() && source_[position_] == '.') {
			++position_;
			hasDigits = ConsumeDigits() || hasDigits;
		}
		if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
			++position_;
			if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-')) {
				++position_;
			}
			if (!ConsumeDigits()) {
				return false;
			}
		}
		if (!hasDigits) {
			return false;
		}

		const std::string numberText(source_.substr(begin, position_ - begin));
		char* end = nullptr;
		value = std::strtod(numberText.c_str(), &end);
		return end == numberText.c_str() + numberText.size() && std::isfinite(value);
	}

	bool ConsumeDigits() {
		bool consumed = false;
		while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_]))) {
			++position_;
			consumed = true;
		}
		return consumed;
	}

	bool ParseFloat(float& value) {
		double parsed = 0.0;
		if (!ParseNumber(parsed)) {
			return false;
		}
		value = static_cast<float>(parsed);
		return true;
	}

	bool ParseUint(uint32_t& value) {
		double parsed = 0.0;
		if (!ParseNumber(parsed)) {
			return false;
		}
		parsed = std::clamp(parsed, 0.0, static_cast<double>((std::numeric_limits<uint32_t>::max)()));
		value = static_cast<uint32_t>(parsed);
		return true;
	}

	bool ParseBool(bool& value) {
		if (ConsumeKeyword("true")) {
			value = true;
			return true;
		}
		if (ConsumeKeyword("false")) {
			value = false;
			return true;
		}
		return false;
	}

	bool ParseEmitterShape(GpuParticle::EmitterShape& value) {
		SkipWhitespace();
		if (position_ < source_.size() && source_[position_] == '"') {
			std::string shapeName;
			if (!ParseString(shapeName)) {
				return false;
			}
			if (shapeName == "Sphere" || shapeName == "sphere") {
				value = GpuParticle::EmitterShape::Sphere;
				return true;
			}
			if (shapeName == "Box" || shapeName == "box") {
				value = GpuParticle::EmitterShape::Box;
				return true;
			}
			if (shapeName == "Cone" || shapeName == "cone") {
				value = GpuParticle::EmitterShape::Cone;
				return true;
			}
			return false;
		}

		uint32_t shapeIndex = 0;
		if (!ParseUint(shapeIndex)) {
			return false;
		}
		value = shapeIndex < GpuParticle::kEmitterShapeCount
			? static_cast<GpuParticle::EmitterShape>(shapeIndex)
			: GpuParticle::EmitterShape::Sphere;
		return true;
	}

	bool ParseVector3(Vector3& value) {
		Vector3 parsed{};
		if (!Consume('[') || !ParseFloat(parsed.x) || !Consume(',') || !ParseFloat(parsed.y) || !Consume(',') || !ParseFloat(parsed.z) || !Consume(']')) {
			return false;
		}
		value = parsed;
		return true;
	}

	bool ParseVector4(Vector4& value) {
		Vector4 parsed{};
		if (!Consume('[') || !ParseFloat(parsed.x) || !Consume(',') || !ParseFloat(parsed.y) || !Consume(',') || !ParseFloat(parsed.z) || !Consume(',') || !ParseFloat(parsed.w) || !Consume(']')) {
			return false;
		}
		value = parsed;
		return true;
	}

	bool ParseEmitters(std::vector<GpuParticle::Emitter>& emitters) {
		std::vector<GpuParticle::Emitter> parsedEmitters;
		if (!Consume('[')) {
			return false;
		}
		while (true) {
			SkipWhitespace();
			if (Consume(']')) {
				emitters = std::move(parsedEmitters);
				return true;
			}

			GpuParticle::Emitter emitter;
			if (!ParseEmitter(emitter)) {
				return false;
			}
			parsedEmitters.push_back(emitter);

			SkipWhitespace();
			if (Consume(']')) {
				emitters = std::move(parsedEmitters);
				return true;
			}
			if (!Consume(',')) {
				return false;
			}
		}
	}

	bool ParseEmitter(GpuParticle::Emitter& emitter) {
		if (!Consume('{')) {
			return false;
		}
		while (true) {
			SkipWhitespace();
			if (Consume('}')) {
				GpuParticle::NormalizeParticleEffectEmitter(emitter);
				return true;
			}

			std::string key;
			if (!ParseString(key) || !Consume(':')) {
				return false;
			}

			if (key == "enabled") {
				if (!ParseBool(emitter.enabled)) {
					return false;
				}
			} else if (key == "position") {
				if (!ParseVector3(emitter.position)) {
					return false;
				}
			} else if (key == "direction") {
				if (!ParseVector3(emitter.direction)) {
					return false;
				}
			} else if (key == "radius") {
				if (!ParseFloat(emitter.radius)) {
					return false;
				}
			} else if (key == "shape") {
				if (!ParseEmitterShape(emitter.shape)) {
					return false;
				}
			} else if (key == "boxSize") {
				if (!ParseVector3(emitter.boxSize)) {
					return false;
				}
			} else if (key == "coneHeight") {
				if (!ParseFloat(emitter.coneHeight)) {
					return false;
				}
			} else if (key == "emitCount") {
				if (!ParseUint(emitter.emitCount)) {
					return false;
				}
			} else if (key == "emitInterval") {
				if (!ParseFloat(emitter.emitInterval)) {
					return false;
				}
			} else if (key == "emissionRate") {
				if (!ParseFloat(emitter.emissionRate)) {
					return false;
				}
			} else if (key == "randomSeed") {
				if (!ParseUint(emitter.randomSeed)) {
					return false;
				}
			} else if (key == "particleTypeIndex") {
				if (!ParseUint(emitter.particleTypeIndex)) {
					return false;
				}
			} else if (!SkipValue()) {
				return false;
			}

			SkipWhitespace();
			if (Consume('}')) {
				GpuParticle::NormalizeParticleEffectEmitter(emitter);
				return true;
			}
			if (!Consume(',')) {
				return false;
			}
		}
	}

	bool ParseParticleTypes(std::vector<GpuParticle::ParticleType>& particleTypes) {
		std::vector<GpuParticle::ParticleType> parsedTypes;
		if (!Consume('[')) {
			return false;
		}
		while (true) {
			SkipWhitespace();
			if (Consume(']')) {
				particleTypes = std::move(parsedTypes);
				return true;
			}

			if (parsedTypes.size() < GpuParticle::kMaxParticleTypes) {
				GpuParticle::ParticleType type = GpuParticle::MakeDefaultParticleEffectType(parsedTypes.size());
				if (!ParseParticleType(type)) {
					return false;
				}
				parsedTypes.push_back(type);
			} else if (!SkipValue()) {
				return false;
			}

			SkipWhitespace();
			if (Consume(']')) {
				particleTypes = std::move(parsedTypes);
				return true;
			}
			if (!Consume(',')) {
				return false;
			}
		}
	}

	bool ParseParticleType(GpuParticle::ParticleType& type) {
		if (!Consume('{')) {
			return false;
		}
		bool hasBaseColor = false;
		bool hasStartColor = false;
		bool hasEndColor = false;
		bool hasDrag = false;
		auto finalizeType = [&type, &hasBaseColor, &hasStartColor, &hasEndColor, &hasDrag]() {
			if (!hasStartColor) {
				type.startColor = type.baseColor;
			}
			if (!hasEndColor) {
				type.endColor = type.baseColor;
			}
			if (!hasBaseColor) {
				type.baseColor = type.startColor;
			}
			if (!hasDrag) {
				type.drag = 0.0f;
			}
			GpuParticle::NormalizeParticleEffectType(type);
		};
		while (true) {
			SkipWhitespace();
			if (Consume('}')) {
				finalizeType();
				return true;
			}

			std::string key;
			if (!ParseString(key) || !Consume(':')) {
				return false;
			}

			if (key == "name") {
				if (!ParseString(type.name)) {
					return false;
				}
			} else if (key == "texturePath") {
				if (!ParseString(type.texturePath)) {
					return false;
				}
			} else if (key == "baseColor") {
				if (!ParseVector4(type.baseColor)) {
					return false;
				}
				hasBaseColor = true;
			} else if (key == "startColor") {
				if (!ParseVector4(type.startColor)) {
					return false;
				}
				hasStartColor = true;
			} else if (key == "endColor") {
				if (!ParseVector4(type.endColor)) {
					return false;
				}
				hasEndColor = true;
			} else if (key == "startScale") {
				if (!ParseFloat(type.startScale)) {
					return false;
				}
			} else if (key == "endScale") {
				if (!ParseFloat(type.endScale)) {
					return false;
				}
			} else if (key == "lifeTimeMin") {
				if (!ParseFloat(type.lifeTimeMin)) {
					return false;
				}
			} else if (key == "lifeTimeMax") {
				if (!ParseFloat(type.lifeTimeMax)) {
					return false;
				}
			} else if (key == "speedMin") {
				if (!ParseFloat(type.speedMin)) {
					return false;
				}
			} else if (key == "speedMax") {
				if (!ParseFloat(type.speedMax)) {
					return false;
				}
			} else if (key == "gravity") {
				if (!ParseFloat(type.gravity)) {
					return false;
				}
			} else if (key == "drag") {
				if (!ParseFloat(type.drag)) {
					return false;
				}
				hasDrag = true;
			} else if (key == "enablePhysics") {
				if (!ParseBool(type.enablePhysics)) {
					return false;
				}
			} else if (key == "enablePlaneCollision") {
				if (!ParseBool(type.enablePlaneCollision)) {
					return false;
				}
			} else if (key == "collisionPlaneY") {
				if (!ParseFloat(type.collisionPlaneY)) {
					return false;
				}
			} else if (key == "restitution") {
				if (!ParseFloat(type.restitution)) {
					return false;
				}
			} else if (key == "friction") {
				if (!ParseFloat(type.friction)) {
					return false;
				}
			} else if (key == "bounceVelocityThreshold") {
				if (!ParseFloat(type.bounceVelocityThreshold)) {
					return false;
				}
			} else if (key == "maxBounceCount") {
				if (!ParseUint(type.maxBounceCount)) {
					return false;
				}
			} else if (key == "killBelowPlane") {
				if (!ParseBool(type.killBelowPlane)) {
					return false;
				}
			} else if (key == "collisionDamping") {
				if (!ParseFloat(type.collisionDamping)) {
					return false;
				}
			} else if (key == "affectedByInfluenceField") {
				if (!ParseBool(type.affectedByInfluenceField)) {
					return false;
				}
			} else if (key == "influenceResponseScale") {
				if (!ParseFloat(type.influenceResponseScale)) {
					return false;
				}
			} else if (key == "affectedByRailFlow") {
				if (!ParseBool(type.affectedByRailFlow)) {
					return false;
				}
			} else if (key == "railFlowScale") {
				if (!ParseFloat(type.railFlowScale)) {
					return false;
				}
			} else if (key == "useAtlas") {
				if (!ParseBool(type.useAtlas)) {
					return false;
				}
			} else if (key == "atlasRows") {
				if (!ParseUint(type.atlasRows)) {
					return false;
				}
			} else if (key == "atlasColumns") {
				if (!ParseUint(type.atlasColumns)) {
					return false;
				}
			} else if (key == "frameCount") {
				if (!ParseUint(type.frameCount)) {
					return false;
				}
			} else if (key == "frameSpeed") {
				if (!ParseFloat(type.frameSpeed)) {
					return false;
				}
			} else if (key == "loopAtlas") {
				if (!ParseBool(type.loopAtlas)) {
					return false;
				}
			} else if (!SkipValue()) {
				return false;
			}

			SkipWhitespace();
			if (Consume('}')) {
				finalizeType();
				return true;
			}
			if (!Consume(',')) {
				return false;
			}
		}
	}

	bool ParseRuntime(GpuParticle::ParticleEffectRuntimeSettings& runtime) {
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

			if (key == "randomEnabled") {
				if (!ParseBool(runtime.randomEnabled)) {
					return false;
				}
			} else if (key == "useFreeListEmit") {
				if (!ParseBool(runtime.useFreeListEmit)) {
					return false;
				}
			} else if (key == "generateUnusedList") {
				if (!ParseBool(runtime.generateUnusedList)) {
					return false;
				}
			} else if (key == "useDeadList") {
				if (!ParseBool(runtime.useDeadList)) {
					return false;
				}
			} else if (key == "autoRecycleDeadList") {
				if (!ParseBool(runtime.autoRecycleDeadList)) {
					return false;
				}
			} else if (key == "autoReuseDeadParticles") {
				if (!ParseBool(runtime.autoReuseDeadParticles)) {
					return false;
				}
			} else if (key == "updateEnabled") {
				if (!ParseBool(runtime.updateEnabled)) {
					return false;
				}
			} else if (key == "maxActiveParticles") {
				if (!ParseUint(runtime.maxActiveParticles)) {
					return false;
				}
			} else if (key == "maxEmitPerFrame") {
				if (!ParseUint(runtime.maxEmitPerFrame)) {
					return false;
				}
			} else if (!SkipValue()) {
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

	bool SkipValue() {
		SkipWhitespace();
		if (position_ >= source_.size()) {
			return false;
		}
		if (source_[position_] == '"') {
			std::string ignored;
			return ParseString(ignored);
		}
		if (source_[position_] == '{') {
			return SkipObject();
		}
		if (source_[position_] == '[') {
			return SkipArray();
		}
		if (ConsumeKeyword("true") || ConsumeKeyword("false") || ConsumeKeyword("null")) {
			return true;
		}
		double ignored = 0.0;
		return ParseNumber(ignored);
	}

	bool SkipObject() {
		if (!Consume('{')) {
			return false;
		}
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

	bool SkipArray() {
		if (!Consume('[')) {
			return false;
		}
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

	std::string_view source_;
	size_t position_ = 0;
};

} // namespace

bool GpuParticle::GpuParticleEffectSerializer::Save(const ParticleEffectData& effectData, const std::string& filePath) {
	try {
		ParticleEffectData outputData = effectData;
		NormalizeParticleEffectData(outputData);

		const std::filesystem::path path(filePath);
		if (path.has_parent_path()) {
			std::filesystem::create_directories(path.parent_path());
		}

		std::ofstream stream(path, std::ios::out | std::ios::trunc);
		if (!stream.is_open()) {
			return false;
		}

		stream << std::fixed << std::setprecision(6);
		stream << "{\n";
		stream << "  \"emitters\": [\n";
		for (size_t index = 0; index < outputData.emitters.size(); ++index) {
			const Emitter& emitter = outputData.emitters[index];
			stream << "    {\n";
			stream << "      \"enabled\": " << ToJsonBool(emitter.enabled) << ",\n";
			stream << "      \"position\": ";
			WriteVector3(stream, emitter.position);
			stream << ",\n";
			stream << "      \"direction\": ";
			WriteVector3(stream, emitter.direction);
			stream << ",\n";
			stream << "      \"radius\": " << emitter.radius << ",\n";
			stream << "      \"shape\": ";
			WriteJsonString(stream, ToEmitterShapeJsonString(emitter.shape));
			stream << ",\n";
			stream << "      \"boxSize\": ";
			WriteVector3(stream, emitter.boxSize);
			stream << ",\n";
			stream << "      \"coneHeight\": " << emitter.coneHeight << ",\n";
			stream << "      \"emitCount\": " << emitter.emitCount << ",\n";
			stream << "      \"emitInterval\": " << emitter.emitInterval << ",\n";
			stream << "      \"emissionRate\": " << emitter.emissionRate << ",\n";
			stream << "      \"randomSeed\": " << emitter.randomSeed << ",\n";
			stream << "      \"particleTypeIndex\": " << emitter.particleTypeIndex << "\n";
			stream << "    }" << (index + 1 < outputData.emitters.size() ? "," : "") << "\n";
		}
		stream << "  ],\n";
		stream << "  \"particleTypes\": [\n";
		for (size_t index = 0; index < outputData.particleTypes.size(); ++index) {
			const ParticleType& type = outputData.particleTypes[index];
			stream << "    {\n";
			stream << "      \"name\": ";
			WriteJsonString(stream, type.name);
			stream << ",\n";
			stream << "      \"texturePath\": ";
			WriteJsonString(stream, type.texturePath);
			stream << ",\n";
			stream << "      \"baseColor\": ";
			WriteVector4(stream, type.baseColor);
			stream << ",\n";
			stream << "      \"startColor\": ";
			WriteVector4(stream, type.startColor);
			stream << ",\n";
			stream << "      \"endColor\": ";
			WriteVector4(stream, type.endColor);
			stream << ",\n";
			stream << "      \"startScale\": " << type.startScale << ",\n";
			stream << "      \"endScale\": " << type.endScale << ",\n";
			stream << "      \"lifeTimeMin\": " << type.lifeTimeMin << ",\n";
			stream << "      \"lifeTimeMax\": " << type.lifeTimeMax << ",\n";
			stream << "      \"speedMin\": " << type.speedMin << ",\n";
			stream << "      \"speedMax\": " << type.speedMax << ",\n";
			stream << "      \"gravity\": " << type.gravity << ",\n";
			stream << "      \"drag\": " << type.drag << ",\n";
			stream << "      \"enablePhysics\": " << ToJsonBool(type.enablePhysics) << ",\n";
			stream << "      \"enablePlaneCollision\": " << ToJsonBool(type.enablePlaneCollision) << ",\n";
			stream << "      \"collisionPlaneY\": " << type.collisionPlaneY << ",\n";
			stream << "      \"restitution\": " << type.restitution << ",\n";
			stream << "      \"friction\": " << type.friction << ",\n";
			stream << "      \"bounceVelocityThreshold\": " << type.bounceVelocityThreshold << ",\n";
			stream << "      \"maxBounceCount\": " << type.maxBounceCount << ",\n";
			stream << "      \"killBelowPlane\": " << ToJsonBool(type.killBelowPlane) << ",\n";
			stream << "      \"collisionDamping\": " << type.collisionDamping << ",\n";
			stream << "      \"affectedByInfluenceField\": " << ToJsonBool(type.affectedByInfluenceField) << ",\n";
			stream << "      \"influenceResponseScale\": " << type.influenceResponseScale << ",\n";
			stream << "      \"affectedByRailFlow\": " << ToJsonBool(type.affectedByRailFlow) << ",\n";
			stream << "      \"railFlowScale\": " << type.railFlowScale << ",\n";
			stream << "      \"useAtlas\": " << ToJsonBool(type.useAtlas) << ",\n";
			stream << "      \"atlasRows\": " << type.atlasRows << ",\n";
			stream << "      \"atlasColumns\": " << type.atlasColumns << ",\n";
			stream << "      \"frameCount\": " << type.frameCount << ",\n";
			stream << "      \"frameSpeed\": " << type.frameSpeed << ",\n";
			stream << "      \"loopAtlas\": " << ToJsonBool(type.loopAtlas) << "\n";
			stream << "    }" << (index + 1 < outputData.particleTypes.size() ? "," : "") << "\n";
		}
		stream << "  ],\n";
		stream << "  \"runtime\": {\n";
		stream << "    \"randomEnabled\": " << ToJsonBool(outputData.runtime.randomEnabled) << ",\n";
		stream << "    \"useFreeListEmit\": " << ToJsonBool(outputData.runtime.useFreeListEmit) << ",\n";
		stream << "    \"generateUnusedList\": " << ToJsonBool(outputData.runtime.generateUnusedList) << ",\n";
		stream << "    \"useDeadList\": " << ToJsonBool(outputData.runtime.useDeadList) << ",\n";
		stream << "    \"autoRecycleDeadList\": " << ToJsonBool(outputData.runtime.autoRecycleDeadList) << ",\n";
		stream << "    \"autoReuseDeadParticles\": " << ToJsonBool(outputData.runtime.autoReuseDeadParticles) << ",\n";
		stream << "    \"updateEnabled\": " << ToJsonBool(outputData.runtime.updateEnabled) << ",\n";
		stream << "    \"maxActiveParticles\": " << outputData.runtime.maxActiveParticles << ",\n";
		stream << "    \"maxEmitPerFrame\": " << outputData.runtime.maxEmitPerFrame << "\n";
		stream << "  }\n";
		stream << "}\n";
		return true;
	} catch (...) {
		return false;
	}
}

bool GpuParticle::GpuParticleEffectSerializer::Load(const std::string& filePath, ParticleEffectData& effectData) {
	try {
		std::ifstream stream{ std::filesystem::path(filePath) };
		if (!stream.is_open()) {
			return false;
		}

		std::ostringstream buffer;
		buffer << stream.rdbuf();

		ParticleEffectData loadedData;
		const std::string jsonText = buffer.str();
		JsonReader reader(jsonText);
		if (!reader.Parse(loadedData)) {
			return false;
		}

		NormalizeParticleEffectData(loadedData);
		effectData = std::move(loadedData);
		return true;
	} catch (...) {
		return false;
	}
}

bool GpuParticle::GpuParticleEffectSerializer::Save(const State& state, const std::string& filePath) {
	return Save(CreateParticleEffectDataFromState(state), filePath);
}

bool GpuParticle::GpuParticleEffectSerializer::Load(const std::string& filePath, State& state) {
	ParticleEffectData effectData;
	if (!Load(filePath, effectData)) {
		return false;
	}

	ApplyParticleEffectDataToState(effectData, state);
	return true;
}

