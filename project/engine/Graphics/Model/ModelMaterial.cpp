#include "Model.h"
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif

namespace {
    std::string ToGenericString(const std::filesystem::path& path) {
        return path.generic_string();
    }

    bool FileExists(const std::string& path) {
        return !path.empty() && std::filesystem::exists(std::filesystem::path(path));
    }

    std::string ResolveTexturePath(const std::string& directoryPath, const std::string& textureFilename) {
        if (textureFilename.empty()) {
            return {};
        }
        const std::filesystem::path texturePath(textureFilename);
        if (texturePath.is_absolute()) {
            return ToGenericString(texturePath);
        }
        return ToGenericString(std::filesystem::path(directoryPath) / texturePath);
    }

    std::string ReadTextureFilename(std::istringstream& stream) {
        std::string token;
        std::string textureFilename;
        while (stream >> token) {
            textureFilename = token;
        }
        return textureFilename;
    }

    std::string FindAutoPbrTexture(const std::string& directoryPath, const std::string& materialName, const std::string& suffix) {
        if (materialName.empty()) {
            return {};
        }
        const std::filesystem::path path = std::filesystem::path(directoryPath) / "textures" / (materialName + suffix);
        return std::filesystem::exists(path) ? ToGenericString(path) : std::string{};
    }
}

void Model::DrawPbrMaterialImGui() {
#ifdef _DEBUG
    ImGui::SeparatorText("PBR Material Debug");
    if (!materialData_) {
        ImGui::TextDisabled("Material buffer is not ready.");
        return;
    }

    bool usePBR = materialData_->usePBR != 0;
    if (ImGui::Checkbox("Use PBR", &usePBR)) {
        materialData_->usePBR = usePBR ? 1 : 0;
        modelData_.material.usePBR = usePBR;
    }
    ImGui::Text("Show Loaded Textures");
    ImGui::TextWrapped("Material Name: %s", modelData_.material.materialName.empty() ? "(none)" : modelData_.material.materialName.c_str());
    ImGui::TextWrapped("BaseColor: %s", modelData_.material.baseColorTexturePath.empty() ? "(none)" : modelData_.material.baseColorTexturePath.c_str());
    ImGui::TextWrapped("Normal: %s", modelData_.material.normalTexturePath.empty() ? "(none)" : modelData_.material.normalTexturePath.c_str());
    ImGui::TextWrapped("MetallicRoughness: %s", modelData_.material.metallicRoughnessTexturePath.empty() ? "(none)" : modelData_.material.metallicRoughnessTexturePath.c_str());
    ImGui::TextWrapped("SpecularF0: %s", modelData_.material.specularF0TexturePath.empty() ? "(none)" : modelData_.material.specularF0TexturePath.c_str());
    ImGui::Text("BaseColor Loaded: %s", FileExists(modelData_.material.baseColorTexturePath) ? "true" : "false");
    ImGui::Text("Normal Loaded: %s", modelData_.material.hasNormalMap ? "true" : "false");
    ImGui::Text("MetallicRoughness Loaded: %s", modelData_.material.hasMetallicRoughnessMap ? "true" : "false");
    ImGui::Text("SpecularF0 Loaded: %s", modelData_.material.hasSpecularF0Map ? "true" : "false");

    int missingCount = 0;
    missingCount += modelData_.material.hasNormalMap ? 0 : 1;
    missingCount += modelData_.material.hasMetallicRoughnessMap ? 0 : 1;
    missingCount += modelData_.material.hasSpecularF0Map ? 0 : 1;
    ImGui::Text("Missing PBR Texture Count: %d", missingCount);
    if (ImGui::SliderFloat("Metallic Factor", &materialData_->metallicFactor, 0.0f, 2.0f, "%.2f")) {
        modelData_.material.metallicFactor = materialData_->metallicFactor;
    }
    if (ImGui::SliderFloat("Roughness Factor", &materialData_->roughnessFactor, 0.02f, 2.0f, "%.2f")) {
        modelData_.material.roughnessFactor = materialData_->roughnessFactor;
    }
    if (ImGui::SliderFloat("Normal Scale", &materialData_->normalScale, 0.0f, 3.0f, "%.2f")) {
        modelData_.material.normalScale = materialData_->normalScale;
    }
#endif
}

Model::MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    MaterialData materialData;
    std::ifstream file(directoryPath + "/" + filename);
    if (!file.is_open()) {
        return materialData;
    }

    std::string line;
    std::string currentMaterialName;
    std::string pbrMaterialName;
    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream stream(line);
        stream >> identifier;
        if (identifier == "newmtl") {
            stream >> currentMaterialName;
            if (materialData.materialName.empty()) {
                materialData.materialName = currentMaterialName;
            }
        } else if (identifier == "map_Kd") {
            const std::string textureFilename = ReadTextureFilename(stream);
            if (materialData.baseColorTexturePath.empty()) {
                materialData.baseColorTexturePath = ResolveTexturePath(directoryPath, textureFilename);
                materialData.textureFilePath = materialData.baseColorTexturePath;
                pbrMaterialName = currentMaterialName;
            }
        } else if (identifier == "map_Bump" || identifier == "bump") {
            const std::string textureFilename = ReadTextureFilename(stream);
            if (materialData.normalTexturePath.empty()) {
                materialData.normalTexturePath = ResolveTexturePath(directoryPath, textureFilename);
                if (pbrMaterialName.empty()) {
                    pbrMaterialName = currentMaterialName;
                }
            }
        }
    }

    if (pbrMaterialName.empty()) {
        pbrMaterialName = materialData.materialName;
    }
    if (materialData.normalTexturePath.empty()) {
        materialData.normalTexturePath = FindAutoPbrTexture(directoryPath, pbrMaterialName, "_normal.png");
    }
    materialData.metallicRoughnessTexturePath = FindAutoPbrTexture(directoryPath, pbrMaterialName, "_metallicRoughness.png");
    materialData.specularF0TexturePath = FindAutoPbrTexture(directoryPath, pbrMaterialName, "_specularf0.png");
    materialData.usePBR =
        !materialData.normalTexturePath.empty() ||
        !materialData.metallicRoughnessTexturePath.empty() ||
        !materialData.specularF0TexturePath.empty();
    return materialData;
}
