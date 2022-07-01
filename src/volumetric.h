#pragma once

#include "includes.h"
#include "shadows.h"
#include "frusturm_culling.h"

namespace GTR {

	struct sVolumetric_Component {
		FBO* vol_fbo = NULL;

		int sample_size = 100;
		float max_ray_len = 800.0f;
		float air_density = 0.001f;

		bool enable_volumetric = false;

		void init(const vec2 &screen_size);

		void render_imgui();

		void render(const Camera *cam, 
					const vec2 &screen_size, 
					const CULLING::sSceneCulling* scene_data, 
						ShadowRenderer* shadow_manager, 
					Texture *depth_Tex);

	};
};