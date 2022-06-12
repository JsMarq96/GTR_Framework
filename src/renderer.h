#pragma once
#include "prefab.h"
#include "fbo.h"
#include "application.h"
#include "shadows.h"
#include "ambient_occlusion.h"
#include "global_ilumination.h"
#include "tonemapping.h"
#include "draw_call.h"
#include "frusturm_culling.h"
#include <algorithm>

//forward declarations
class Camera;

namespace GTR {

	inline float Min(const float x, const float y) {
		return (x < y) ? x : y;
	}


	class Prefab;
	class Material;

	enum eRenderPipe : int {
		FORWARD = 0,
		DEFERRED
	};

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
		enum eDeferredDebugOutput : int {
			RESULT = 0,
			COLOR,
			NORMAL,
			MATERIAL,
			DEPTH,
			WORLD_POS,
			EMMISIVE,
			AMBIENT_OCCLUSION,
			AMBIENT_OCCLUSION_BLUR,
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

		Camera* camera;

		// CONPONENTS =====
		ShadowRenderer shadowmap_renderer;
		SSAO_Component ao_component;
		Tonemapping_Component tonemapping_component;
		sGI_Component irradiance_component;

		// CONFIG FLAGS =====
		eRenderPipe current_pipeline = FORWARD;

		bool use_single_pass = true;
		bool render_light_volumes = false;
		bool use_ssao = true;

		// DEBUG FLAGS ====
		eDeferredDebugOutput deferred_output = RESULT;

		bool show_shadowmap = false;
		bool liniearize_shadowmap_vis = false;

	public:
		std::vector<BaseEntity*>* entity_list;
		Scene* current_scene;
		//add here your functions
		//...
		void init();

		// Scene
		void compute_visible_objects(Camera* camera, std::vector<sDrawCall>* opaque_calls, std::vector<sDrawCall>* translucent_calls);

		void forwardSingleRenderDrawCall(const sDrawCall& draw_call, const Camera* cam, const vec3 ambient_ligh);
		void forwardMultiRenderDrawCall(const sDrawCall& draw_call, const Camera* cam, const Scene* scene);
		void forwardOpacyRenderDrawCall(const sDrawCall& draw_call, const Scene* scene);
		void renderDeferredLightVolumes(CULLING::sSceneCulling* scene_data);
		void tonemappingPass();

		void forwardRenderScene(const Scene* scene, Camera* camera, FBO* resulting_fbo, CULLING::sSceneCulling* scene_data);
		void deferredRenderScene(const Scene* scene, Camera* camera, FBO* resulting_fbo, CULLING::sSceneCulling* scene_data);

		void renderDeferredPlainDrawCall(const sDrawCall& draw_call, const Scene* scene);
		void renderDefferredPass(const Scene* scene, CULLING::sSceneCulling* scene_data);

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
				shader->setUniform("u_emmisive_factor", material->emissive_factor);
			} else {
				shader->setUniform("u_emmisive_factor", vec3(0.0f, 0.0f, 0.0f));
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
			if (show_shadowmap) {
				ImGui::Checkbox("Linearize shadomap visualization", &liniearize_shadowmap_vis);
			}

			const char* rend_pipe[2] = { "FORWARD", "DEFERRED" };
			const char* deferred_output_labels[DEFERRED_DEBUG_SIZE] = { "Final Result", "Color", "Normal", "Materials","Depth", "World pos.", "Emmisive", "Ambient occlusion", "Ambient occlusion blurred" };
			ImGui::Combo("Rendering pipeline", (int*)&current_pipeline, rend_pipe, IM_ARRAYSIZE(rend_pipe));

			switch (current_pipeline) {
			case FORWARD:
				ImGui::Checkbox("Use singlepass", &use_single_pass);
				break;
			case DEFERRED:
				ImGui::Combo("Deferred debug", (int*)&deferred_output, deferred_output_labels, IM_ARRAYSIZE(deferred_output_labels));
				ImGui::Checkbox("Show Light volumes", &render_light_volumes);
				ImGui::Checkbox("Use SSAO", &use_ssao);
				if (use_ssao) {
					ImGui::SliderFloat("AO radius", &ao_component.ao_radius, 0.0f, 40.0f);
				}
				break;
			default:
				break;
			}
			tonemapping_component.imgui_config();
			irradiance_component.render_imgui();
#endif
		}
	};

	Texture* CubemapFromHDRE(const char* filename);
};