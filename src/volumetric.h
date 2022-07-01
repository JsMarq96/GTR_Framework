#pragma once

#include "includes.h"

namespace GTR {
	


	struct sVolumetric_Component {
		FBO* vol_fbo = NULL;

		int sample_size = 100;
		float air_density = 0.001f;

		bool enable_volumetric = false;

		void init(const vec2 &screen_size);

		void render(const Camera *cam, const vec2 &screen_size);

		inline void add_volumetric() {
		}
	};
};