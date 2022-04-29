#pragma once
#include "prefab.h"
#include "fbo.h"
#include <algorithm>

//forward declarations
class Camera;

namespace GTR {

#define MAX_LIGHT_NUM 10

	struct sDrawCall {
		Matrix44 model;
		Mesh* mesh;
		Material* material;
		Camera* camera;
		float camera_distance;

		BoundingBox aabb; // Mesh Nounding box

		uint16_t light_count;
		LightEntity *lights_for_call[MAX_LIGHT_NUM];
		// Data oriented comon light data
		eLightType light_type[MAX_LIGHT_NUM];
		vec3  light_positions[MAX_LIGHT_NUM];
		vec3  light_color[MAX_LIGHT_NUM];

		// Spot light data
		float  light_cone_angle[MAX_LIGHT_NUM];
		float  light_cone_decay[MAX_LIGHT_NUM];

		// Directional & Spot data
		vec3   light_direction[MAX_LIGHT_NUM];

		// Area light
		float  light_directional_area[MAX_LIGHT_NUM];

		inline void add_light( LightEntity *light) {
			if (light_count < MAX_LIGHT_NUM) {
				light_type[light_count] = light->light_type;
				light_positions[light_count] = light->get_translation();
				light_color[light_count] = light->color;

				light_direction[light_count] = light->get_model().frontVector() * -1.0f;

				switch(light_type[light_count]) {
				case SPOT_LIGHT:
					light_cone_angle[light_count] = light->cone_angle * 0.0174533; // To radians
					light_cone_decay[light_count] = light->cone_exp_decay;
					break;
				case DIRECTIONAL_LIGHT: 
					light_directional_area[light_count] = light->area_size;
					break;
				default: break;
				}

				lights_for_call[light_count++] = light;
			}
		}
	};

	inline float Min(const float x, const float y) {
		return (x < y) ? x : y;
	}

	// ================
	//  SHADOW RENDERER
	// ================
	// NOTA: la sombra de luzes rollo sol (direcional no?) es relativa a la camara
	//       
#define SHADOW_MAP_RES 1024

	struct sShadowDrawCall {
		LightEntity* light;

		uint16_t obj_cout;
		std::vector<Matrix44> models;
		std::vector<Mesh*> meshes;

		inline void clear() {
			models.clear();
			meshes.clear();
		}
	};

	class ShadowRenderer {
		FBO* shadowmap;
		std::vector<sShadowDrawCall> draw_call_stack;
	public:
		void init();
		void clean();

		void render_light(const sShadowDrawCall& draw_call);

		void render_scene_shadows();

		inline uint16_t add_light(LightEntity* light) {
			draw_call_stack.push_back({ light });

			return draw_call_stack.size() - 1;
		}

		inline void add_instance_to_light(const uint16_t light, Mesh* inst_mesh, Matrix44& inst_model) {
			sShadowDrawCall* draw_call = &draw_call_stack[light];

			draw_call->models.push_back(inst_model);
			draw_call->meshes.push_back(inst_mesh);
			draw_call->obj_cout++;
		}
	};


	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
		std::vector<sDrawCall> _opaque_objects;
		std::vector<sDrawCall> _translucent_objects;
		std::vector<LightEntity*> _scene_lights;

		ShadowRenderer shadowmap_renderer;

	public:

		//add here your functions
		//...
		void init();
		void renderDrawCall(const sDrawCall& draw_call);

		void add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera);

		void add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox &aabb);

		// ===============================================
		// ===============================================

		//renders several elements of the scene
		void renderScene(GTR::Scene* scene, Camera* camera);
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);
};