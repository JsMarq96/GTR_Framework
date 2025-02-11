#include "renderer.h"

// Defintion of the Deferred rendering functions

void GTR::Renderer::renderDeferredLightVolumes(CULLING::sSceneCulling *scene_data) {
	// Set depth test as only max, without writting to it
	glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_GREATER);
	glDepthMask(false);
	
	// Face culling configureg to only render backface, to avoid overdraw
	glEnable(GL_CULL_FACE);
	//glFrontFace(GL_CW);

	// Enable additive blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);


	Mesh* sphere_mesh = Mesh::Get("data/meshes/sphere.obj", false);
	Mesh* cone_mesh = Mesh::Get("data/meshes/cone.obj", false);
	Shader* shader = Shader::Get("deferred_lightpass");
	assert(glGetError() == GL_NO_ERROR);

	shader->enable();

	shader->setUniform("u_albedo_tex", deferred_gbuffer->color_textures[0], 0);
	shader->setUniform("u_normal_occ_tex", deferred_gbuffer->color_textures[1], 1);
	shader->setUniform("u_met_rough_tex", deferred_gbuffer->color_textures[2], 2);
	shader->setUniform("u_depth_tex", deferred_gbuffer->depth_texture, 3);
	shader->setUniform("u_screen_res", vec2(1.0f / (deferred_gbuffer->depth_texture->width), 1.0f / (deferred_gbuffer->depth_texture->height)));
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_skybox_texture", skybox_texture, 9);

	// Set the shadowmap
	shadowmap_renderer.bind_shadows(shader);

	shader->setUniform("u_time", (float) getTime());

	// TODO: Render point lights
	for (uint16_t i = 0; i < scene_data->_scene_non_directonal_lights.size(); i++) {
		LightEntity* light = scene_data->_scene_non_directonal_lights[i];

		mat4 model, scaling, rotation;

		vec3 light_pos = light->get_translation();
		model.setTranslation(light_pos.x, light_pos.y, light_pos.z);

		float angles[3];

		light->get_model().getXYZ(angles);

		float light_size = light->max_distance;
		if (light->light_type == POINT_LIGHT) {
			scaling.setScale(light_size, light_size, light_size);
		}
		else if (light->light_type == SPOT_LIGHT) {
			// Cone radius = cone height * tan(cone angle)
			float radius = light_size * tanf(light->cone_angle * DEG2RAD);
			scaling.scale(radius, radius, light_size);
		}

		rotation.rotate(angles[0], vec3(1.0f, 0.0f, 0.0f));
		rotation.rotate(angles[1], vec3(0.0f, 1.0f, 0.0f));
		rotation.rotate(angles[2], vec3(0.0f, 0.0f, 1.0f));

		model = scaling * rotation * model;

		// Upload light data
		// Common data of the lights
		shader->setUniform("u_light_pos", model.getTranslation());
		shader->setUniform("u_light_color", light->color);
		shader->setUniform("u_light_type", light->light_type);
		shader->setUniform("u_light_shadow_id", (int)light->shadow_id);
		shader->setUniform("u_light_max_dist", light_size);
		shader->setUniform("u_light_intensities", light->intensity);
		shader->setUniform("u_light_direction", light->get_model().frontVector() * -1.0f);
		shader->setUniform("u_light_cone_angle", (float)(light->cone_angle * 0.0174533));
		shader->setUniform("u_light_cone_decay", light->cone_exp_decay);

		shader->setUniform("u_model", model);

		//do the draw call that renders the mesh into the screen
		if (light->light_type == POINT_LIGHT) {
			sphere_mesh->render(GL_TRIANGLES);
		} else if (light->light_type == SPOT_LIGHT) {
			
			cone_mesh->render(GL_TRIANGLES);
		}
	}

	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glFrontFace(GL_CCW);
	glDepthFunc(GL_LESS);
	glDepthMask(true);

	if (!render_light_volumes) {
		return;
	}

	glDepthMask(false);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	Shader *shaderp = Shader::Get("flat");
	assert(glGetError() == GL_NO_ERROR);

	shaderp->enable();

	shaderp->setUniform("u_viewprojection", camera->viewprojection_matrix);

	// TODO: Render point lights
	for (uint16_t i = 0; i < scene_data->_scene_non_directonal_lights.size(); i++) {
		LightEntity* light = scene_data->_scene_non_directonal_lights[i];

		mat4 model, scaling, rotation;

		float angles[3];

		light->get_model().getXYZ(angles);

		vec3 light_pos = light->get_translation();
		model.setTranslation(light_pos.x, light_pos.y, light_pos.z);

		float light_size = light->max_distance;
		if (light->light_type == POINT_LIGHT) {
			scaling.setScale(light_size, light_size, light_size);
		} else if (light->light_type == SPOT_LIGHT) {
			// Cone radius = cone height * tan(cone angle)
			float radius = light_size * tanf(light->cone_angle * DEG2RAD);
			scaling.scale( radius, radius, light_size);
		}
		
		rotation.rotate(angles[0], vec3(1.0f, 0.0f, 0.0f));
		rotation.rotate(angles[1], vec3(0.0f, 1.0f, 0.0f));
		rotation.rotate(angles[2], vec3(0.0f, 0.0f, 1.0f));

		model = rotation * scaling * model;

		// Upload light data
		// Common data of the lights
		shaderp->setUniform("u_color", vec4(light->color, 1.0f));

		shaderp->setUniform("u_model", model);

		//do the draw call that renders the mesh into the screen
		if (light->light_type == POINT_LIGHT) {
			sphere_mesh->render(GL_TRIANGLES);
		}
		else if (light->light_type == SPOT_LIGHT) {
			cone_mesh->render(GL_TRIANGLES);
		}
	}

	shaderp->disable();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDepthMask(true);
}