#include "volumetric.h"
#include "texture.h"
#include "shader.h"
#include "camera.h"
#include "fbo.h"

void GTR::sVolumetric_Component::init(const vec2& screen_size) {
	vol_fbo = new FBO();

	vol_fbo->create(screen_size.x / 2.0f, screen_size.y / 2.0f, 1, GL_RGBA, GL_FLOAT);
}

void GTR::sVolumetric_Component::render(const Camera* cam, const vec2& screen_size) {
	Shader* shader = Shader::Get("volumetric");

	mat4 in_view_proj = cam->viewprojection_matrix;

	shader->setUniform("u_vol_samples", sample_size);
	shader->setUniform("u_camera_position", cam->eye);
}