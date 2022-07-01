#include "volumetric.h"
#include "texture.h"
#include "shader.h"
#include "camera.h"
#include "fbo.h"


void GTR::sVolumetric_Component::init(const vec2 &screen_size) {
	vol_fbo = new FBO();

	vol_fbo->create(screen_size.x / 4.0f, screen_size.y / 4.0f, 1, GL_RGBA, GL_FLOAT);
}

void GTR::sVolumetric_Component::render(const Camera* cam,
										const vec2& screen_size,
										const CULLING::sSceneCulling* scene_data,
											  ShadowRenderer* shadow_manager,
										 Texture* depth_Tex) {
	if (!enable_volumetric)
		return;

	// TODO: detect screen size change
	
	Shader* shader = Shader::Get("volumetric");
	Mesh* quad = Mesh::getQuad();

	mat4 in_view_proj = cam->viewprojection_matrix;

	// Get light data
	vec3  light_positions[MAX_LIGHT_NUM];
	vec3  light_color[MAX_LIGHT_NUM];
	float light_max_distance[MAX_LIGHT_NUM];
	float light_intensities[MAX_LIGHT_NUM];
	vec3 light_direction[MAX_LIGHT_NUM];
	float light_cone_angle[MAX_LIGHT_NUM];
	float light_cone_decay[MAX_LIGHT_NUM];
	int light_id[MAX_LIGHT_NUM];

	int light_shadow_id[MAX_LIGHT_NUM];
	uint16_t light_count = 0;

	for (; light_count < scene_data->_scene_directional_lights.size(); light_count++) {
		LightEntity* light_ent = scene_data->_scene_directional_lights[light_count];

		light_id[light_count] = light_ent->light_type;
		light_positions[light_count] = light_ent->get_translation();
		light_color[light_count] = light_ent->color;
		light_max_distance[light_count] = light_ent->max_distance;
		light_intensities[light_count] = light_ent->intensity;
		light_shadow_id[light_count] = light_ent->shadow_id;
		light_direction[light_count] = light_ent->get_model().frontVector() * -1.0f;
	}
	for (int i = 0; i < scene_data->_scene_non_directonal_lights.size(); light_count++, i++) {
		LightEntity* light_ent = scene_data->_scene_non_directonal_lights[i];

		light_id[light_count] = light_ent->light_type;
		light_positions[light_count] = light_ent->get_translation();
		light_color[light_count] = light_ent->color;
		light_max_distance[light_count] = light_ent->max_distance;
		light_intensities[light_count] = light_ent->intensity;
		light_shadow_id[light_count] = light_ent->shadow_id;
		light_direction[light_count] = light_ent->get_model().frontVector() * -1.0f;
		light_cone_angle[light_count] = light_ent->cone_angle * 0.0174533f;
		light_cone_decay[light_count] = light_ent->cone_exp_decay;
	}

	vol_fbo->bind();

	shader->enable();
	shader->setUniform3Array("u_light_pos", (float*)light_positions, light_count);
	shader->setUniform3Array("u_light_color", (float*)light_color, light_count);
	shader->setUniform1Array("u_light_shadow_id", (int*)light_shadow_id, light_count);
	shader->setUniform1Array("u_light_max_dist", (float*)light_max_distance, light_count);
	shader->setUniform1Array("u_light_intensities", light_intensities, light_count);
	shader->setUniform3Array("u_light_direction", (float*)light_direction, light_count);
	shader->setUniform1Array("u_light_cone_angle", (float*) light_cone_angle, light_count);
	shader->setUniform1Array("u_light_cone_decay", (float*) light_cone_decay, light_count);
	shader->setUniform1Array("u_light_type", (int*)light_id, light_count);
	shader->setUniform("u_num_lights", light_count);

	shadow_manager->bind_shadows(shader);

	shader->setUniform("u_ray_max_len", max_ray_len);
	shader->setUniform("u_ray_sample_num", sample_size);
	shader->setUniform("u_air_density", air_density);
	shader->setUniform("u_camera_position", cam->eye);
	shader->setUniform("u_camera_nearfar", vec2(cam->near_plane, cam->far_plane));
	shader->setUniform("u_viewprojection", cam->viewprojection_matrix);
	shader->setUniform("u_depth_tex", depth_Tex, 1);

	quad->render(GL_TRIANGLES);
	shader->disable();

	vol_fbo->unbind();

	vol_fbo->color_textures[0]->toViewport();
}

void GTR::sVolumetric_Component::render_imgui() {
	if (ImGui::TreeNode("Volumetric light")) {

		ImGui::Checkbox("Enable Volumetric light", &enable_volumetric);
		ImGui::SliderFloat("Max ray len", &max_ray_len, 100.0f, 1000.0f);
		ImGui::SliderFloat("Air density", &air_density, 0.0001f, 0.05f);
		ImGui::SliderInt("Sample count", &sample_size, 50, 1000);

		ImGui::TreePop();
	}
}