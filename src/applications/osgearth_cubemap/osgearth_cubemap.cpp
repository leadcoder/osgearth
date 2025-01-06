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

#include <osgEarthImGui/ImGuiApp>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgViewer/Viewer>
#include <osg/TextureCubeMap>
#include <osg/Camera>


#include <osgEarthImGui/LayersGUI>
#include <osgEarthImGui/ContentBrowserGUI>
#include <osgEarthImGui/NetworkMonitorGUI>
#include <osgEarthImGui/NotifyGUI>
#include <osgEarthImGui/SceneGraphGUI>
#include <osgEarthImGui/TextureInspectorGUI>
#include <osgEarthImGui/ViewpointsGUI>
#include <osgEarthImGui/LiveCamerasGUI>
#include <osgEarthImGui/SystemGUI>
#include <osgEarthImGui/EnvironmentGUI>
#include <osgEarthImGui/TerrainGUI>
#include <osgEarthImGui/ShaderGUI>
#include <osgEarthImGui/CameraGUI>
#include <osgEarthImGui/RenderingGUI>
#include <osgEarthImGui/AnnotationsGUI>
#include <osgEarthImGui/PickerGUI>
#include <osgEarthImGui/OpenEarthFileGUI>

#ifdef OSGEARTH_HAVE_GEOCODER
#include <osgEarthImGui/SearchGUI>
#endif

#ifdef OSGEARTH_HAVE_PROCEDURAL_NODEKIT
#include <osgEarthImGui/LifeMapLayerGUI>
#include <osgEarthImGui/TerrainEditGUI>
#include <osgEarthImGui/TextureSplattingLayerGUI>
#include <osgEarthImGui/VegetationLayerGUI>
#endif

#ifdef OSGEARTH_HAVE_CESIUM_NODEKIT
#include <osgEarthImGui/CesiumIonGUI>
#endif

#include "EnvironmentMap.h"

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
    using namespace osgEarth::Util;

    /**
     * GUI for the EarthManipulator settings
     */
    struct CubeMapGUI : public ImGuiPanel
    {
        Config _loadConf;
        osgEarth::EnvironmentMap* _envMap{};
        CubeMapGUI(EnvironmentMap* map) :
            ImGuiPanel("Cubemap"),
            _envMap(map)
        {
            
            //nop
        }

        void load(const Config& conf) override
        {
            // remember, the settings come in one at a time so we have to use merge
            _loadConf.merge(conf);
        }

        void save(Config& conf) override
        {
            
        }

        void draw(osg::RenderInfo& ri)
        {
            if (!isVisible())
                return;

            ImGui::Begin(name(), visible());
            {
                for (auto t : _envMap->_textures)
                {
                    ImGuiEx::OSGTexture(t, ri, 100);
                }
                
            }
            ImGui::End();
        }
    };
}

#if 0

// Helper function to orient the camera for each face
void setCubeMapCameraView(osg::Camera* camera, int face, const osg::Vec3& position)
{
    osg::Vec3 target, up;

    switch (face)
    {
    case osg::TextureCubeMap::POSITIVE_X:
        target = osg::Vec3(1, 0, 0);
        up = osg::Vec3(0, -1, 0);
        break;
    case osg::TextureCubeMap::NEGATIVE_X:
        target = osg::Vec3(-1, 0, 0);
        up = osg::Vec3(0, -1, 0);
        break;
    case osg::TextureCubeMap::POSITIVE_Y:
        target = osg::Vec3(0, 1, 0);
        up = osg::Vec3(0, 0, 1);
        break;
    case osg::TextureCubeMap::NEGATIVE_Y:
        target = osg::Vec3(0, -1, 0);
        up = osg::Vec3(0, 0, -1);
        break;
    case osg::TextureCubeMap::POSITIVE_Z:
        target = osg::Vec3(0, 0, 1);
        up = osg::Vec3(0, -1, 0);
        break;
    case osg::TextureCubeMap::NEGATIVE_Z:
        target = osg::Vec3(0, 0, -1);
        up = osg::Vec3(0, -1, 0);
        break;
    }

    camera->setViewMatrixAsLookAt(position, position + target, up);
}


