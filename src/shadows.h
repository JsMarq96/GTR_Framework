#pragma once

#include "includes.h"
#include "scene.h"
#include "mesh.h"
#include "fbo.h"
#include "shader.h"
#include "application.h"
#include "frusturm_culling.h"
// ================
	//  SHADOW RENDERER
	// ================
	//       
#define SHADOW_MAP_RES 4048

namespace GTR {
	struct sShadowDrawCall {
		LightEntity* light;

		uint16_t obj_cout;
		std::vector<Matrix44> models;
		std::vector<Mesh*> meshes;
		std::vector<Texture*> albedo_textures;
		std::vector<float> alpha_cutoffs;

		inline void clear() {
			models.clear();
			meshes.clear();
			albedo_textures.clear();
		}
	};

	class ShadowRenderer {
	public:
		const vec3 SHADOW_TILES_SIZES[7] = {
			vec3(0.0, 0.5, 0.5), // Tile 0
			vec3(0.0, 0.0, 0.5), // 1
			vec3(0.5, 0.0, 0.5), // 2
			vec3(0.5, 0.5, 0.5), // 3
			vec3(0.75, 0.0, 0.25), // 4
			vec3(0.5, 0.0, 0.5), // 5
			vec3(0.75, 0.5, 0.25) // 6
		};

		std::vector<sShadowDrawCall> draw_call_stack;
		Matrix44 light_view_projections[MAX_LIGHT_NUM];
		int  light_projection_count = 0;

		FBO* shadowmap;

		float shadow_bias = 0.005f;

		void init();
		void clean();

		inline void clear_shadowmap() {
			draw_call_stack.clear();
			light_projection_count = 0;
		}

		void render_light(sShadowDrawCall& draw_call, Matrix44& vp_matrix);

		void render_scene_shadows(Camera* cam);

		inline void bind_shadows(Shader* scene_shader) {
			scene_shader->setUniform("u_shadow_map", get_shadowmap(), 8);
			scene_shader->setMatrix44Array("u_shadow_vp", light_view_projections, MAX_LIGHT_NUM);
			scene_shader->setUniform("u_shadow_count", light_projection_count);
			scene_shader->setUniform("u_shadow_bias", shadow_bias);
		}

		inline Texture* get_shadowmap() const {
			return shadowmap->depth_texture;
		}

		inline uint16_t add_light(LightEntity* light) {
			draw_call_stack.push_back({ light });
			return light->light_id;
		}

		inline void add_instance_to_light(const uint16_t light, Mesh* inst_mesh, Texture *text, float alpha_cutoff, Matrix44& inst_model) {
			sShadowDrawCall* draw_call = &draw_call_stack[light];

			draw_call->models.push_back(inst_model);
			draw_call->meshes.push_back(inst_mesh);
			draw_call->albedo_textures.push_back(text);
			draw_call->alpha_cutoffs.push_back(alpha_cutoff);
			draw_call->obj_cout++;
		}

		void add_scene_data(CULLING::sSceneCulling* scene_data) {
			// Add the lights to the shadowmap
			draw_call_stack.resize(scene_data->_scene_non_directonal_lights.size() + scene_data->_scene_directional_lights.size());

			int light_count = 0;
			for (uint32_t i = 0; i < scene_data->_scene_directional_lights.size(); i++) {
				LightEntity* curr_light = scene_data->_scene_directional_lights[i];
				curr_light->shadow_id = light_count++;
				draw_call_stack[curr_light->shadow_id] = { curr_light };
			}
			for (uint32_t i = 0; i < scene_data->_scene_non_directonal_lights.size(); i++) {
				LightEntity* curr_light = scene_data->_scene_non_directonal_lights[i];
				// Avoid pointlight shadowmaps
				if (curr_light->light_type == DIRECTIONAL_LIGHT || curr_light->light_type == SPOT_LIGHT) {
					curr_light->shadow_id = light_count++;
					draw_call_stack[curr_light->shadow_id] = { curr_light };
				}
			}

			// Add objects to the lights
			for (uint32_t i = 0; i < scene_data->_opaque_objects.size(); i++) {
				BoundingBox world_bounding = scene_data->_opaque_objects[i].aabb;

				// Test if the objects is on the light's frustum
				for (uint16_t light_i = 0; light_i < scene_data->_scene_non_directonal_lights.size(); light_i++) {
					LightEntity* curr_light = scene_data->_scene_non_directonal_lights[light_i];

					if (curr_light->is_in_light_frustum(world_bounding)) {
						add_instance_to_light(curr_light->shadow_id,
							scene_data->_opaque_objects[i].mesh, 
							scene_data->_opaque_objects[i].material->color_texture.texture,
							scene_data->_opaque_objects[i].material->alpha_cutoff,
							scene_data->_opaque_objects[i].model);
					}
				}

				for (uint16_t light_i = 0; light_i < scene_data->_scene_directional_lights.size(); light_i++) {
					LightEntity* curr_light = scene_data->_scene_directional_lights[light_i];

					if (curr_light->is_in_light_frustum(world_bounding)) {
						add_instance_to_light(curr_light->shadow_id,
							scene_data->_opaque_objects[i].mesh,
							scene_data->_opaque_objects[i].material->color_texture.texture,
							scene_data->_opaque_objects[i].material->alpha_cutoff,
							scene_data->_opaque_objects[i].model);
					}
				}
			}
		}

		inline void renderInMenu() {
#ifndef SKIP_IMGUI
			ImGui::Text("ShadowMap settings");
			ImGui::SliderFloat("Shadow bias", &shadow_bias, 0.00001f, 0.01f);
#endif
		}
	};
}