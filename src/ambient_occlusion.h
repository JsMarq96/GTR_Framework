#pragma once

#include "includes.h"
#include "scene.h"
#include "mesh.h"
#include "fbo.h"
#include "shader.h"
#include "application.h"


namespace GTR {
#define AO_SAMPLE_SIZE 64
#define AO_RANDOM_SIZE 16

	inline uint32_t upload_raw_texture(const vec3* data, const int size) {
		uint32_t texture;

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, int(size/2.0f), int(size/2.0f), 0, GL_RGB, GL_FLOAT, (float*)data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		return texture;
	}

	inline uint32_t upload_raw_texture_square(const float* data, const int size) {
		uint32_t texture;

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, int(size/4.0f), int(size / 4.0f), 0, GL_LUMINANCE, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		return texture;
	}

	struct SSAO_Component {
		FBO* ao_fbo;

		vec3 _samples[AO_SAMPLE_SIZE];

		Mesh* quad_mesh;

		float ao_radius = 10.0f;

		uint32_t custom_noise_tex = 0;

		void init() {
			ao_fbo = new FBO();

			ao_fbo->create(Application::instance->window_width / 1.0f, Application::instance->window_height / 1.0f,
				2,
				GL_RGB,
				GL_UNSIGNED_BYTE,
				false);

			// Create random samples
			srand(time(NULL));

			float rand_max_f = (float)RAND_MAX;
			for (uint32_t i = 0; i < AO_SAMPLE_SIZE; i++) {
				float x = ((float)rand()) / rand_max_f, z = ((float)rand()) / rand_max_f;
				x *= 2.0f, z *= 2.0f;
				x -= 1.0f, z -= 1.0f;
				//float y = sqrt((1.0f - (x * x) - (z * z)));
				float y = ((float)rand()) / rand_max_f;

				vec3 point = vec3(x, y, z);
				float scale = (float)i / 64.0;
				scale = lerp(0.1f, 1.0f, scale * scale);

				_samples[i] = point;
			}

			for (int i = 0; i < AO_SAMPLE_SIZE; i += 1) {
				Vector3& p = _samples[i];
				float u = random();
				float v = random();
				float theta = u * 2.0 * PI;
				float phi = acos(2.0 * v - 1.0);
				float r = cbrt(random() * 0.9 + 0.1) * 1.0;
				float sinTheta = sin(theta);
				float cosTheta = cos(theta);
				float sinPhi = sin(phi);
				float cosPhi = cos(phi);
				p.x = r * sinPhi * cosTheta;
				p.y = r * sinPhi * sinTheta;
				p.z = r * cosPhi;
				if (p.z < 0)
					p.z *= -1.0;

				p = p.normalize();
			}

			vec3 noise_vecs[16];
			for (uint32_t i = 0; i < 16; i++) {
				noise_vecs[i].x = (((float)rand()) / rand_max_f) * 2.0f - 1.0f;
				noise_vecs[i].x = (((float)rand()) / rand_max_f) * 2.0f - 1.0f;
				noise_vecs[i].z = 0.0;

				noise_vecs[i] = noise_vecs[i].normalize();
			}

			custom_noise_tex = upload_raw_texture_square((float*) noise_vecs, 16);

			quad_mesh = Mesh::getQuad();
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

			Matrix44 inv_proj_mat = camera->projection_matrix;
			inv_proj_mat.inverse();

			shader->setUniform("u_view", camera->view_matrix);
			shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			shader->setUniform("u_inv_projection", inv_proj_mat);
			shader->setUniform("u_projection", camera->projection_matrix);
			shader->setUniform("u_near_far", vec2(camera->near_plane, camera->far_plane));
			shader->setUniform3Array("u_random_point", (float*) _samples, AO_SAMPLE_SIZE);
			shader->setUniform("u_ao_radius", ao_radius);
			shader->setUniform("u_normal_occ_tex", normal_tex, 0);
			shader->setUniform("u_depth_tex", depth_tex, 1);
			
			glActiveTexture(GL_TEXTURE0 + 2);
			glBindTexture(GL_TEXTURE_2D, custom_noise_tex);
			shader->setUniform1("u_noise_tex", 2);
			shader->setUniform("u_noise_scale", vec2(ao_fbo->width / 4.0f, ao_fbo->height / 4.0f));

			//shader->setUniform("u_noise_tex", Texture::Get("data/noise_tex.png"), 2);
			//shader->setUniform("u_noise_scale", vec2(ao_fbo->width / 512.0f, ao_fbo->height / 512.0f));

			quad_mesh->render(GL_TRIANGLES);

			shader->disable();

			// Blur AO PASS ======
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