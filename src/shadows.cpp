#include "shadows.h"


void GTR::ShadowRenderer::init() {
	shadowmap = new FBO();
	shadowmap->setDepthOnly(SHADOW_MAP_RES, SHADOW_MAP_RES);
}
void GTR::ShadowRenderer::clean() {
	shadowmap->freeTextures();
}

void GTR::ShadowRenderer::render_light(sShadowDrawCall& draw_call, Matrix44 &vp_matrix) {
	//define locals to simplify coding
	Shader* shader = Shader::Get("shadow_flat");

	shader->enable();

	assert(glGetError() == GL_NO_ERROR);
	glEnable(GL_CULL_FACE);

	for (uint16_t i = 0; i < draw_call.obj_cout; i++) {
		//upload uniforms
		Texture* tex = draw_call.albedo_textures[i];
		if (tex == NULL) {
			tex = Texture::getWhiteTexture();
		}
		shader->setUniform("u_viewprojection", vp_matrix);
		shader->setUniform("u_texture", tex, 0);
		shader->setUniform("u_alpha_cutoff", draw_call.alpha_cutoffs[i]);
		shader->setUniform("u_model", draw_call.models[i]);

		//do the draw call that renders the mesh into the screen
		draw_call.meshes[i]->render(GL_TRIANGLES);
	}

	assert(glGetError() == GL_NO_ERROR);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void GTR::ShadowRenderer::render_scene_shadows(Camera *cam) {
	// Disable color writing
	glColorMask(false, false, false, false);

	// Enable write to FBO
	shadowmap->bind();

	glClear(GL_DEPTH_BUFFER_BIT);

	for (light_projection_count = 0; light_projection_count < draw_call_stack.size(); light_projection_count++) {
		sShadowDrawCall& draw_call = draw_call_stack[light_projection_count];

		if (draw_call.light == NULL) {
			continue;
		}

		Camera light_cam;
		// Set the view-projection
		Matrix44& light_model = draw_call.light->get_model();
		vec3 light_pos;
		light_pos = light_model.getTranslation();
		/*if (draw_call.light->light_type == DIRECTIONAL_LIGHT) {
			light_pos = cam->eye + vec3(0.0f, 30.0f, 0.0f);
		} else {
			
		}*/
		Matrix44 projection, view;
		
		light_cam.lookAt(light_pos, light_model * vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));

		if (draw_call.light->light_type == SPOT_LIGHT) {
			light_cam.setPerspective(draw_call.light->cone_angle,
				1.0f,
				0.1f,
				draw_call.light->max_distance);
		}
		else if (draw_call.light->light_type == DIRECTIONAL_LIGHT) {
			
			float half_area = draw_call.light->area_size / 2.0f;
			light_cam.setOrthographic(-half_area,
				half_area,
				-half_area,
				half_area,
				0.1f,
				draw_call.light->max_distance);
		}

		light_view_projections[light_projection_count] = light_cam.viewprojection_matrix;


		// For the shadomap atlas
		shadowmap->setViewportAsUVs(SHADOW_TILES_SIZES[light_projection_count].x,
			SHADOW_TILES_SIZES[light_projection_count].y,
			SHADOW_TILES_SIZES[light_projection_count].z,
			SHADOW_TILES_SIZES[light_projection_count].z);

		render_light(draw_call, light_view_projections[light_projection_count]);
		// Cleanup
		draw_call.clear();
	}

	// Restore the viewport
	glViewport(0, 0, Application::instance->window_width, Application::instance->window_height);

	// Reenable write to framebuffer
	shadowmap->unbind();

	// Re-enable color writing
	glColorMask(true, true, true, true);
}