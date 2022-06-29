#pragma once

#include "utils.h"
#include "camera.h"
#include "sphericalharmonics.h"
#include "fbo.h"
#include "texture.h"
#include "shader.h"
#include "scene.h"

namespace GTR {

#define MAX_PROBE_COUNT 530

	class Renderer;

	struct sGI_Component {
		Renderer* renderer_instance;
		FBO* irradiance_fbo;

		Texture* probe_texture = NULL;

		int probe_size = 0;
		vec3* probe_pos = NULL;
		SphericalHarmonics* harmonics = NULL;
		
		vec3 origin_probe_position = vec3(-350.0f, 28.0f, -400.0f);
		vec3 probe_end_position = vec3();
		vec3 probe_area_size = vec3(9.0f, 3.00f, 10.0f);
		vec3 old_probe_size = vec3();
		float probe_distnace_radius = 100.0f;

		bool debug_show_spheres = false;

		void init(Renderer *rend_inst);

		void create_probe_area(const vec3 postion, const vec3 size, const float probe_area_size);

		void compute_all_probes(const std::vector<BaseEntity*> &entity_list);

		void render_to_probe(const std::vector<BaseEntity*> &entity_list, const uint32_t probe_id);

		void render_imgui();

		void debug_render_probe(const uint32_t probe_id, const float radius, Camera* cam);

		void debug_render_all_probes(const float radius, Camera* cam);

		inline void bind_GI(Shader *shad) {

			shad->setUniform("u_gi_probe_tex", probe_texture, 6);
			shad->setUniform("u_irr_start", origin_probe_position);
			shad->setUniform("u_irr_end", probe_end_position);
			shad->setUniform("u_irr_size", probe_area_size);
			shad->setUniform("u_irr_radius", probe_distnace_radius);
			shad->setUniform("u_irr_tex_size", vec2(probe_texture->width, probe_texture->height));
			shad->setUniform("u_irr_probe_count", probe_size);
			//std::cout << probe_count << std::endl;
		}
	};
};