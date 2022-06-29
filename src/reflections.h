#pragma once

#include "includes.h"
#include "scene.h"
#include "texture.h"
#include "shader.h"

namespace GTR {

#define MAX_REF_PROBE_COUNT 13

	class Renderer;

	struct sReflections_Component {
		Renderer* renderer_instance;

		FBO* ref_fbo = NULL;

		Texture* probe_cubemap[MAX_REF_PROBE_COUNT];
		vec3     probe_position[MAX_REF_PROBE_COUNT];
		bool     is_in_use[MAX_REF_PROBE_COUNT];

		bool debug_render = false;
		bool enable_reflections = false;


		void init(Renderer* rend_inst);
		void clean();
		int init_probe();
		void render_probe(Camera& cam, const int probe_id);
		void render_probes(Camera& cam);
		void capture_probe(const std::vector<BaseEntity*>& entity_list, const int probe_id);
		void capture_all_probes(const std::vector<BaseEntity*>& entity_list);

		inline bool bind_reflections(const vec3 pos, 
									 Shader *shad) const {
			int lowest_index = 0;
			float min_dist = FLT_MAX;

			for (int i = 0; i < MAX_REF_PROBE_COUNT; i++) {
				if (!is_in_use[i]) {
					continue;
				}

				float dist = (pos - probe_position[i]).length();

				if (dist < min_dist) {
					lowest_index = i;
					min_dist = dist;
				}
			}

			shad->setUniform("u_skybox_texture", probe_cubemap[lowest_index], 9);

			return min_dist != FLT_MAX;
		}

		void debug_imgui();
	};
}