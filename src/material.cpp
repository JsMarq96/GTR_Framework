
#include "material.h"

#include "includes.h"
#include "texture.h"

using namespace GTR;

std::map<std::string, Material*> Material::sMaterials;

Material* Material::Get(const char* name)
{
	assert(name);
	std::map<std::string, Material*>::iterator it = sMaterials.find(name);
	if (it != sMaterials.end())
		return it->second;
	return NULL;
}

void Material::registerMaterial(const char* name)
{
	this->name = name;
	sMaterials[name] = this;

	// Ugly Hack for clouds sorting problem
	if (!strcmp(name, "Clouds"))
	{
		_zMin = 0.9f;
		_zMax = 1.0f;
	}
}

Material::~Material()
{
	if (name.size())
	{
		auto it = sMaterials.find(name);
		if (it != sMaterials.end())
			sMaterials.erase(it);
	}
}

void Material::Release()
{
	std::vector<Material *>mats;

	for (auto mp : sMaterials)
	{
		Material *m = mp.second;
		mats.push_back(m);
	}

	for (Material *m : mats)
	{
		delete m;
	}
	sMaterials.clear();
}


void Material::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Show String
	ImGui::Checkbox("Two sided", &two_sided);
	ImGui::Combo("AlphaMode", (int*)&alpha_mode, "NO_ALPHA\0MASK\0BLEND", 3);
	ImGui::SliderFloat("Alpha Cutoff", &alpha_cutoff, 0.0f, 1.0f);
	ImGui::ColorEdit4("Color", color.v); // Edit 4 floats representing a color + alpha
	if (color_texture.texture && ImGui::TreeNode(color_texture.texture, "Color Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = color_texture.texture->width / (float)color_texture.texture->height;
		ImGui::Image((void*)(intptr_t)color_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
	if (emissive_texture.texture && ImGui::TreeNode(emissive_texture.texture, "Emissive Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = emissive_texture.texture->width / (float)emissive_texture.texture->height;
		ImGui::Image((void*)(intptr_t)emissive_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
	if (opacity_texture.texture && ImGui::TreeNode(opacity_texture.texture, "Opactity Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = opacity_texture.texture->width / (float)opacity_texture.texture->height;
		ImGui::Image((void*)(intptr_t)opacity_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
	if (metallic_roughness_texture.texture && ImGui::TreeNode(metallic_roughness_texture.texture, "Met Rough Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = metallic_roughness_texture.texture->width / (float)metallic_roughness_texture.texture->height;
		ImGui::Image((void*)(intptr_t)metallic_roughness_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
	if (occlusion_texture.texture && ImGui::TreeNode(occlusion_texture.texture, "Occlusion Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = occlusion_texture.texture->width / (float)occlusion_texture.texture->height;
		ImGui::Image((void*)(intptr_t)occlusion_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
	if (normal_texture.texture && ImGui::TreeNode(normal_texture.texture, "Normal Texture"))
	{
		int w = ImGui::GetColumnWidth();
		float aspect = normal_texture.texture->width / (float)normal_texture.texture->height;
		ImGui::Image((void*)(intptr_t)normal_texture.texture->texture_id, ImVec2(w, w * aspect));
		ImGui::TreePop();
	}
#endif
}
