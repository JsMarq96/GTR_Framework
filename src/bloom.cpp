#include "bloom.h"

void GTR::SBloom_Component::init(const vec2 size) {
	bloom_FBO = new FBO();
	bloom_FBO->create(size.x, size.y, 1, GL_RGBA, GL_FLOAT);
}

Texture* GTR::SBloom_Component::bloom_pass(Texture* text) {
	if (!enabled_bloom)
		return text;

	text->generateMipmaps();
	int numLevels = (log2(max(text->width, text->height)));

	Shader* shader = Shader::Get("bloom");
	Mesh* quad = Mesh::getQuad();

	bloom_FBO->bind();
	bloom_FBO->enableSingleBuffer(0);
	glClearColor(0.1, 0.1, 0.1, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	shader->enable();

	shader->setUniform("u_color_tex", text, 0);
	shader->setUniform("u_max_step_LOD", numLevels - (numLevels - max_LOD) );
	shader->setUniform("u_bloom_steps_num", min(bloom_steps, numLevels));
	shader->setUniform("u_min_step", min_step_size);

	quad->render(GL_TRIANGLES);

	shader->disable();

	bloom_FBO->unbind();

	return bloom_FBO->color_textures[0];
}


void GTR::SBloom_Component::render_imgui() {
	if (ImGui::TreeNode("Bloom")) {

		ImGui::Checkbox("Enable bloom", &enabled_bloom);
	
		ImGui::SliderInt("# of steps", &bloom_steps, 2, 15);
		ImGui::SliderInt("MIN steps", &min_step_size, 1, 15);
		ImGui::SliderInt("Max LOD", &max_LOD, 1, 20);

		ImGui::TreePop();
	}
}
