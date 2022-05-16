#ifndef SCENE_H
#define SCENE_H

#include "framework.h"
#include "camera.h"
#include <string>
#include <algorithm>

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
	
	enum eLightType : int {
		POINT_LIGHT = 0,
		DIRECTIONAL_LIGHT,
		SPOT_LIGHT,
		LIGHT_TYPE_COUNT
	};

	inline float min_f(const float x, const float y) {
		return (x > y) ? y : x;
	}
	inline float max_f(const float x, const float y) {
		return (x < y) ? y : x;
	}
	
	class LightEntity : BaseEntity {
	public:
		eLightType light_type = POINT_LIGHT;
		Vector3 color = {1,1,1};
		float intensity = 1.0f;
		float max_distance = 10.0f;

		float rotation_angle = 0.0f;

		// For Spotlight
		float cone_angle = 0.0f;
		float cone_exp_decay = 1.0f;
		
		// For Directional light
		float area_size = 1.0f;

		// Shadowmap data
		vec3 shadomap_tile = vec3();
		uint32_t light_id = 0;

		LightEntity() { entity_type = LIGHT; }

		void configure(cJSON* json);
		void renderInMenu();

		inline bool is_in_range(const vec3& position) {
			return std::abs((model.getTranslation() - position).length()) < max_distance;
		}
		// Check if the light inside the bounding box, or the distance to the center
		// TODO: check distance to the Bounding box
		inline bool is_in_range(const BoundingBox& bbox) {
			vec3 light_pos = model.getTranslation();

			vec3 aabb_point = light_pos;
			aabb_point.x = max_f(aabb_point.x, bbox.center.x - bbox.halfsize.x);
			aabb_point.x = min_f(aabb_point.x, bbox.center.x + bbox.halfsize.x);

			aabb_point.y = max_f(aabb_point.y, bbox.center.y - bbox.halfsize.y);
			aabb_point.y = min_f(aabb_point.y, bbox.center.y + bbox.halfsize.y);

			aabb_point.z = max_f(aabb_point.z, bbox.center.z - bbox.halfsize.z);
			aabb_point.z = min_f(aabb_point.z, bbox.center.z + bbox.halfsize.z);

			return std::abs((light_pos - aabb_point).length()) < max_distance;
		}

		inline bool is_in_light_frustum(const BoundingBox& world_bbox) {

			Camera light_cam;
			light_cam.lookAt(model.getTranslation(), model * vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));

			if (light_type == POINT_LIGHT) {
				return false;
			}
			else if (light_type == SPOT_LIGHT) {
				light_cam.setPerspective(cone_angle, 1.0f, 0.1f, max_distance);
			}
			else {
				float half_area = area_size / 2.0f;
				light_cam.setOrthographic(-half_area, half_area, half_area, -half_area, 0.1f, max_distance);
			}

			
			//if bounding box is inside the light frustum then the object is probably visible
			return light_cam.testBoxInFrustum(world_bbox.center, world_bbox.halfsize + vec3(0.1f, 0.1f, 0.1f));
		}

		inline vec3 get_translation() {
			return model.getTranslation();
		}

		inline Matrix44& get_model() {
			return model;
		}
	};

};

#endif