#include "global_ilumination.h"

#include "renderer.h"

void GTR::sGI_Component::init() {
	irradiance_fbo = new FBO();

	irradiance_fbo->create(64, 64, 
		1, 
		GL_RGBA, 
		GL_FLOAT, 
		true);
}

void GTR::sGI_Component::render_to_probe(const uint32_t probe_id) {
	FloatImage cube_views[6];

	Camera render_cam;
}