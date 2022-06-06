#include "global_ilumination.h"
#include "renderer.h"

void GTR::sGI_Component::init(Renderer* rend_inst) {
	renderer_instance = rend_inst;
	irradiance_fbo = new FBO();

	irradiance_fbo->create(164, 164, 
		1, 
		GL_RGBA, 
		GL_FLOAT, 
		true);

	probe_position[0] = vec3(0.0f, 64.0f, 0.0f);
}

// Generate probe coefficients
void GTR::sGI_Component::render_to_probe(const uint32_t probe_id) {
	FloatImage cube_views[6];

	Camera render_cam;

	render_cam.setPerspective(90.0f, 1.0f, 0.1f, 1000.0f);

	for(int i = 0; i < 6; i++) {
		// Set camera looking at the direction
		vec3 eye = probe_position[probe_id];
		vec3 front = cubemapFaceNormals[i][2];
		vec3 center = eye + front;
		vec3 up = cubemapFaceNormals[i][1];

		render_cam.lookAt(eye, center, up);

		// Given the scene, and a camara, perform frustum culling
		std::vector<sDrawCall> opaque_draw_calls;
		std::vector<sDrawCall> translucent_draw_calls;
		//renderer_instance->compute_visible_objects(&render_cam, &opaque_draw_calls, &translucent_draw_calls);

		// Bind and enable the fbo
		irradiance_fbo->bind();
		irradiance_fbo->enableSingleBuffer(0);

		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		//glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);

		// First, render the opaque object
		for (uint16_t i = 0; i < opaque_draw_calls.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(opaque_draw_calls[i], &render_cam, vec3(1.0f, 0.0f, 0.0f));
		}

		// then, render the translucnet, and masked objects
		for (uint16_t i = 0; i < translucent_draw_calls.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(translucent_draw_calls[i], &render_cam, vec3(1.0f, 0.0f, 0.0f));
		}

		opaque_draw_calls.clear();
		translucent_draw_calls.clear();

		glDepthFunc(GL_LESS);
		//glDisable(GL_DEPTH_TEST);
		irradiance_fbo->unbind();

		// Store it back to the CPU
		cube_views[i].fromTexture(irradiance_fbo->color_textures[0]);
	}

	// Calculate the harmonics
	harmonics[probe_id] = computeSH(cube_views);
	//std::cout << harmonics[probe_id].coeffs[0].x << " " << harmonics[probe_id].coeffs[0].y << std::endl;
	//std::cout << "Calculated probe" << std::endl;
}

void GTR::sGI_Component::render_imgui() {
	ImGui::SliderFloat3("Probe 1 position", (float*)&probe_position[0], -100, 100);

	if (ImGui::Button("Store to probe")) {
		render_to_probe(0);
	}
}

void GTR::sGI_Component::debug_render_probe(const uint32_t probe_id, const float radius, Camera* cam) {
	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj", false);
	Shader* shader = Shader::Get("probe");

	mat4 model;
	model.setTranslation(probe_position[probe_id].x, probe_position[probe_id].y, probe_position[probe_id].z);
	model.scale(radius, radius, radius);

	shader->enable();

	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", cam->viewprojection_matrix);
	shader->setUniform("u_camera_position", cam->eye);
	shader->setUniform3Array("u_sphere_coeffs", (float*) harmonics[probe_id].coeffs, 9);

	sphere->render(GL_TRIANGLES);

	shader->disable();
}