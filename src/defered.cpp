#include "renderer.h"

// Defintion of the Deferred rendering functions

void GTR::Renderer::_init_deferred_renderer() {
	deferred_gbuffer = new FBO();

	deferred_gbuffer->create(Application::instance->window_width, Application::instance->window_height, 
		4,
		GL_RGBA, 
		GL_FLOAT,  // Enought..? Now, yes
		true);
}


inline void GTR::Renderer::forwardOpacyRenderDrawCall(const sDrawCall& draw_call, const Scene* scene) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;

	//select the blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("forward_singlepass_pbr");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	// Upload light data
	// Common data of the lights
	shader->setUniform("u_ambient_light", scene->ambient_light);
	shader->setUniform3Array("u_light_pos", (float*)draw_call.light_positions, draw_call.light_count);
	shader->setUniform3Array("u_light_color", (float*)draw_call.light_color, draw_call.light_count);
	shader->setUniform1Array("u_light_type", (int*)draw_call.light_type, draw_call.light_count);
	shader->setUniform1Array("u_light_shadow_id", (int*)draw_call.light_shadow_id, draw_call.light_count);
	shader->setUniform1Array("u_light_max_dist", (float*)draw_call.light_max_distance, draw_call.light_count);
	shader->setUniform1Array("u_light_intensities", draw_call.light_intensities, draw_call.light_count);
	shader->setUniform("u_num_lights", draw_call.light_count);
	shader->setUniform("u_material_type", draw_call.pbr_structure);

	// Spotlight data of the lights
	shader->setUniform3Array("u_light_direction", (float*)draw_call.light_direction, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_angle", (float*)draw_call.light_cone_angle, draw_call.light_count);
	shader->setUniform1Array("u_light_cone_decay", (float*)draw_call.light_cone_decay, draw_call.light_count);

	//upload uniforms
	shader->setUniform("u_viewprojection", draw_call.camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", draw_call.camera->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	// Material properties
	shader->setUniform("u_color", draw_call.material->color);
	int enabled_texteres = bind_textures(draw_call.material, shader);

	shader->setUniform("u_enabled_texteres", enabled_texteres);

	// Set the shadowmap
	shadowmap_renderer.bind_shadows(shader);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	draw_call.mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}

void GTR::Renderer::deferredRenderDecal(const DecalEntity* ent, Camera *cam, Texture* depth) {
	Mesh cube;
	cube.createCube();

	Shader* shader = Shader::Get("deferred_decals");

	shader->enable();
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_model", ent->model);
	shader->setUniform("u_color_tex", ent->color_tex, 1);
	shader->setUniform("u_depth_tex", depth, 2);
	shader->setUniform("u_iRes", vec2(Application::instance->window_width, Application::instance->window_height));
	
	cube.render(GL_TRIANGLES);

	shader->disable();
}

void GTR::Renderer::deferredRenderScene(const Scene* scene, Camera* camera, FBO* resulting_fbo, CULLING::sSceneCulling* scene_data) {
	deferred_gbuffer->bind();

	// Clean last frame
	deferred_gbuffer->enableSingleBuffer(0);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	deferred_gbuffer->enableSingleBuffer(1);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	deferred_gbuffer->enableSingleBuffer(2);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	deferred_gbuffer->enableSingleBuffer(3);
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	deferred_gbuffer->enableAllBuffers();

	// Note, only render the opaque drawcalls

	//std::cout << _opaque_objects.size() << std::endl;
	for (uint16_t i = 0; i < scene_data->_opaque_objects.size(); i++) {
		renderDeferredPlainDrawCall(scene_data->_opaque_objects[i], scene);
	}

	if (scene->decals.size() > 0) {
		// Copy depth bufffer
		deferred_gbuffer->unbind();
		if (depth_decal_fbo == NULL) {
			depth_decal_fbo = new FBO();
			depth_decal_fbo->create(deferred_gbuffer->width, deferred_gbuffer->height, 4, GL_RGBA, GL_FLOAT, true);
		}

		//depth_decal_fbo->bind();
		deferred_gbuffer->color_textures[0]->copyTo(depth_decal_fbo->color_textures[0]);
		deferred_gbuffer->color_textures[1]->copyTo(depth_decal_fbo->color_textures[1]);
		deferred_gbuffer->color_textures[2]->copyTo(depth_decal_fbo->color_textures[2]);
		deferred_gbuffer->color_textures[3]->copyTo(depth_decal_fbo->color_textures[3]);
		
		
		depth_decal_fbo->bind();
		deferred_gbuffer->depth_texture->copyTo(NULL);
		depth_decal_fbo->unbind();

		deferred_gbuffer->bind();

		glEnable(GL_DEPTH_TEST);
		glDepthMask(false);
		glDepthFunc(GL_LEQUAL);

		glEnable(GL_CULL_FACE);
		//glFrontFace(GL_CW);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColorMask(true, true, true, false);

		deferred_gbuffer->enableAllBuffers();

		Mesh cube;
		cube.createCube();

		Shader* shader = Shader::Get("deferred_decals");

		shader->enable();

		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_iRes", vec2(1.0f / (float)Application::instance->window_width, 1.0f/ (float) Application::instance->window_height));
		shader->setUniform("u_depth_tex", depth_decal_fbo->depth_texture, 3);


		for (int i = 0; i < scene->decals.size(); i++) {
			
			shader->setUniform("u_model", scene->decals[i]->model);
			shader->setUniform("u_color_tex", scene->decals[i]->color_tex, 1);
			cube.render(GL_TRIANGLES);
		}
		
		shader->disable();

		
		glColorMask(true, true, true, true);
		glDepthFunc(GL_LEQUAL);
		glFrontFace(GL_CCW);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glDepthMask(true);
	}

	deferred_gbuffer->unbind();

	switch (deferred_output) {
	case WORLD_POS:
	case RESULT:
		renderDefferredPass(scene, scene_data);
		break;
	case COLOR:
		deferred_gbuffer->color_textures[0]->toViewport();
		break;
	case NORMAL:
		deferred_gbuffer->color_textures[1]->toViewport();
		break;
	case MATERIAL:
		deferred_gbuffer->color_textures[2]->toViewport();
		break;
	case AMBIENT_OCCLUSION:
		ao_component.compute_AO(deferred_gbuffer->depth_texture, deferred_gbuffer->color_textures[1], camera);
		ao_component.ao_fbo->color_textures[0]->toViewport();
		break;
	case AMBIENT_OCCLUSION_BLUR:
		ao_component.compute_AO(deferred_gbuffer->depth_texture, deferred_gbuffer->color_textures[1], camera);
		ao_component.ao_fbo->color_textures[1]->toViewport();
		break;
	case EMMISIVE:
		deferred_gbuffer->color_textures[3]->toViewport();
		break;
	case DEPTH:
		Shader* depth = Shader::Get("depth");
		depth->enable();
		depth->setUniform("u_camera_nearfar", vec2(0.1, scene->main_camera.far_plane));
		deferred_gbuffer->depth_texture->toViewport(depth);
		break;
	};
} 

void GTR::Renderer::renderDefferredPass(const Scene* scene, CULLING::sSceneCulling * scene_data) {
	// Compute AO
	Texture* ao_tex = NULL;
	if (use_ssao) {
		ao_tex = ao_component.compute_AO(deferred_gbuffer->depth_texture, deferred_gbuffer->color_textures[1], camera);
	} else {
		ao_tex = Texture::getWhiteTexture();
	}

	Shader* shader_pass = (deferred_output == WORLD_POS) ? Shader::Get("deferred_world_pos") : Shader::Get("deferred_pass");

	final_illumination_fbo->bind();

	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	assert(shader_pass != NULL && "Error null shader");
	shader_pass->enable();

	shader_pass->setUniform("u_albedo_tex", deferred_gbuffer->color_textures[0], 0);
	shader_pass->setUniform("u_normal_occ_tex", deferred_gbuffer->color_textures[1], 1);
	shader_pass->setUniform("u_met_rough_tex", deferred_gbuffer->color_textures[2], 2);
	shader_pass->setUniform("u_emmisive_tex", deferred_gbuffer->color_textures[3], 3);
	shader_pass->setUniform("u_depth_tex", deferred_gbuffer->depth_texture, 4);
	shader_pass->setUniform("u_ambient_occlusion_tex", ao_tex, 5);


	shader_pass->setUniform("u_camera_nearfar", vec2(0.1, camera->far_plane));
	shader_pass->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader_pass->setUniform("u_camera_position", camera->eye);
	shader_pass->setUniform("u_camera_near_far", vec2(camera->near_plane, camera->far_plane));
	float t = getTime();
	shader_pass->setUniform("u_time", t);

	vec3  light_positions[MAX_LIGHT_NUM];
	vec3  light_color[MAX_LIGHT_NUM];
	float light_max_distance[MAX_LIGHT_NUM];
	float light_intensities[MAX_LIGHT_NUM];
	vec3 light_direction[MAX_LIGHT_NUM];

	int light_shadow_id[MAX_LIGHT_NUM];
	uint16_t light_count = 0;

	for (; light_count < scene_data->_scene_directional_lights.size(); light_count++) {
		LightEntity* light_ent = scene_data->_scene_directional_lights[light_count];

		light_positions[light_count] = light_ent->get_translation();
		light_color[light_count] = light_ent->color;
		light_max_distance[light_count] = light_ent->max_distance;
		light_intensities[light_count] = light_ent->intensity;
		light_shadow_id[light_count] = light_ent->shadow_id;
		light_direction[light_count] = light_ent->get_model().frontVector() * -1.0f;
	}

	shader_pass->setUniform("u_ambient_light", scene->ambient_light);
	shader_pass->setUniform3Array("u_light_pos", (float*)light_positions, light_count);
	shader_pass->setUniform3Array("u_light_color", (float*)light_color, light_count);
	shader_pass->setUniform1Array("u_light_shadow_id", (int*)light_shadow_id, light_count);
	shader_pass->setUniform1Array("u_light_max_dist", (float*)light_max_distance, light_count);
	shader_pass->setUniform1Array("u_light_intensities", light_intensities, light_count);
	shader_pass->setUniform3Array("u_light_direction", (float*)light_direction, light_count);
	shader_pass->setUniform("u_num_lights", light_count);

	irradiance_component.bind_GI(shader_pass);

	if (reflections_component.enable_reflections) {
		reflections_component.bind_reflections(camera->eye, shader_pass);
	} else {
		shader_pass->setUniform("u_skybox_texture", skybox_texture, 9);
	}
	shader_pass->setUniform("u_skybox_env_texture", skybox_texture, 9);
	shadowmap_renderer.bind_shadows(shader_pass);

	Mesh* quad = Mesh::getQuad();
	
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	quad->render(GL_TRIANGLES);

	assert(glGetError() == GL_NO_ERROR);
	shader_pass->disable();

	// Copy the depth shader to the illumination shader
	deferred_gbuffer->depth_texture->copyTo(NULL);

	renderDeferredLightVolumes(scene_data);

	// Avoid the translucent to write to the depth buffer
	//glDepthMask(false);
	for (uint16_t i = 0; i < scene_data->_translucent_objects.size(); i++) {
		forwardOpacyRenderDrawCall(scene_data->_translucent_objects[i], scene);
	}
	//glDepthMask(true);

	final_illumination_fbo->unbind();

	//tonemapping_fbo->bind();
	//tonemappingPass();
	//tonemapping_fbo->unbind();

	//tonemapping_fbo->color_textures[0]->toViewport();
}

void GTR::Renderer::tonemappingPass() {
	glDisable(GL_DEPTH_TEST);
	Mesh* quad = Mesh::getQuad();

	Shader* shader = Shader::Get("tonemapping_pass");

	shader->enable();

	shader->setUniform("u_albedo_tex", final_illumination_fbo->color_textures[0], 0);

	quad->render(GL_TRIANGLES);

	shader->disable();
	glEnable(GL_DEPTH_TEST);
}

void GTR:: Renderer::renderDeferredPlainDrawCall(const sDrawCall& draw_call, const Scene* scene) {
	//in case there is nothing to do
	if (!draw_call.mesh || !draw_call.mesh->getNumVertices() || !draw_call.material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;

	glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if (draw_call.material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
	assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	shader = Shader::Get("deferred_plane_opaque");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();
	camera = draw_call.camera;

	//upload uniforms
	shader->setUniform("u_viewprojection", draw_call.camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", draw_call.camera->eye);
	shader->setUniform("u_model", draw_call.model);
	float t = getTime();
	shader->setUniform("u_time", t);

	// Material properties
	shader->setUniform("u_color", draw_call.material->color);
	bind_textures(draw_call.material, shader);
	shader->setUniform("u_material_type", draw_call.pbr_structure);

	//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
	shader->setUniform("u_alpha_cutoff", draw_call.material->alpha_mode == GTR::eAlphaMode::MASK ? draw_call.material->alpha_cutoff : 0);

	//do the draw call that renders the mesh into the screen
	draw_call.mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}