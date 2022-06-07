#pragma once

#include "includes.h"
#include "utils.h"
#include "camera.h"
#include "fbo.h"
#include "sphericalharmonics.h"
#include "scene.h"

namespace GTR {

#define MAX_PROBE_COUNT 30

	class Renderer;

	struct sGI_Component {
		Renderer* renderer_instance;
		FBO* irradiance_fbo;
		
		bool  used_probe[MAX_PROBE_COUNT];
		vec3  probe_position[MAX_PROBE_COUNT];
		SphericalHarmonics harmonics[MAX_PROBE_COUNT];
		uint32_t linear_indices[MAX_PROBE_COUNT];

		// Debug ===

		void init(Renderer *rend_inst);

		void render_to_probe(const std::vector<BaseEntity*> entity_list, const uint32_t probe_id);

		void render_imgui();

		void debug_render_probe(const uint32_t probe_id, const float radius, Camera* cam);
	};
};