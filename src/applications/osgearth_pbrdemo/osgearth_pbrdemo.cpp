/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2020 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/


#include <osgEarth/ImGui/ImGui>
#include <osgEarth/PBRMaterial>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgViewer/Viewer>
#include <osgEarth/ModelNode>

#include <osgDB/ReadFile>
#include <osg/TextureCubeMap>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include "LUTGenerator.cpp"

osg::Texture2D* LUT_TEX = nullptr;
#define LC "[imgui] "

using namespace osgEarth;
using namespace osgEarth::Util;

int
usage(const char* name)
{
	OE_NOTICE
		<< "\nUsage: " << name << " file.earth" << std::endl
		<< MapNodeHelper().usage() << std::endl;
	return 0;
}

namespace osgEarth
{
	namespace GUI
	{
		using namespace osgEarth;
		using namespace osgEarth::Util;

		inline osg::Uniform* overrideUniform(osg::StateSet* ss, std::string name, osg::Uniform::Type type)
		{
			auto uniform = ss->getUniform(name);
			if (!uniform)
			{
				uniform = new osg::Uniform(type, name);
				ss->addUniform(uniform, osg::StateAttribute::OVERRIDE);
			}
			return uniform;
		}

		inline void drawTexture(osg::RenderInfo& ri, osg::Texture2D* texture, int size = 100)
		{
			
			if (texture)
			{
				std::string filename = texture->getImage() != nullptr ? texture->getImage()->getFileName() : texture->getName();
				ImGui::Text("Name %s", filename.c_str());
				ImGuiUtil::Texture(texture, ri, size);
			}
		}


		class PBRGUI : public BaseGUI
		{
		private:
			PbrUberMaterial* _UberMaterial;
			osg::Node* _Model;

			
		public:
			std::vector<std::string> _models;
			PBRGUI(osg::Node* model, PbrUberMaterial* mat) : BaseGUI("Pbr"),
				_UberMaterial(mat),
				_Model(model)
			{
				//osg::Node* mesh = osgDB::readNodeFile("C:/tmp/beetlefusca_version_1/scene.gltf");
			}

			void load(const Config& conf) override
			{
			}
			void save(Config& conf) override
			{
			}

			class MaterialVisitor : public osg::NodeVisitor
			{
			public:
				int _matCount = 0;
				osg::RenderInfo* _renderInfo = nullptr;
				MaterialVisitor(osg::RenderInfo& ri) :
					osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
					_renderInfo(&ri)
				{
					setNodeMaskOverride(~0);
				}

				void apply(osg::Group& node)
				{
					traverse(node);
				}

				

				void apply(osg::Node& node)
				{
					auto material = dynamic_cast<PbrMaterial*>(node.getStateSet());
					if (material)
					{
						// Non groups act as leaf nodes
						std::string label = "Material "+ std::to_string(_matCount) + " " + node.getName();
						if (ImGui::TreeNode(label.c_str()))
						{
							if (material->hasColorMap()  && ImGui::TreeNode("ColorMap"))
							{
								auto enabled = material->getColorMapEnabled();
								if (ImGui::Checkbox("Enabled", &enabled))
									material->setColorMapEnabled(enabled);
								drawTexture(*_renderInfo, material->getColorMap());
								ImGui::TreePop();
							}

							if (material->hasNormalMap() && ImGui::TreeNode("NormalMap"))
							{
								auto enabled = material->getNormalMapEnabled();
								if (ImGui::Checkbox("Enabled", &enabled))
									material->setNormalMapEnabled(enabled);
								drawTexture(*_renderInfo, material->getNormalMap());
								ImGui::TreePop();
							}

							if (material->hasMetalRoughness() && ImGui::TreeNode("MetalRoughnessMap"))
							{
								auto enabled = material->getMetalRoughnessMapEnabled();
								if (ImGui::Checkbox("Enabled", &enabled))
									material->setMetalRoughnessMapEnabled(enabled);
								drawTexture(*_renderInfo, material->getMetalRoughnessMap());
								ImGui::TreePop();
							}

							if (material->hasEmmisiveMap() && ImGui::TreeNode("EmmisiveMap"))
							{
								auto enabled = material->getEmmisiveMapEnabled();
								if (ImGui::Checkbox("Enabled", &enabled))
									material->setEmissiveMapEnabled(enabled);
								drawTexture(*_renderInfo, material->getEmmisiveMap());
								ImGui::TreePop();
							}

							if (material->hasOcclusionMap() && ImGui::TreeNode("OcclusionMap"))
							{
								auto enabled = material->getOcclusionMapEnabled();
								if (ImGui::Checkbox("Enabled", &enabled))
									material->setOcclusionMapEnabled(enabled);
								ImGui::TreePop();
							}

							auto color_factor = material->getColorFactor();
							if (ImGui::ColorEdit3("ColorFactor", &color_factor.x()))
								material->setColorFactor(color_factor);

							auto emissive_factor = material->getEmissiveFactor();
							if (ImGui::ColorEdit3("EmissiveFactor", &emissive_factor.x()))
								material->setEmissiveFactor(emissive_factor);

							float roughness = material->getRoughnessFactor();
							if (ImGui::SliderFloat("RoughnessFactor", &roughness, 0.0f, 1.0f))
								material->setRoughnessFactor(roughness);

							float metal = material->getMetalFactor();
							if (ImGui::SliderFloat("MetalFactor", &metal, 0.0f, 1.0f))
								material->setMetalFactor(metal);

							ImGui::TreePop();
						}
						
						_matCount++;
					}
				}
			};

