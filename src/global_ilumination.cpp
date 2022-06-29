#include "global_ilumination.h"

#include "includes.h"
#include "renderer.h"
#include "frusturm_culling.h"

void GTR::sGI_Component::init(Renderer* rend_inst) {
	renderer_instance = rend_inst;
	irradiance_fbo = new FBO();

	irradiance_fbo->create(64, 64, 
		1, 
		GL_RGBA, 
		GL_FLOAT, 
		true);

	//probe_position[0] = vec3(00.0f, 020.0f, 0.0f);
	//used_probe[0] = true;
	//probe_position[1] = vec3(0, 50.0, 0);
	create_probe_area(origin_probe_position, probe_area_size, probe_distnace_radius);
}

void GTR::sGI_Component::create_probe_area(const vec3 postion, const vec3 size, const float probe_radius_size) {
	
	bool has_size_changed = old_probe_size.x != size.x ||
							old_probe_size.y != size.y ||
							old_probe_size.z != size.z;
	if (has_size_changed) {
		if (probe_pos != NULL) {
			free(probe_pos);
			free(harmonics);
		}

		probe_pos = (vec3*)malloc(sizeof(vec3) * size.x * size.y * size.z);
		probe_size = size.x * size.y * size.z;
		harmonics = (SphericalHarmonics*)malloc(sizeof(SphericalHarmonics) * size.x * size.y * size.z);
	}

	for (float y = 0.0f; y < size.y; y++ ) {
		for (float z = 0.0f; z < size.z; z++) {
			for (float x = 0.0f; x < size.x; x++) {
				//

					// Store the current ir probe
				int probe_index = x + y * size.x + z * size.x * size.y;
				// Store the current ir probe
				probe_pos[probe_index] = postion + vec3(x, y, z) * probe_radius_size;
			}
		}
	}

	origin_probe_position = postion;
	old_probe_size = size;

	if (has_size_changed) {
		if (probe_texture != NULL) {
			delete probe_texture;
		}
		probe_texture = new Texture(9, probe_size, GL_RGB, GL_FLOAT);
	}
}

void GTR::sGI_Component::compute_all_probes(const std::vector<BaseEntity*> &entity_list) {
	for (int i = 0; i < probe_size; i++) {
		render_to_probe(entity_list, i);
	}
	probe_texture->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	probe_texture->unbind();
	probe_texture->upload(GL_RGB, GL_FLOAT, false, (uint8_t*) harmonics);
}


// Generate probe coefficients
void GTR::sGI_Component::render_to_probe(const std::vector<BaseEntity*> &entity_list, const uint32_t probe_id) {
	FloatImage cube_views[6];

	Camera render_cam;

	render_cam.setPerspective(90.0f, 1.0f, 0.1f, 1000.0f);

	for(int i = 0; i < 6; i++) {
		// Set camera looking at the direction
		vec3 eye = probe_pos[probe_id];
		vec3 front = cubemapFaceNormals[i][2];
		vec3 center = eye + front;
		vec3 up = cubemapFaceNormals[i][1];

		render_cam.lookAt(eye, center, up);

		// Given the scene, and a camara, perform frustum culling
		CULLING::sSceneCulling culling_result;
		// Note: maybe start from scene culling is a bit of a waste
		CULLING::frustrum_culling(entity_list, &culling_result, &render_cam);

		// Bind and enable the fbo
		irradiance_fbo->bind();
		irradiance_fbo->enableSingleBuffer(0);

		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//glEnable(GL_DEPTH_TEST);
		//glDepthFunc(GL_ALWAYS);

		// First, render the opaque object
		for (uint16_t i = 0; i < culling_result._opaque_objects.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(culling_result._opaque_objects[i], &render_cam, vec3(0.0f, 0.0f, 0.0f), false);
		}

		// then, render the translucnet, and masked objects
		for (uint16_t i = 0; i < culling_result._translucent_objects.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(culling_result._translucent_objects[i], &render_cam, vec3(0.0f, 0.0f, 0.0f), false);
		}

		culling_result.clear();

		glDepthFunc(GL_LESS);
		//glDisable(GL_DEPTH_TEST);
		irradiance_fbo->unbind();

		// Store it back to the CPU
		cube_views[i].fromTexture(irradiance_fbo->color_textures[0]);
	}

	// Calculate the harmonics
	harmonics[probe_id] = computeSH(cube_views);
}

void GTR::sGI_Component::render_imgui() {
	vec3 pos = origin_probe_position;

	bool has_updated = ImGui::SliderFloat3("Probe area position",(float*) &origin_probe_position, -3000.0f, 3000.0f);
	bool has_updated2 = ImGui::SliderFloat3("Probe area size", (float*) &probe_area_size, 1.0f, 100.0f);
	bool has_updated3 = ImGui::SliderFloat("Probe distance", &probe_distnace_radius, 100.0f, 1000.0f);
	//has_updated = has_updated && has_updated2;
	//has_updated = has_updated && has_updated3;

	if (has_updated || has_updated2 || has_updated3) {
		create_probe_area(origin_probe_position, probe_area_size, probe_distnace_radius);
	}

	if (ImGui::Button("Compute GI")) {
		compute_all_probes(*renderer_instance->entity_list);
	}
}

void GTR::sGI_Component::debug_render_probe(const uint32_t probe_id, const float radius, Camera* cam) {
	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj", false);
	Shader* shader = Shader::Get("probe");

	mat4 model;
	model.setTranslation(probe_pos[probe_id].x, probe_pos[probe_id].y, probe_pos[probe_id].z);
	model.scale(radius, radius, radius);

	shader->enable();

	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", cam->viewprojection_matrix);
	shader->setUniform("u_camera_position", cam->eye);
	shader->setUniform3Array("u_sphere_coeffs", (float*) harmonics[probe_id].coeffs, 9);

	sphere->render(GL_TRIANGLES);

	shader->disable();
}

void GTR::sGI_Component::debug_render_all_probes(const float radius, Camera* cam) {
	if (!debug_show_spheres)
		return;
	for (int i = 0; i < probe_size; i++) {
		debug_render_probe(i, radius, cam);
	}
}