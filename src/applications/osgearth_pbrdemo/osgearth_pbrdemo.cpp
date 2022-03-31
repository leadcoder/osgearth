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
#include "osg/TextureCubeMap"
#include "osgDB/FileNameUtils"
#include "osgDB/FileUtils"

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




int main(int argc, char** argv)
{

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
        // Group to hold all our annotation elements.
        osg::Group* model_group = new osg::Group();
        model_group->setStateSet(new PbrUberMaterial());
        mapNode->addChild(model_group);

        const SpatialReference* geoSRS = mapNode->getMapSRS()->getGeographicSRS();
        std::string libname = osgDB::Registry::instance()->createLibraryNameForExtension("gltf");
        osgDB::Registry::instance()->loadLibrary(libname);
        osg::Node* mesh = osgDB::readNodeFile(DATA_PATH + "DamagedHelmet/DamagedHelmet.gltf.10.scale");
        auto modelNode = new GeoTransform();
        modelNode->setPosition(GeoPoint(geoSRS, 15.35552, 58.47792, 90));
        auto rot_node = new osg::PositionAttitudeTransform();
        rot_node->setAttitude(osg::Quat(0, osg::Vec3d(0, 0, 1)));
        rot_node->addChild(mesh);
        modelNode->addChild(rot_node);
        model_group->addChild(modelNode);

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