			void draw(osg::RenderInfo& ri) override
			{
				ImGui::Begin(name(), visible());
				{
					if (ImGui::TreeNode("Model Materials"))
					{
						MaterialVisitor visitor(ri);
						_Model->accept(visitor);
						ImGui::TreePop();
					}
					
					static float oe_model_brightness = 1.0f;
					static float oe_model_contrast = 1.0f;
					static float oe_model_roughness = 1.0f;
					static float oe_model_metal = 1.0f;
					static float oe_ao_factor = 1.0f;
					static osg::Vec3f oe_model_emissive(1.0f, 1.0f, 1.0f);
					static osg::Vec3f oe_model_color(1.0f, 1.0f, 1.0f);
					auto ss = _UberMaterial;

					static bool color_map_enabled = true;
					static bool normal_map_enabled = true;
					static bool emissive_map_enabled = true;
					static bool ibl_enabled = true;

					if (ImGui::Checkbox("ColorMap", &color_map_enabled))
						_UberMaterial->setColorMapEnabled(color_map_enabled);

					if (ImGui::Checkbox("NormalMap", &normal_map_enabled))
						_UberMaterial->setNormalMapEnabled(normal_map_enabled);

					if (ImGui::Checkbox("EmissiveMap", &emissive_map_enabled))
						_UberMaterial->setEmissiveMapEnabled(emissive_map_enabled);

					if (ImGui::Checkbox("IBL", &ibl_enabled))
						_UberMaterial->setIBLEnabled(ibl_enabled);

					

					
					if (ImGui::SliderFloat("Contrast", &oe_model_contrast, 0.5f, 4.0f))
						ss->getOrCreateUniform("oe_pbr_contrast", osg::Uniform::FLOAT)->set(oe_model_contrast);

					if (ImGui::SliderFloat("Brightness", &oe_model_brightness, 0.5f, 4.0f))
						ss->getOrCreateUniform("oe_pbr_brightness", osg::Uniform::FLOAT)->set(oe_model_brightness);
					if (ImGui::SliderFloat("RoughnessFactor", &oe_model_roughness, 0.0f, 10.0f))
						overrideUniform(ss, "oe_pbr_roughness_factor", osg::Uniform::FLOAT)->set(oe_model_roughness);
					if (ImGui::SliderFloat("MetalFactor", &oe_model_metal, 0.0f, 2.0f))
						overrideUniform(ss, "oe_pbr_metal_factor", osg::Uniform::FLOAT)->set(oe_model_metal);
					if (ImGui::ColorEdit3("EmissiveFactor", &oe_model_emissive.x()))
						overrideUniform(ss, "oe_pbr_emissive_factor", osg::Uniform::FLOAT_VEC3)->set(oe_model_emissive);
					if (ImGui::ColorEdit3("ColorFactor", &oe_model_color.x()))
						overrideUniform(ss, "oe_pbr_color_factor", osg::Uniform::FLOAT_VEC3)->set(oe_model_color);
					if (ImGui::SliderFloat("AOFactor", &oe_ao_factor, 0.0f, 1.0f))
						overrideUniform(ss, "oe_pbr_ao_factor", osg::Uniform::FLOAT)->set(oe_ao_factor);


					if (ImGui::TreeNode("BRDFLutTexture"))
					{
						drawTexture(ri, _UberMaterial->m_LutTexture,300);
						//drawTexture(ri, LUT_TEX, 300);
						ImGui::TreePop();
					}
					static int cmodel = 0;
					if (ImGui::Button("Toggle Model"))
					{
						cmodel = (cmodel + 1) % _models.size();
						osg::Node* new_mesh = osgDB::readNodeFile(_models[cmodel]);
						auto parent = _Model->getParent(0);
						parent->removeChild(_Model);
						parent->addChild(new_mesh);
						_Model = new_mesh;
					}
				}
				ImGui::End();
			}
		};
	}
}

