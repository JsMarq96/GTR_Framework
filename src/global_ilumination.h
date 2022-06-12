#pragma once

#include "includes.h"
#include "utils.h"
#include "camera.h"
#include "fbo.h"
#include "sphericalharmonics.h"
#include "scene.h"

namespace GTR {

#define MAX_PROBE_COUNT 130

	class Renderer;

	struct sGI_Component {
		Renderer* renderer_instance;
		FBO* irradiance_fbo;
		
		bool  used_probe[MAX_PROBE_COUNT];
		vec3  probe_position[MAX_PROBE_COUNT];
		SphericalHarmonics harmonics[MAX_PROBE_COUNT];
		uint32_t linear_indices[MAX_PROBE_COUNT];

		vec3 origin_probe_position = vec3(-257.143f, 28.570f, -371.428f);
		vec3 probe_area_size = vec3(566.667f, 626.6670f, 500.0f);
		float probe_distnace_radius = 100.0f;

		float debug_spheres = false;

		// Debug ===

		void init(Renderer *rend_inst);

		void create_probe_area(const vec3 postion, const vec3 size, const float probe_area_size);

		void compute_all_probes(const std::vector<BaseEntity*> &entity_list);

		void render_to_probe(const std::vector<BaseEntity*> entity_list, const uint32_t probe_id);

		void render_imgui();

		void debug_render_probe(const uint32_t probe_id, const float radius, Camera* cam);

		void debug_render_all_probes(const float radius, Camera* cam);
	};
};