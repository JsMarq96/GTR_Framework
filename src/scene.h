#ifndef SCENE_H
#define SCENE_H

#include "framework.h"
#include "camera.h"
#include <string>

//forward declaration
class cJSON; 

#include <iostream>

//our namespace
namespace GTR {



	enum eEntityType {
		NONE = 0,
		PREFAB = 1,
		LIGHT = 2,
		CAMERA = 3,
		REFLECTION_PROBE = 4,
		DECALL = 5
	};

	class Scene;
	class Prefab;

	//represents one element of the scene (could be lights, prefabs, cameras, etc)
	class BaseEntity
	{
	public:
		Scene* scene;
		std::string name;
		eEntityType entity_type;
		Matrix44 model;
		bool visible;
		BaseEntity() { entity_type = NONE; visible = true; }
		virtual ~BaseEntity() {}
		virtual void renderInMenu();
		virtual void configure(cJSON* json) {}
	};

	//represents one prefab in the scene
	class PrefabEntity : public GTR::BaseEntity
	{
	public:
		std::string filename;
		Prefab* prefab;
		
		PrefabEntity();
		virtual void renderInMenu();
		virtual void configure(cJSON* json);
	};

	//contains all entities of the scene
	class Scene
	{
	public:
		static Scene* instance;

		Vector3 background_color;
		Vector3 ambient_light;
		Camera main_camera;

		Scene();

		std::string filename;
		std::vector<BaseEntity*> entities;

		void clear();
		void addEntity(BaseEntity* entity);

		bool load(const char* filename);
		BaseEntity* createEntity(std::string type);
	};


	// CUSTOM CLASSES =================
	
	enum eLightType : uint8_t {
		POINT_LIGHT = 0,
		DIRECTIONAL_LIGHT,
		SPOT_LIGHT,
		LIGHT_TYPE_COUNT
	};

	class LightEntity : BaseEntity {
	public:
		eLightType light_type = POINT_LIGHT;
		Vector3 color = {1,1,1};
		float intensity = 1.0f;
		float max_distance = 10.0f;

		// For Spotlight
		float cone_angle = 0.0f;
		float cone_exp_decay = 1.0f;
		
		// For Directional light
		float area_size = 1.0f;

		LightEntity() { entity_type = LIGHT; }

		void configure(cJSON* json);
		void renderInMenu();

		inline bool is_in_range(const vec3& position) {
			return std::abs((model.getTranslation() - position).length()) < max_distance;
		}

		inline bool is_in_range(const BoundingBox& bbox, const vec3 &position) {
			vec3 light_pos = model.getTranslation();
			vec3 bbox_max = bbox.center + bbox.halfsize + position, bbox_min = bbox.center - bbox.halfsize + position;
			if ((light_pos.x >= bbox_min.x && light_pos.x <= bbox_max.x) &&
				(light_pos.y >= bbox_min.y && light_pos.y <= bbox_max.y) &&
				(light_pos.z >= bbox_min.z && light_pos.z <= bbox_max.z)) {
				return true;
			}

			return std::abs((light_pos - position).length()) < max_distance;
		}

		inline vec3 get_translation() {
			return model.getTranslation();
		}
	};

};

#endif