#endif

int
main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc, argv);
    if (arguments.read("--help"))
        return usage(argv[0]);

    osgEarth::initialize(arguments);

    // Set up the viewer and input handler:
    osgViewer::Viewer viewer(arguments);
    viewer.setThreadingModel(viewer.SingleThreaded);
    viewer.setCameraManipulator(new EarthManipulator(arguments));

    // Call this to enable ImGui rendering.
    // If you use the MapNodeHelper, call this first.
    viewer.setRealizeOperation(new ImGuiAppEngine::RealizeOperation);

    // Load the earth file.
    osg::ref_ptr<osg::Node> node = MapNodeHelper().load(arguments, &viewer);
    if (node.valid())
    {
        // Call this to add the GUI. 
        auto ui = new ImGuiAppEngine(arguments);

#ifdef OSGEARTH_HAVE_OPEN_EARTH_FILE_GUI
        ui->add("File", new OpenEarthFileGUI());
#endif
        ui->add("File", new ImGuiDemoWindowGUI());
        ui->add("File", new SeparatorGUI());
        ui->add("File", new QuitGUI());

        ui->add("Tools", new CameraGUI());
        ui->add("Tools", new ContentBrowserGUI());
        ui->add("Tools", new EnvironmentGUI());
        ui->add("Tools", new NetworkMonitorGUI());
        ui->add("Tools", new NVGLInspectorGUI());
        ui->add("Tools", new AnnotationsGUI());
        ui->add("Tools", new LayersGUI());
        ui->add("Tools", new PickerGUI());
        ui->add("Tools", new RenderingGUI());
        ui->add("Tools", new SceneGraphGUI());
#ifdef OSGEARTH_HAVE_GEOCODER
        ui->add("Tools", new SearchGUI());
#endif
        ui->add("Tools", new ShaderGUI(&arguments));
        ui->add("Tools", new SystemGUI());
        ui->add("Tools", new TerrainGUI());
        ui->add("Tools", new TextureInspectorGUI());
        ui->add("Tools", new ViewpointsGUI());
        ui->add("Tools", new LiveCamerasGUI());

#ifdef OSGEARTH_HAVE_CESIUM_NODEKIT
        ui->add("Cesium", new osgEarth::Cesium::CesiumIonGUI());
#endif

#ifdef OSGEARTH_HAVE_PROCEDURAL_NODEKIT
        ui->add("Procedural", new osgEarth::Procedural::LifeMapLayerGUI());
        ui->add("Procedural", new osgEarth::Procedural::TerrainEditGUI);
        ui->add("Procedural", new osgEarth::Procedural::TextureSplattingLayerGUI());
        ui->add("Procedural", new osgEarth::Procedural::VegetationLayerGUI());
#endif

        ui->onStartup = []()
        {
            ImGui::GetIO().FontAllowUserScaling = true;
        };

        // Put it on the front of the list so events don't filter through to other handlers.
        viewer.getEventHandlers().push_front(ui);
       
        osgEarth::MapNode* mapNode = osgEarth::MapNode::get(node);
        if (mapNode)
        {
            mapNode->setNodeMask(2U);
        }
        // Render cubemap at camera position
        osg::Vec3 cameraPos(0.0f, 0.0f, 0.0f); // Replace with the main camera position

        osg::Group* root = new osg::Group;
        
        root->addChild(node);
        viewer.setSceneData(root);
        auto env_map = new osgEarth::EnvironmentMap(node);
        root->addChild(env_map);
        ui->add("Tools", new CubeMapGUI(env_map));
        while (!viewer.done())
        {
            osg::Vec3d eye = viewer.getCamera()->getInverseViewMatrix().getTrans();
            //env_map->setMatrix(osg::Matrixd::translate(eye));
            env_map->_position = eye;
            //CAMERA_CB->_debug = viewer.getCamera();
            viewer.frame();
        }
    }
    else
    {
        return usage(argv[0]);
    }
}
