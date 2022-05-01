#include "shadows.h"


void GTR::ShadowRenderer::init() {
	shadowmap = new FBO();
	shadowmap->setDepthOnly(SHADOW_MAP_RES, SHADOW_MAP_RES);
}
void GTR::ShadowRenderer::clean() {
	shadowmap->freeTextures();
}

void GTR::ShadowRenderer::render_light(sShadowDrawCall& draw_call) {
	//define locals to simplify coding
	Shader* shader = Shader::Get("flat");

	Matrix44 view_matrix;

	Matrix44& light_model = draw_call.light->get_model();
	vec3 light_pos = light_model.getTranslation();

	view_matrix.lookAt(light_pos, light_model * vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));

	// Set the type of the view-projection
	if (draw_call.light->light_type == SPOT_LIGHT) {
		Matrix44 projection;
		projection.perspective(draw_call.light->cone_angle,
			1.0f,
			0.1f,
			draw_call.light->max_distance);
		view_matrix = view_matrix * projection;
	}
	else if (draw_call.light->light_type == DIRECTIONAL_LIGHT) {
		Matrix44 projection;
		float half_area = draw_call.light->area_size / 2.0f;
		projection.ortho(-half_area,
			half_area,
			half_area,
			-half_area,
			0.1f,
			draw_call.light->max_distance);
		view_matrix = view_matrix * projection;
	}

	light_view_projections[light_projection_count++] = view_matrix;

	shader->enable();

	assert(glGetError() == GL_NO_ERROR);
	glEnable(GL_CULL_FACE);

	for (uint16_t i = 0; i < draw_call.obj_cout; i++) {
		//upload uniforms
		shader->setUniform("u_viewprojection", view_matrix);
		shader->setUniform("u_model", draw_call.models[i]);

		//do the draw call that renders the mesh into the screen
		draw_call.meshes[i]->render(GL_TRIANGLES);
	}

	assert(glGetError() == GL_NO_ERROR);

	glViewport(0, 0, Application::instance->window_width, Application::instance->window_height);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void GTR::ShadowRenderer::render_scene_shadows() {
	// Disable color writing
	glColorMask(false, false, false, false);

	// Enable write to FBO
	shadowmap->bind();

	glClear(GL_DEPTH_BUFFER_BIT);

	for (uint16_t i = 0; i < draw_call_stack.size(); i++) {
		// For the shadomap atlas
		shadowmap->setViewportAsUVs(TILES_SIZES[i].x, TILES_SIZES[i].y, TILES_SIZES[i].z, TILES_SIZES[i].z);
		render_light(draw_call_stack[i]);
		draw_call_stack[i].clear();
	}

	// Reenable write to framebuffer
	shadowmap->unbind();

	// Re-enable color writing
	glColorMask(true, true, true, true);
}