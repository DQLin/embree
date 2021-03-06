// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "scene.h"

namespace embree
{
  extern "C" int g_instancing_mode;

  void TutorialScene::add(Ref<SceneGraph::GroupNode> group) 
  {
    for (auto& node : group->children) 
    {
      if (Ref<SceneGraph::LightNode> lightNode = node.dynamicCast<SceneGraph::LightNode>()) {
        lights.push_back(lightNode->light);
      } 
      else if (Ref<SceneGraph::TransformNode> xfmNode = node.dynamicCast<SceneGraph::TransformNode>()) {
        if (g_instancing_mode == SceneGraph::INSTANCING_GEOMETRY_GROUP) {
          if (Ref<SceneGraph::GroupNode> groupNode = xfmNode->child.dynamicCast<SceneGraph::GroupNode>()) {
            for (size_t i=0; i<groupNode->size(); i++) {
              addGeometry(groupNode->child(i));
            }
          }
        }
        addGeometry(xfmNode->child);
        addGeometry(node);
      }
      else if (Ref<SceneGraph::PerspectiveCameraNode> cameraNode = node.dynamicCast<SceneGraph::PerspectiveCameraNode>()) {
        cameras.push_back(cameraNode);
      } 
      else {
        addGeometry(node);
      }
      geomID_to_scene.resize(geometries.size());
      geomID_to_inst.resize(geometries.size());
    }
  }
  
  unsigned TutorialScene::addGeometry(Ref<SceneGraph::Node> node) 
  {
    if (geometry2id.find(node) == geometry2id.end()) {
      geometries.push_back(node);
      geometry2id[node] = unsigned(geometries.size()-1);
    }
    return geometry2id[node];
  }
  
  unsigned TutorialScene::materialID(Ref<SceneGraph::MaterialNode> material) 
  {
    if (material2id.find(material) == material2id.end()) {
      materials.push_back(material);
      material2id[material] = unsigned(materials.size()-1);
    }
    return material2id[material];
  }
  
  unsigned TutorialScene::geometryID(Ref<SceneGraph::Node> geometry) 
  {
    assert(geometry2id.find(geometry) != geometry2id.end());
    return geometry2id[geometry];
  }
  
  void TutorialScene::print_camera_names ()
  {
    if (cameras.size() == 0) {
      std::cout << "no cameras inside the scene" << std::endl;
      return;
    }
    
    for (size_t i=0; i<cameras.size(); i++)
      std::cout << "camera " << i << ": " << cameras[i]->name << std::endl;
  }

  Ref<SceneGraph::PerspectiveCameraNode> TutorialScene::getDefaultCamera()
  {
    if (cameras.size()) return cameras[0];
    return nullptr;
  }

  Ref<SceneGraph::PerspectiveCameraNode> TutorialScene::getCamera(const std::string& name)
  {
    for (size_t i=0; i<cameras.size(); i++)
      if (cameras[i]->name == name) return cameras[i];
    
    THROW_RUNTIME_ERROR("camera \"" + name +"\" not found");
  }
};
