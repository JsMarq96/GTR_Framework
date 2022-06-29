#include "reflections.h"
#include "renderer.h"


void GTR::sReflections_Component::init(Renderer* rend_inst) {
	renderer_instance = rend_inst;
	memset(is_in_use, false, sizeof(sReflections_Component::is_in_use));

	ref_fbo = new FBO();
}

void GTR::sReflections_Component::clean() {

}

int GTR::sReflections_Component::init_probe() {
	int i = 0;
	for (; i < MAX_REF_PROBE_COUNT; i++) {
		if (is_in_use[i])
			continue;
		break;
	}

	probe_cubemap[i] = new Texture();
	probe_cubemap[i]->createCubemap(512, 512,
		NULL, 
		GL_RGB, 
		GL_UNSIGNED_INT, 
		true);
	is_in_use[i] = true;
	probe_position[i] = vec3(0.0f, 100.0f, 0.0f);

	return i;
}


void GTR::sReflections_Component::capture_probe(const std::vector<BaseEntity*>& entity_list, 
												const int probe_id) {
	Camera render_cam;

	render_cam.setPerspective(90.0f, 1.0f, 0.1f, 1000.0f);

	for (int i = 0; i < 6; i++) {
		// Set camera looking at the direction
		vec3 eye = probe_position[probe_id];
		vec3 front = cubemapFaceNormals[i][2];
		vec3 center = eye + front;
		vec3 up = cubemapFaceNormals[i][1];

		render_cam.lookAt(eye, center, up);

		// Given the scene, and a camara, perform frustum culling
		CULLING::sSceneCulling culling_result;
		// Note: maybe start from scene culling is a bit of a waste
		CULLING::frustrum_culling(entity_list, &culling_result, &render_cam);

		// Bind and enable the fbo
		ref_fbo->setTexture(probe_cubemap[probe_id], i);
		ref_fbo->bind();
		ref_fbo->enableSingleBuffer(0);

		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// First, render the opaque object
		for (uint16_t i = 0; i < culling_result._opaque_objects.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(culling_result._opaque_objects[i], &render_cam, renderer_instance->current_scene->ambient_light, false, false);
		}

		// then, render the translucnet, and masked objects
		for (uint16_t i = 0; i < culling_result._translucent_objects.size(); i++) {
			renderer_instance->forwardSingleRenderDrawCall(culling_result._translucent_objects[i], &render_cam, renderer_instance->current_scene->ambient_light, false, false);
		}

		culling_result.clear();

		glDepthFunc(GL_LESS);
		ref_fbo->unbind();
	}

	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	probe_cubemap[probe_id]->generateMipmaps();
}


void GTR::sReflections_Component::capture_all_probes(const std::vector<BaseEntity*>& entity_list) {
	for (int i = 0; i < MAX_REF_PROBE_COUNT; i++) {
		if (!is_in_use[i])
			continue;
		capture_probe(entity_list, i);
	}
}


void GTR::sReflections_Component::render_probe(Camera& cam, const int probe_id) {
	Mesh* sphere = Mesh::Get("data/meshes/sphere.obj", false);
	Shader* shader = Shader::Get("ref_probe");

	mat4 model;
	model.setTranslation(probe_position[probe_id].x, probe_position[probe_id].y, probe_position[probe_id].z);
	model.scale(10.0f, 10.0f, 10.0f);

	shader->enable();

	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", cam.viewprojection_matrix);
	shader->setUniform("u_camera_position", cam.eye);
	shader->setUniform("u_skybox_texture", probe_cubemap[probe_id], 1);

	sphere->render(GL_TRIANGLES);

	shader->disable();
}

void GTR::sReflections_Component::render_probes(Camera& cam) {
	if (!debug_render)
		return;
	for (int i = 0; i < MAX_REF_PROBE_COUNT; i++) {
		if (!is_in_use[i])
			continue;

		render_probe(cam, i);
	}
}

void GTR::sReflections_Component::debug_imgui() {
	if (ImGui::TreeNode("Reflection probes")) {
		for (int i = 0; i < MAX_REF_PROBE_COUNT; i++) {
			if (!is_in_use[i])
				continue;
			ImGui::SliderFloat3("Probe pos: ", (float*) &probe_position[i], -1000.0f, 1000.0f);
		}

		if (ImGui::Button("Add probe")) {
			init_probe();
		}
		if (ImGui::Button("Calculate reflections")) {
			capture_all_probes(*renderer_instance->entity_list);
		}
		ImGui::Checkbox("Show ref probes ", &debug_render);
		ImGui::Checkbox("Enable reflections ", &enable_reflections);
		ImGui::TreePop();
	}
}