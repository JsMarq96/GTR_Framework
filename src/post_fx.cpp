#include "post_fx.h"

void GTR::sPostFX_Component::init(const vec2& size) {
	post_fx_1 = new FBO();
	post_fx_2 = new FBO();

	post_fx_2->create(size.x, size.y, 1, GL_RGBA, GL_FLOAT);
	post_fx_1->create(size.x, size.y, 1, GL_RGBA, GL_FLOAT);

	memset(enabled, false, sizeof(enabled));
}

Texture* GTR::sPostFX_Component::add_postFX(Texture* text) {
	if (!enable_postFX)
		return text;

	FBO* fbo_list[2] = {post_fx_1, post_fx_2};

	int fbo_index = 0;

	Mesh *quad = Mesh::getQuad();

	// Copy to start the poing pong
	post_fx_2->bind();
	text->copyTo(NULL);
	post_fx_2->unbind();

	for (int i = 0; i < POSTFX_COUNT; i++) {
		if (!enabled[i])
			continue;

		FBO* curr_fbo = fbo_list[fbo_index];
		FBO* prev_fbo = fbo_list[(fbo_index == 0) ? 1 : 0];

		curr_fbo->bind();

		curr_fbo->enableSingleBuffer(0);
		glClearColor(0.1, 0.1, 0.1, 1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		Shader* shader = Shader::Get(shaders[i]);
		shader->enable();

		shader->setUniform("u_color_tex", prev_fbo->color_textures[0], 1);

		quad->render(GL_TRIANGLES);

		shader->disable();

		curr_fbo->unbind();

		// Ping pong
		fbo_index = (fbo_index == 0) ? 1 : 0;
	}
}