int main(int argc, char** argv)
{
	LUTGenerator gen;
	LUT_TEX = gen.generateLUT();

	osg::ArgumentParser arguments(&argc, argv);
	if (arguments.read("--help"))
		return usage(argv[0]);

	const std::string  DATA_PATH = "D:/dev_zone/osgEarth/pbr_data/";
	osgDB::Registry::instance()->getDataFilePathList().push_back(DATA_PATH);

	osgEarth::initialize();

	osgViewer::Viewer viewer(arguments);
	// Use SingleThreaded mode with imgui.
	viewer.setThreadingModel(viewer.SingleThreaded);
	viewer.setCameraManipulator(new EarthManipulator(arguments));

	// Call this to enable ImGui rendering.
	// If you use the MapNodeHelper, call this first.
	viewer.setRealizeOperation(new GUI::ApplicationGUI::RealizeOperation);

	osg::Node* node = MapNodeHelper().loadWithoutControls(arguments, &viewer);
	if (node)
	{
		// Call this to add the GUI. 
		// Passing "true" tells it to install all the built-in osgEarth GUI tools.
		// Put it on the front of the list so events don't filter
		// through to other handlers.
		GUI::ApplicationGUI* gui = new GUI::ApplicationGUI(arguments, true);
		
		viewer.getEventHandlers().push_front(gui);



		// find the map node that we loaded.
		MapNode* mapNode = MapNode::findMapNode(node);
		
		int normal;
		int rm;
		int ibl;
		int brdf;
		int emission;

		mapNode->getTerrainEngine()->getResources()->reserveTextureImageUnit(normal);
		mapNode->getTerrainEngine()->getResources()->reserveTextureImageUnit(rm);
		mapNode->getTerrainEngine()->getResources()->reserveTextureImageUnit(ibl);
		mapNode->getTerrainEngine()->getResources()->reserveTextureImageUnit(brdf);
		mapNode->getTerrainEngine()->getResources()->reserveTextureImageUnit(emission);

		
		// Group to hold all our annotation elements.
		osg::Group* model_group = new osg::Group();
		auto pbr_material = new PbrUberMaterial(LUT_TEX);
		model_group->setStateSet(pbr_material);
		mapNode->addChild(model_group);

		const SpatialReference* geoSRS = mapNode->getMapSRS()->getGeographicSRS();
		std::string libname = osgDB::Registry::instance()->createLibraryNameForExtension("gltf");
		osgDB::Registry::instance()->loadLibrary(libname);
		osg::Node* mesh = osgDB::readNodeFile(DATA_PATH + "t72/t72.gltf.10.scale");

	
		auto modelNode = new GeoTransform();
		modelNode->setPosition(GeoPoint(geoSRS, 15.35552, 58.47792, 90));
		auto rot_node = new osg::PositionAttitudeTransform();
		rot_node->setAttitude(osg::Quat(0, osg::Vec3d(0, 0, 1)));
		rot_node->addChild(mesh);
		modelNode->addChild(rot_node);
		model_group->addChild(modelNode);

		auto pbr_gui = new GUI::PBRGUI(mesh, pbr_material);
		pbr_gui->_models.push_back(DATA_PATH + "t72/t72.gltf.10.scale");
		pbr_gui->_models.push_back(DATA_PATH + "DamagedHelmet/DamagedHelmet.gltf.10.scale");
		pbr_gui->_models.push_back(DATA_PATH + "beetlefusca/scene.gltf");
		pbr_gui->_models.push_back(DATA_PATH + "MetalRoughSpheres/glTF/MetalRoughSpheres.gltf.5.scale");
		
		

		gui->add(pbr_gui, true);

		viewer.setSceneData(node);
		viewer.getCamera()->getGraphicsContext()->getState()->setUseModelViewAndProjectionUniforms(true);

		EventRouter* router = new EventRouter();
		viewer.addEventHandler(router);
		bool update_rot = true;
		router->onKeyPress(router->KEY_D, [&update_rot]() {
			update_rot = !update_rot;
			});

		while (!viewer.done())
		{
			viewer.frame();
			if (update_rot)
			{
				double rt = 40;
				double inter = fmod(viewer.elapsedTime(), rt) / rt;
				rot_node->setAttitude(osg::Quat(inter * osg::PI * 2, osg::Vec3d(0, 0, 1)));
			}
		}
		return 0;
	}
	else
	{
		return usage(argv[0]);
	}
}
