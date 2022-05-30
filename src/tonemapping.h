#pragma once

#include "mesh.h"
#include "fbo.h"
#include "shader.h"
#include "application.h"


namespace GTR {

	enum eTonemappers : uint16_t {
		PERCEPTION_MAPPER = 0,
		UNCHARTED_MAPPER
	};

	struct Tonemapping_Component {

		FBO* fbo = NULL;
		FBO* compute_fbo = NULL;
		FBO* ilumination_fbo = NULL;

		bool compute_fbo_swapchain;

		bool first_frame = false;

		eTonemappers current_mapper = PERCEPTION_MAPPER;

		void init();
		Texture* pass(Texture* prev_pass);
		Texture* get_avg_max_lum_of_texture(Texture* text);
	};
	
};