#pragma once
#include "prefab.h"
#include "fbo.h"
#include "application.h"
#include "shadows.h"
#include <algorithm>

//forward declarations
class Camera;

namespace GTR {
	// Data orienteation here...? 
	struct sDrawCall {
		Matrix44 model;
		Mesh* mesh;
		Material* material;
		Camera* camera;
		float camera_distance;
		ePBR_Type pbr_structure;

		BoundingBox aabb; // Mesh Nounding box

		uint16_t light_count;
		LightEntity *lights_for_call[MAX_LIGHT_NUM];
		// Data oriented comon light data
		eLightType light_type[MAX_LIGHT_NUM];
		vec3  light_positions[MAX_LIGHT_NUM];
		vec3  light_color[MAX_LIGHT_NUM];
		float light_max_distance[MAX_LIGHT_NUM];
		float light_intensities[MAX_LIGHT_NUM];

		int light_shadow_id[MAX_LIGHT_NUM];

		// Spot light data
		float  light_cone_angle[MAX_LIGHT_NUM];
		float  light_cone_decay[MAX_LIGHT_NUM];

		// Directional & Spot data
		vec3   light_direction[MAX_LIGHT_NUM];

		// Area light
		float  light_directional_area[MAX_LIGHT_NUM];


		inline void add_light( LightEntity *light) {
			if (light_count < MAX_LIGHT_NUM) {
				light_shadow_id[light_count] = light->light_id;
				light_type[light_count] = light->light_type;
				light_positions[light_count] = light->get_translation();
				light_color[light_count] = light->color;
				light_max_distance[light_count] = light->max_distance;
				light_intensities[light_count] = light->intensity;

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


	class Prefab;
	class Material;

	enum eRenderPipe : uint8_t {
		FORWARD = 0,
		DEFERRED
	};

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
		enum eDeferredDebugOutput {
			RESULT = 0,
			COLOR,
			NORMAL,
			MATERIAL,
			DEPTH,
			WORLD_POS,
			EMMISIVE,
			DEFERRED_DEBUG_SIZE
		};

		enum eTextMaterials : int {
			ALBEDO_MAT = 1,
			NORMAL_MAT = 10,
			EMMISIVE_MAT = 100,
			MET_ROUGHT_MAT = 1000,
			OCCLUSION_MAT = 10000
		};

		FBO* deferred_gbuffer = NULL;
		FBO* final_illumination_fbo = NULL;

		std::vector<sDrawCall> _opaque_objects;
		std::vector<sDrawCall> _translucent_objects;
		std::vector<LightEntity*> _scene_non_directonal_lights;
		std::vector<LightEntity*> _scene_directional_lights;

		ShadowRenderer shadowmap_renderer;

		eRenderPipe current_pipeline = DEFERRED;

		// Debgging toggles for forward rendering
		bool use_single_pass = true;
		bool render_light_volumes = false;

		// Debugging for Deferred rendering
		eDeferredDebugOutput deferred_output = RESULT;

		bool show_shadowmap = false;
		bool liniearize_shadowmap_vis = false;

		Camera* camera;

	public:

		//add here your functions
		//...
		void init();
		void forwardSingleRenderDrawCall(const sDrawCall& draw_call, const Scene *scene);
		void forwardMultiRenderDrawCall(const sDrawCall& draw_call, const Scene* scene);
		void forwardOpacyRenderDrawCall(const sDrawCall& draw_call, const Scene* scene);
		void renderDeferredLightVolumes();

		void forwardRenderScene(const Scene* scene);
		void deferredRenderScene(const Scene* scene, Camera *camera);

		void renderDeferredPlainDrawCall(const sDrawCall& draw_call, const Scene* scene);
		void renderDefferredPass(const Scene* scene);

		void _init_deferred_renderer();

		void add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, ePBR_Type pbr);

		void add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox& aabb, const ePBR_Type pbr);

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


		// =======================
		// INLINE FUNCTIONS
		// =======================
		inline int bind_textures(const Material* material, Shader* shader) {
			int enabled_textures = 0;
			Texture* albedo_texture = NULL, * emmisive_texture = NULL, * mr_texture = NULL, * normal_texture = NULL;
			Texture* occlusion_texture = NULL;

			albedo_texture = material->color_texture.texture;
			emmisive_texture = material->emissive_texture.texture;
			mr_texture = material->metallic_roughness_texture.texture;
			normal_texture = material->normal_texture.texture;
			occlusion_texture = material->occlusion_texture.texture;

			if (albedo_texture == NULL) {
				enabled_textures += eTextMaterials::ALBEDO_MAT;
				albedo_texture = Texture::getWhiteTexture(); //a 1x1 white texture
			}
				

			if (emmisive_texture == NULL) {
				enabled_textures += eTextMaterials::EMMISIVE_MAT;
				emmisive_texture = Texture::getBlackTexture(); //a 1x1 black texture
			}
				

			if (mr_texture == NULL) {
				enabled_textures += eTextMaterials::MET_ROUGHT_MAT;
				mr_texture = Texture::getWhiteTexture(); //a 1x1 white texture
			}
				

			if (normal_texture == NULL) {
				enabled_textures += eTextMaterials::NORMAL_MAT;
				normal_texture = Texture::getBlackTexture(); //a 1x1 black texture
			}
				

			if (occlusion_texture == NULL) {
				enabled_textures += eTextMaterials::OCCLUSION_MAT;
				occlusion_texture = Texture::getWhiteTexture(); //a 1x1 black texture
			}
				
			shader->setUniform("u_texture", albedo_texture, 0);
			shader->setUniform("u_emmisive_tex", emmisive_texture, 1);
			shader->setUniform("u_met_rough_tex", mr_texture, 2);
			shader->setUniform("u_normal_tex", normal_texture, 3);
			shader->setUniform("u_occlusion_tex", occlusion_texture, 4);

			return enabled_textures;
		};


		inline void renderInMenu() {
#ifndef SKIP_IMGUI
			shadowmap_renderer.renderInMenu();
			ImGui::Checkbox("Show shadowmap", &show_shadowmap);
			ImGui::Checkbox("Linearize shadomap visualization", &liniearize_shadowmap_vis);

			const char* rend_pipe[2] = { "FORWARD", "DEFERRED"};
			const char* deferred_output_labels[DEFERRED_DEBUG_SIZE] = {"Final Result", "Color", "Normal", "Materials", "Emmisive", "Depth", "World pos."};
			ImGui::Combo("Rendering pipeline", (int*) &current_pipeline, rend_pipe, IM_ARRAYSIZE(rend_pipe));

			switch (current_pipeline) {
			case FORWARD:
				ImGui::Checkbox("Use singlepass", &use_single_pass);
				break;
			case DEFERRED:
				ImGui::Combo("Deferred debug", (int*)&deferred_output, deferred_output_labels, IM_ARRAYSIZE(deferred_output_labels));
				ImGui::Checkbox("Show Light volumes", &render_light_volumes);
				break;
			default:
				break;
			}
			
#endif
		}
	};

	Texture* CubemapFromHDRE(const char* filename);
};