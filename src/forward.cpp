#include "renderer.h"

// Definition of the forward renderer functions
void GTR::Renderer::forwardRenderScene(const Scene* scene, Camera* camera, FBO* resulting_fbo, CULLING::sSceneCulling* scene_data, const bool use_irradiance) {
	resulting_fbo->bind();

	resulting_fbo->enableSingleBuffer(0);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	render_skybox(camera);

	if (use_single_pass) {
		// First, render the opaque object
		for (uint16_t i = 0; i < scene_data->_opaque_objects.size(); i++) {
			forwardSingleRenderDrawCall(scene_data->_opaque_objects[i], camera, scene->ambient_light, use_irradiance, reflections_component.enable_reflections);
		}

		// then, render the translucnet, and masked objects
		for (uint16_t i = 0; i < scene_data->_translucent_objects.size(); i++) {
			forwardSingleRenderDrawCall(scene_data->_translucent_objects[i], camera, scene->ambient_light, use_irradiance, reflections_component.enable_reflections);
		}
	}
	else {
		// First, render the opaque object
		for (uint16_t i = 0; i < scene_data->_opaque_objects.size(); i++) {
			forwardMultiRenderDrawCall(scene_data->_opaque_objects[i], camera, scene);
		}

		// then, render the translucnet, and masked objects
		for (uint16_t i = 0; i < scene_data->_translucent_objects.size(); i++) {
			forwardMultiRenderDrawCall(scene_data->_translucent_objects[i], camera, scene);
		}
	}
	resulting_fbo->unbind();
}

inline void GTR::Renderer::forwardSingleRenderDrawCall(const sDrawCall& draw_call, const Camera *cam, const vec3 ambient_ligh, const bool use_GI, const bool reflections) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;

	//select the blending
	if (draw_call.material->alpha_mode == GTR::eAlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("forward_singlepass_pbr");

	assert(glGetError() == GL_NO_ERROR);

	// Prepare shadow ids
	int* shadow_ids = (int*)malloc(sizeof(int) * draw_call.light_count);
	for (int i = 0; i < draw_call.light_count; i++) {
		shadow_ids[i] = (int) draw_call.lights_for_call[i]->shadow_id;
	}

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// Upload light data
	// Common data of the lights
	shader->setUniform("u_ambient_light", ambient_ligh);
	shader->setUniform3Array("u_light_pos", (float*)draw_call.light_positions, draw_call.light_count);
	shader->setUniform3Array("u_light_color", (float*)draw_call.light_color, draw_call.light_count);
	shader->setUniform1Array("u_light_type", (int*)draw_call.light_type, draw_call.light_count);
	shader->setUniform1Array("u_light_shadow_id", shadow_ids, draw_call.light_count);
	shader->setUniform1Array("u_light_max_dist", (float*)draw_call.light_max_distance, draw_call.light_count);
	shader->setUniform1Array("u_light_intensities", draw_call.light_intensities, draw_call.light_count);
	shader->setUniform("u_num_lights", draw_call.light_count);

	free(shadow_ids);

	// Spotlight data of the lights
	shader->setUniform3Array("u_light_direction", (float*)draw_call.light_direction, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_angle", (float*)draw_call.light_cone_angle, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_decay", (float*)draw_call.light_cone_decay, draw_call.light_count);

	//upload uniforms
	shader->setUniform("u_viewprojection", cam->viewprojection_matrix);
	shader->setUniform("u_camera_position", cam->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	// Material properties
	shader->setUniform("u_color", draw_call.material->color);
	int enabled_texteres = bind_textures(draw_call.material, shader);
	//shader->setUniform("u_ambient_occlusion_tex", Texture::getWhiteTexture(), 5);

	shader->setUniform("u_enabled_texteres", enabled_texteres);

	// Set the shadowmap
	shadowmap_renderer.bind_shadows(shader);

	irradiance_component.bind_GI(shader);
	
	if (reflections) {
		reflections_component.bind_reflections(draw_call.aabb.center, shader);
	} else {
		shader->setUniform("u_skybox_texture", skybox_texture, 9);
	}

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	draw_call.mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}


inline void GTR::Renderer::forwardMultiRenderDrawCall(const sDrawCall& draw_call, const Camera* cam, const Scene* scene) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("multi_phong");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// Render pixels that has equal or less depth that the actual pixel
	glDepthFunc(GL_LEQUAL);
	// Change the blending so that we can render transparent object
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//upload uniforms
	shader->setUniform("u_viewprojection", cam->viewprojection_matrix);
	shader->setUniform("u_camera_position", cam->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	// Material properties
	shader->setUniform("u_color", draw_call.material->color);
	bind_textures(draw_call.material, shader);

	// Set the shadowmap
	shadowmap_renderer.bind_shadows(shader);
	glEnable(GL_BLEND);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	shader->setUniform("u_ambient_light", scene->ambient_light);
	for (int light_id = 0; light_id < draw_call.light_count; light_id++) {
		if (light_id == 0) {
			shader->setUniform("u_ambient_light", scene->ambient_light);
		}
		else if (light_id == 1) {
			// Set blending to additive
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			// Set the ambient and the emissive textures for the next passes
			shader->setUniform("u_ambient_light", vec3(0.0f, 0.0f, 0.0f));
			shader->setUniform("u_emmisive_tex", Texture::getBlackTexture(), 1);
		}
		// Upload light data
		// Common data of the lights
		shader->setUniform("u_light_pos", draw_call.light_positions[light_id]);
		shader->setUniform("u_light_color", draw_call.light_color[light_id]);
		shader->setUniform("u_light_type", draw_call.light_type[light_id]);
		shader->setUniform("u_light_max_dist", draw_call.light_max_distance[light_id]);
		shader->setUniform("u_light_intensities", draw_call.light_intensities[light_id]);
		shader->setUniform("u_light_id", draw_call.light_shadow_id[light_id]);

		// Spotlight data of the lights
		shader->setUniform("u_light_direction", draw_call.light_direction[light_id]);
		shader->setUniform("u_light_cone_angle", draw_call.light_cone_angle[light_id]);
		shader->setUniform("u_light_cone_decay", draw_call.light_cone_decay[light_id]);

		//do the draw call that renders the mesh into the screen
		draw_call.mesh->render(GL_TRIANGLES);
	}

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glDepthFunc(GL_LESS);
}