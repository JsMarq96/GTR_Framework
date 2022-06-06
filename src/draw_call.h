#pragma once

#include "application.h"
#include "prefab.h"
#include "mesh.h"

namespace GTR {
	struct sDrawCall {
		Matrix44 model;
		Mesh* mesh;
		Material* material;
		Camera* camera;
		float camera_distance;
		ePBR_Type pbr_structure;

		BoundingBox aabb; // Mesh Nounding box

		uint16_t light_count;
		LightEntity* lights_for_call[MAX_LIGHT_NUM];
		// Data oriented comon light data
		eLightType light_type[MAX_LIGHT_NUM];
		vec3  light_positions[MAX_LIGHT_NUM];
		vec3  light_color[MAX_LIGHT_NUM];
		float light_max_distance[MAX_LIGHT_NUM];
		float light_intensities[MAX_LIGHT_NUM];

		int light_shadow_id[MAX_LIGHT_NUM];

		// Spot light data
		float  light_cone_angle[MAX_LIGHT_NUM];
		float  light_cone_decay[MAX_LIGHT_NUM];

		// Directional & Spot data
		vec3   light_direction[MAX_LIGHT_NUM];

		// Area light
		float  light_directional_area[MAX_LIGHT_NUM];


		inline void add_light(LightEntity* light) {
			if (light_count < MAX_LIGHT_NUM) {
				light_shadow_id[light_count] = light->light_id;
				light_type[light_count] = light->light_type;
				light_positions[light_count] = light->get_translation();
				light_color[light_count] = light->color;
				light_max_distance[light_count] = light->max_distance;
				light_intensities[light_count] = light->intensity;

				light_direction[light_count] = light->get_model().frontVector() * -1.0f;

				switch (light_type[light_count]) {
				case SPOT_LIGHT:
					light_cone_angle[light_count] = light->cone_angle * 0.0174533; // To radians
					light_cone_decay[light_count] = light->cone_exp_decay;
					break;
				case DIRECTIONAL_LIGHT:
					light_directional_area[light_count] = light->area_size;
					break;
				default: break;
				}

				lights_for_call[light_count++] = light;
			}
		}
	};
};