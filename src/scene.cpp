#include "scene.h"
#include "utils.h"

#include "prefab.h"
#include "extra/cJSON.h"
#include "extra/hdre.h"

GTR::Scene* GTR::Scene::instance = NULL;

GTR::Scene::Scene()
{
	instance = this;
	
}

void GTR::Scene::clear()
{
	for (int i = 0; i < entities.size(); ++i)
	{
		BaseEntity* ent = entities[i];
		delete ent;
	}
	entities.resize(0);
}


void GTR::Scene::addEntity(BaseEntity* entity)
{
	entities.push_back(entity); entity->scene = this;
}

bool GTR::Scene::load(const char* filename)
{
	std::string content;

	this->filename = filename;
	std::cout << " + Reading scene JSON: " << filename << "..." << std::endl;

	if (!readFile(filename, content))
	{
		std::cout << "- ERROR: Scene file not found: " << filename << std::endl;
		return false;
	}

	//parse json string 
	cJSON* json = cJSON_Parse(content.c_str());
	if (!json)
	{
		std::cout << "ERROR: Scene JSON has errors: " << filename << std::endl;
		return false;
	}

	//read global properties
	background_color = readJSONVector3(json, "background_color", background_color);
	ambient_light = readJSONVector3(json, "ambient_light", ambient_light );
	main_camera.eye = readJSONVector3(json, "camera_position", main_camera.eye);
	main_camera.center = readJSONVector3(json, "camera_target", main_camera.center);
	main_camera.fov = readJSONNumber(json, "camera_fov", main_camera.fov);
	std::string skybox_dir = readJSONString(json, "skybox", "");

	// Cleanup of prefab list
	prefab_storage.clear();

	//entities
	cJSON* entities_json = cJSON_GetObjectItemCaseSensitive(json, "entities");
	cJSON* entity_json;
	cJSON_ArrayForEach(entity_json, entities_json)
	{
		std::string type_str = cJSON_GetObjectItem(entity_json, "type")->valuestring;
		BaseEntity* ent = createEntity(type_str);
		if (!ent)
		{
			std::cout << " - ENTITY TYPE UNKNOWN: " << type_str << std::endl;
			//continue;
			ent = new BaseEntity();
		}

		addEntity(ent);

		if (cJSON_GetObjectItem(entity_json, "name"))
		{
			ent->name = cJSON_GetObjectItem(entity_json, "name")->valuestring;
			stdlog(std::string(" + entity: ") + ent->name);
			 
			if (type_str == "PREFAB") {
				prefab_storage.insert({ ent->name, (PrefabEntity*) ent});
			}
		}

		//read transform
		if (cJSON_GetObjectItem(entity_json, "position"))
		{
			ent->model.setIdentity();
			Vector3 position = readJSONVector3(entity_json, "position", Vector3());
			ent->model.translate(position.x, position.y, position.z);
		}

		if (cJSON_GetObjectItem(entity_json, "angle"))
		{
			float angle = cJSON_GetObjectItem(entity_json, "angle")->valuedouble;
			ent->model.rotate(angle * DEG2RAD, Vector3(0, 1, 0));
		}

		if (cJSON_GetObjectItem(entity_json, "rotation"))
		{
			Vector4 rotation = readJSONVector4(entity_json, "rotation");
			Quaternion q(rotation.x, rotation.y, rotation.z, rotation.w);
			Matrix44 R;
			q.toMatrix(R);
			ent->model = R * ent->model;
		}

		if (cJSON_GetObjectItem(entity_json, "target"))
		{
			Vector3 target = readJSONVector3(entity_json, "target", Vector3());
			Vector3 front = target - ent->model.getTranslation();
			ent->model.setFrontAndOrthonormalize(front);
		}

		if (cJSON_GetObjectItem(entity_json, "scale"))
		{
			Vector3 scale = readJSONVector3(entity_json, "scale", Vector3(1, 1, 1));
			ent->model.scale(scale.x, scale.y, scale.z);
		}

		ent->configure(entity_json);
	}

	//free memory
	cJSON_Delete(json);

	return true;
}

GTR::BaseEntity* GTR::Scene::createEntity(std::string type)
{
	if (type == "PREFAB") {
		return new GTR::PrefabEntity();
	}
	else if (type == "LIGHT") {
		return (GTR::BaseEntity*) new GTR::LightEntity();
	}
    return NULL;
}

