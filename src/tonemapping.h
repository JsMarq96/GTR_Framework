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

		eTonemappers current_mapper = UNCHARTED_MAPPER;


		void init();
		Texture* pass(Texture* prev_pass);
	};
	
};