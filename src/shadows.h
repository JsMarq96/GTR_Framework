#pragma once

#include "includes.h"
#include "scene.h"
#include "mesh.h"
#include "fbo.h"
#include "shader.h"
#include "application.h"
// ================
	//  SHADOW RENDERER
	// ================
	// NOTA: la sombra de luzes rollo sol (direcional no?) es relativa a la camara
	//       
#define SHADOW_MAP_RES 1024

namespace GTR {
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
		const vec3 TILES_SIZES[7] = {
			vec3(0.0, 0.5, 0.5), // Tile 0
			vec3(0.0, 0.0, 0.5), // 1
			vec3(0.5, 0.0, 0.25), // 2
			vec3(0.5, 0.25, 0.25), // 3
			vec3(0.75, 0.0, 0.25), // 4
			vec3(0.75, 0.25, 0.25), // 5
			vec3(0.75, 0.5, 0.25) // 6
		};

	public:
		std::vector<sShadowDrawCall> draw_call_stack;
		Matrix44 light_view_projections[MAX_LIGHT_NUM];
		uint32_t  light_projection_count = 0;

		FBO* shadowmap;

		float shadow_bias = 0.0001f;

		void init();
		void clean();

		inline void clear_shadowmap() {
			draw_call_stack.clear();
			light_projection_count = 0;
		}

		void render_light(sShadowDrawCall& draw_call);

		void render_scene_shadows();

		inline Texture* get_shadowmap() const {
			return shadowmap->depth_texture;
		}

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

		inline void renderInMenu() {
#ifndef SKIP_IMGUI
			ImGui::Text("ShadowMap settings");
			ImGui::SliderFloat("Shadow bias", &shadow_bias, 0.00001f, 0.01f);
#endif
		}
	};
}