void GTR::BaseEntity::renderInMenu()
{
#ifndef SKIP_IMGUI
	ImGui::Text("Name: %s", name.c_str()); // Edit 3 floats representing a color
	ImGui::Checkbox("Visible", &visible); // Edit 3 floats representing a color
	//Model edit
	ImGuiMatrix44(model, "Model");
#endif
}




GTR::PrefabEntity::PrefabEntity()
{
	entity_type = PREFAB;
	prefab = NULL;
}

void GTR::PrefabEntity::configure(cJSON* json)
{
	if (cJSON_GetObjectItem(json, "filename"))
	{
		filename = cJSON_GetObjectItem(json, "filename")->valuestring;
		prefab = GTR::Prefab::Get( (std::string("data/") + filename).c_str());
	}
	if (cJSON_GetObjectItem(json, "pbr_type"))
	{
		int pbr_id = cJSON_GetObjectItem(json, "pbr_type")->valueint;
		pbr_structure = (ePBR_Type)pbr_id;
	}
}

void GTR::PrefabEntity::renderInMenu()
{
	BaseEntity::renderInMenu();

#ifndef SKIP_IMGUI
	ImGui::Text("filename: %s", filename.c_str()); // Edit 3 floats representing a color
	if (prefab && ImGui::TreeNode(prefab, "Prefab Info"))
	{
		prefab->root.renderInMenu();
		ImGui::TreePop();
	}
#endif
}

/// LIGHT ENTITY =================
void GTR::LightEntity::configure(cJSON* json) {
	if (!cJSON_GetObjectItem(json, "light_type"))
		assert("Light does not have a type");

	std::string light_type_raw = cJSON_GetObjectItem(json, "light_type")->valuestring;

	// Concrete parameters
	if (light_type_raw == "SPOT") {
		light_type = SPOT_LIGHT;

		if (cJSON_GetObjectItem(json, "cone_angle"))
		{
			cone_angle = readJSONNumber(json, "cone_angle", 45.0f);
		}

		if (cJSON_GetObjectItem(json, "cone_exp"))
		{
			cone_exp_decay = readJSONNumber(json, "cone_exp", 60.0f);
		}
	}
	else if (light_type_raw == "DIRECTIONAL") {
		light_type = DIRECTIONAL_LIGHT;

		if (cJSON_GetObjectItem(json, "area_size"))
		{
			area_size = readJSONNumber(json, "area_size", 1500.0f);
		}
	}
	else {
		light_type = POINT_LIGHT;
	}

	// Shared parameters upon all lights
	if (cJSON_GetObjectItem(json, "angle"))
	{
		rotation_angle = readJSONNumber(json, "angle", 0.0f);
	}

	if (cJSON_GetObjectItem(json, "color"))
	{
		color = readJSONVector3(json, "color", Vector3(1, 1, 1));
	}

	if (cJSON_GetObjectItem(json, "intensity"))
	{
		intensity = readJSONNumber(json, "intensity", 1.0f);
	}

	if (cJSON_GetObjectItem(json, "max_dist"))
	{
		max_distance = readJSONNumber(json, "max_dist", 70.0f);
	}
}

void GTR::LightEntity::renderInMenu() {
#ifndef SKIP_IMGUI
	std::string light_types[LIGHT_TYPE_COUNT] = { "Point light", "Directional light", "Spot light" };
	ImGui::Text("Name: %s | %s", name.c_str(), light_types[light_type].c_str());
	ImGui::Checkbox("Visible", &visible); // Edit 3 floats representing a color

	ImGui::ColorEdit3("Light color", (float*)&color);
	ImGui::SliderFloat("Intensity", &intensity, 20.0f, 500.0f);
	ImGui::SliderFloat("Max. distance", &max_distance, 0.0f, 1500.0f);

	switch (light_type) {
	case SPOT_LIGHT:
		ImGui::SliderFloat("Cone angle", &cone_angle, 0.0f, 357.0f);
		ImGui::SliderFloat("Cone exp. decay", &cone_exp_decay, 0.0f, 180.0f);
		break;
	case DIRECTIONAL_LIGHT:
		ImGui::SliderFloat("Area size", &area_size, 0.0f, 2000.0f);
		break;
	}

	//Model edit
	ImGuiMatrix44(model, "Model");
#endif
}