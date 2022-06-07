#include "frusturm_culling.h"


namespace GTR {

	namespace CULLING {

		bool translucent_draw_call_distance_comp(const GTR::sDrawCall& d1, const GTR::sDrawCall& d2) {
			return d1.camera_distance > d2.camera_distance;
		}
		bool opaque_draw_call_distance_comp(const GTR::sDrawCall& d1, const GTR::sDrawCall& d2) {
			return d1.camera_distance < d2.camera_distance;
		}

		void frustrum_culling(std::vector<GTR::BaseEntity*> entities, sSceneCulling* culling_result, Camera *cam) {
			int total_light_count = 0;
			// Get all the prefabs (that need to be rendered and the lights from the entitty list
			for (int i = 0; i < entities.size(); ++i)
			{
				BaseEntity* ent = entities[i];
				if (!ent->visible)
					continue;

				//is a prefab!
				if (ent->entity_type == PREFAB)
				{
					PrefabEntity* pent = (GTR::PrefabEntity*)ent;
					if (pent->prefab) {
						culling_result->scene_prefabs.push_back(pent);
					}
					else {
						assert("PREFAB IS NULL");
					}
				}
				else if (ent->entity_type == LIGHT) {
					LightEntity* curr_light = (LightEntity*)ent;
					curr_light->light_id = total_light_count++;

					if (curr_light->light_type == DIRECTIONAL_LIGHT) {
						culling_result->_scene_directional_lights.push_back(curr_light);
					}
					else {
						culling_result->_scene_non_directonal_lights.push_back(curr_light);
					}
				}
			}

			// Accourding tot the rendering data, add it to the rendering queue
			for (uint16_t i = 0; i < culling_result->scene_prefabs.size(); i++) {
				PrefabEntity* pent = culling_result->scene_prefabs[i];
				culling_result->add_to_render_queue(pent->model, &(pent->prefab->root), cam, pent->pbr_structure);
			}

			// Iterate all the lights on the scene
			for (uint16_t light_i = 0; light_i < culling_result->_scene_non_directonal_lights.size(); light_i++) {
				LightEntity* curr_light = culling_result->_scene_non_directonal_lights[light_i];

				// Check if the opaque object is in range of the light
				for (uint16_t i = 0; i < culling_result->_opaque_objects.size(); i++) {
					if (curr_light->is_in_range(culling_result->_opaque_objects[i].aabb)) {
						culling_result->_opaque_objects[i].add_light(curr_light);
					}
				}

				// Check if the translucent object is in range of the light
				for (uint16_t i = 0; i < culling_result->_translucent_objects.size(); i++) {
					if (curr_light->is_in_range(culling_result->_translucent_objects[i].aabb)) {
						culling_result->_translucent_objects[i].add_light(curr_light);
					}
				}
			}

			for (uint16_t light_i = 0; light_i < culling_result->_scene_directional_lights.size(); light_i++) {
				LightEntity* curr_light = culling_result->_scene_directional_lights[light_i];

				// Check if the opaque object is in range of the light
				for (uint16_t i = 0; i < culling_result->_opaque_objects.size(); i++) {
					if (curr_light->is_in_range(culling_result->_opaque_objects[i].aabb)) {
						culling_result->_opaque_objects[i].add_light(curr_light);
					}
				}

				// Check if the translucent object is in range of the light
				for (uint16_t i = 0; i < culling_result->_translucent_objects.size(); i++) {
					if (curr_light->is_in_range(culling_result->_translucent_objects[i].aabb)) {
						culling_result->_translucent_objects[i].add_light(curr_light);
					}
				}
			}

			// Order the opaque & translucent
			std::sort(culling_result->_opaque_objects.begin(), culling_result->_opaque_objects.end(), opaque_draw_call_distance_comp);
			std::sort(culling_result->_translucent_objects.begin(), culling_result->_translucent_objects.end(), translucent_draw_call_distance_comp);
		}



			void sSceneCulling::add_to_render_queue(const Matrix44& prefab_model, GTR::Node* node, Camera* camera, ePBR_Type pbr) {
				if (!node->visible)
					return;

				//compute global matrix
				Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

				//does this node have a mesh? then we must render it
				if (node->mesh && node->material)
				{
					//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
					BoundingBox world_bounding = transformBoundingBox(node_model, node->mesh->box);

					//if bounding box is inside the camera frustum then the object is probably visible
					if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize))
					{
						// Compute the closest distance between the bounding box of the mesh and the camera
						float camera_distance = Min((world_bounding.center + world_bounding.halfsize).distance(camera->eye), world_bounding.center.distance(camera->eye));
						camera_distance = Min(camera_distance, (world_bounding.center - world_bounding.halfsize).distance(camera->eye));
						add_draw_instance(node_model, node->mesh, node->material, camera, world_bounding.center.distance(camera->eye), world_bounding, pbr);
					}
				}

				//iterate recursively with children
				for (int i = 0; i < node->children.size(); ++i)
					add_to_render_queue(prefab_model, node->children[i], camera, pbr);
			}

			void sSceneCulling::add_draw_instance(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera, const float camera_distance, const BoundingBox& aabb, const ePBR_Type pbr) {
				// Based on the material, we add it to the translucent or the opaque queue
				if (material->alpha_mode != NO_ALPHA) {
					_translucent_objects.push_back(sDrawCall{ model, mesh, material, camera, camera_distance, pbr, aabb });
				}
				else {
					_opaque_objects.push_back(sDrawCall{ model, mesh, material, camera,  camera_distance, pbr, aabb });
				}
			}
	};
};