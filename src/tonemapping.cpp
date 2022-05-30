#include "tonemapping.h"
#include "texture.h"

namespace GTR {

	const char* mapping_shader[2] = { "perception_tonemapper", "uncharted_tonemapper" };

	void Tonemapping_Component::init() {
		fbo = new FBO();
		fbo->create(Application::instance->window_width, Application::instance->window_height,
			1,
			GL_RGBA,
			GL_FLOAT,
			true);
	}

	 Texture* Tonemapping_Component::pass(Texture* prev_pass) {
		fbo->bind();

		fbo->enableSingleBuffer(0);

		glDisable(GL_DEPTH_TEST);
		Mesh* quad = Mesh::getQuad();

		Shader* shader = Shader::Get(mapping_shader[current_mapper]);

		shader->enable();
		shader->setUniform("u_albedo_tex", prev_pass, 0);
		quad->render(GL_TRIANGLES);
		shader->disable();

		glEnable(GL_DEPTH_TEST);

		fbo->unbind();

		return fbo->color_textures[0];
	}
};