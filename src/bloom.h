#pragma once
#include "includes.h"
#include "texture.h"
#include "shader.h"
#include "mesh.h"
#include "fbo.h"
#include "scene.h"

namespace GTR {
	
	struct SBloom_Component {

		FBO* bloom_FBO = NULL;

		int bloom_steps = 5;
		int max_LOD = 10;
		int min_step_size = 1;

		bool enabled_bloom = true;

		void init(const vec2 size);

		Texture* bloom_pass(Texture* base);

		void render_imgui();
	};
};

