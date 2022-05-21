#define once

#include "includes.h"
#include "scene.h"
#include "mesh.h"
#include "fbo.h"
#include "shader.h"
#include "application.h"


namespace GTR {
#define AO_SAMPLE_SIZE 64

	struct SSAO_Component {
		FBO* ao_fbo;

		vec3 _samples[AO_SAMPLE_SIZE];

		Mesh* quad_mesh;

		Texture* noise_tex;

		void init() {
			ao_fbo = new FBO();

			ao_fbo->create(Application::instance->window_width / 1.50f, Application::instance->window_height / 1.50f,
				2,
				GL_LUMINANCE,
				GL_UNSIGNED_BYTE,
				false);

			// Create random samples
			srand(time(NULL));

			float rand_max_f = (float)RAND_MAX;
			for (uint32_t i = 0; i < AO_SAMPLE_SIZE; i++) {
				float x = ((float)rand()) / rand_max_f, z = ((float)rand()) / rand_max_f;
				x *= 2.0f, z *= 2.0f;
				x -= 1.0f, z -= 1.0f;
				float y = sqrt(abs(1.0f - (x * x) - (z * z)));

				vec3 point = vec3(x, y, z);
				point = lerp(vec3(0.0f, 0.0f, 0.0f), point, ((float)rand()) / rand_max_f);

				_samples[i] = point;
			}

			quad_mesh = Mesh::getQuad();

			// Create noise texture
			Image noise_img;
			noise_img.fromScreen(8, 1);

			for (uint8_t i = 0; i < 8; i++) {
				//noise_img.setPixel(i, 0, Color({ ((float)rand()) / rand_max_f, ((float)rand()) / rand_max_f, 0.0f, 1.0f }));
			}

			noise_tex = new Texture(&noise_img);

			noise_img.clear();
		}


		Texture* compute_AO(Texture* depth_tex, Texture* normal_tex, const Camera *camera) {
			ao_fbo->bind();

			// Clean last frame
			ao_fbo->enableSingleBuffer(0);
			glClearColor(0.1, 0.1, 0.1, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// COmpute the AO ===========
			Shader* shader = Shader::Get("ao_pass");

			shader->enable();

			shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			shader->setUniform3Array("u_random_point", (float*) _samples, AO_SAMPLE_SIZE);
			shader->setUniform("u_normal_occ_tex", normal_tex, 0);
			shader->setUniform("u_depth_tex", depth_tex, 1);

			quad_mesh->render(GL_TRIANGLES);

			shader->disable();

			// Blur AO ======
			ao_fbo->enableSingleBuffer(1);
			glClearColor(0.1, 0.1, 0.1, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shader = Shader::Get("blur");

			shader->enable();
			shader->setUniform("u_to_blur_tex", ao_fbo->color_textures[0], 0);
			quad_mesh->render(GL_TRIANGLES);
			shader->disable();

			ao_fbo->unbind();

			return ao_fbo->color_textures[1];
		}
	};
};