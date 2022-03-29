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


osg::ref_ptr<osg::TextureCubeMap> loadCubeMap(const std::string& filePath)
{
    osg::ref_ptr<osg::TextureCubeMap> cubemap;

    std::string absolutePath = osgDB::findDataFile(filePath);
    std::map<std::string, std::string> imageMap;

    osgDB::DirectoryContents contents = osgDB::getDirectoryContents(absolutePath);
    for (unsigned int i = 0; i < contents.size(); ++i)
    {
        std::string filenameInDir = osgDB::convertToLowerCase(contents[i]);

        if (filenameInDir == "." ||
            filenameInDir == "..")
        {
            continue;
        }


        if (filenameInDir.find("posx") != std::string::npos || filenameInDir.find("right") != std::string::npos)
        {
            imageMap["posx"] = absolutePath + "\\" + contents[i];
        }

        if (filenameInDir.find("negx") != std::string::npos || filenameInDir.find("left") != std::string::npos)
        {
            imageMap["negx"] = absolutePath + "\\" + contents[i];
        }

        if (filenameInDir.find("posy") != std::string::npos || filenameInDir.find("top") != std::string::npos)
        {
            imageMap["posy"] = absolutePath + "\\" + contents[i];
        }

        if (filenameInDir.find("negy") != std::string::npos || filenameInDir.find("bottom") != std::string::npos)
        {
            imageMap["negy"] = absolutePath + "\\" + contents[i];
        }

        if (filenameInDir.find("posz") != std::string::npos || filenameInDir.find("front") != std::string::npos)
        {
            imageMap["posz"] = absolutePath + "\\" + contents[i];
        }

        if (filenameInDir.find("negz") != std::string::npos || filenameInDir.find("back") != std::string::npos)
        {
            imageMap["negz"] = absolutePath + "\\" + contents[i];
        }
    }

    if (imageMap.size() < 6)
        return cubemap;

    osg::ref_ptr<osg::Image> imagePosX = osgDB::readImageFile(imageMap["posx"]);
    osg::ref_ptr<osg::Image> imageNegX = osgDB::readImageFile(imageMap["negx"]);
    osg::ref_ptr<osg::Image> imagePosY = osgDB::readImageFile(imageMap["posy"]);
    osg::ref_ptr<osg::Image> imageNegY = osgDB::readImageFile(imageMap["negy"]);
    osg::ref_ptr<osg::Image> imagePosZ = osgDB::readImageFile(imageMap["posz"]);
    osg::ref_ptr<osg::Image> imageNegZ = osgDB::readImageFile(imageMap["negz"]);


    if (imagePosX.valid() && imageNegX.valid() && imagePosY.valid() && imageNegY.valid() && imagePosZ.valid() && imageNegZ.valid())
    {
        imagePosX->flipVertical();
        imageNegX->flipVertical();
        imagePosY->flipVertical();
        imageNegY->flipVertical();
        imagePosZ->flipVertical();
        imageNegZ->flipVertical();

        imagePosX->flipHorizontal();
        imageNegX->flipHorizontal();
        imagePosY->flipHorizontal();
        imageNegY->flipHorizontal();
        imagePosZ->flipHorizontal();
        imageNegZ->flipHorizontal();

        imagePosX->setInternalTextureFormat(GL_SRGB8);
        imageNegX->setInternalTextureFormat(GL_SRGB8);
        imagePosY->setInternalTextureFormat(GL_SRGB8);
        imageNegY->setInternalTextureFormat(GL_SRGB8);
        imagePosZ->setInternalTextureFormat(GL_SRGB8);
        imageNegZ->setInternalTextureFormat(GL_SRGB8);

        cubemap = new osg::TextureCubeMap;
        cubemap->setImage(osg::TextureCubeMap::POSITIVE_X, imagePosX);
        cubemap->setImage(osg::TextureCubeMap::NEGATIVE_X, imageNegX);
        cubemap->setImage(osg::TextureCubeMap::POSITIVE_Y, imagePosY);
        cubemap->setImage(osg::TextureCubeMap::NEGATIVE_Y, imageNegY);
        cubemap->setImage(osg::TextureCubeMap::POSITIVE_Z, imagePosZ);
        cubemap->setImage(osg::TextureCubeMap::NEGATIVE_Z, imageNegZ);

        cubemap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        cubemap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        cubemap->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);


        cubemap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
        cubemap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);

        //cubemap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::NEAREST);
        //cubemap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::NEAREST);
    }

    return cubemap.get();
}


int
main(int argc, char** argv)
{
    osg::ArgumentParser arguments(&argc, argv);
    if (arguments.read("--help"))
        return usage(argv[0]);

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
        mapNode->addChild(model_group);

        const SpatialReference* geoSRS = mapNode->getMapSRS()->getGeographicSRS();
        Style style;
        style.getOrCreate<ModelSymbol>()->autoScale() = false;
        std::string libname = osgDB::Registry::instance()->createLibraryNameForExtension("gltf");
        osgDB::Registry::instance()->loadLibrary(libname);
        //style.getOrCreate<ModelSymbol>()->url()->setLiteral("D:/dev_zone/GASS/gass-leadcoder/repo/samples/common/data/gfx/osg/3dmodels/gta/gta_chassis.obj.10.scale");
        style.getOrCreate<ModelSymbol>()->url()->setLiteral("C:/tmp/DamagedHelmet/DamagedHelmet.gltf.10.scale");
        //ModelNode* modelNode = new ModelNode(mapNode, style);
        // A lat/long SRS for specifying points.
        osg::Node* mesh = osgDB::readNodeFile("C:/tmp/DamagedHelmet/gltf/DamagedHelmet.gltf.10.scale");
        auto modelNode = new GeoTransform();
        modelNode->setPosition(GeoPoint(geoSRS, 15.35552, 58.47792, 90));
        auto rot_node = new osg::PositionAttitudeTransform();
        rot_node->setAttitude(osg::Quat(0, osg::Vec3d(0, 0, 1)));
        rot_node->addChild(mesh);
        modelNode->addChild(rot_node);

        auto stateset = modelNode->getOrCreateStateSet();

        //Create object shader
        auto* vp = osgEarth::VirtualProgram::getOrCreate(stateset);
        vp->setInheritShaders(true);
		const char* vs = R"(
    #version 330 compatibility
    in vec4 oe_pbr_tangent;
    out vec4 oe_pbr_texcoord0;
    out mat3 oe_pbr_TBN;
    vec3 vp_Normal;
    void vs(inout vec4 vertex_model)
    {
        oe_pbr_texcoord0 = gl_MultiTexCoord0;
        vec3 normal = normalize(vp_Normal);
        vec3 tangent = normalize(gl_NormalMatrix*oe_pbr_tangent.xyz);
        vec3 bitangent = normalize(cross(normal, tangent) * oe_pbr_tangent.w);
        oe_pbr_TBN = mat3(tangent, bitangent, normal);
    }
)";



        const char* fs = R"(
    #version 430
    // fragment stage global PBR parameters.
struct OE_PBR {
    float roughness;
    float ao;
    float metal;
    float brightness;
    float contrast;
} oe_pbr;
    uniform float oe_model_brightness = 1.0;
    uniform float oe_model_contrast = 1.0;
    uniform float oe_model_roughness = 1.0;
    uniform float oe_model_metal = 1.0;

    vec3 vp_Normal;
    in vec4 oe_pbr_texcoord0;
    in mat3 oe_pbr_TBN;
    uniform sampler2D oe_pbr_color_sampler;
    uniform sampler2D oe_pbr_roughness_metal_sampler;
    uniform sampler2D oe_pbr_normal;
    void fs(inout vec4 color)
    {
        vec4 n = texture(oe_pbr_normal, oe_pbr_texcoord0.xy);
        n.xyz = n.xyz*2.0 - 1.0;
        vp_Normal = normalize(oe_pbr_TBN * n.xyz);

        vec4 roughness_metal = texture(oe_pbr_roughness_metal_sampler, oe_pbr_texcoord0.xy);
        oe_pbr.roughness = roughness_metal.g * oe_model_roughness;
		oe_pbr.metal = roughness_metal.b * oe_model_metal;
		oe_pbr.brightness = oe_model_brightness;
		oe_pbr.contrast = oe_model_contrast;
        color = color * texture(oe_pbr_color_sampler, oe_pbr_texcoord0.xy);
    }
)";
        vp->setFunction("vs", vs, osgEarth::ShaderComp::LOCATION_VERTEX_VIEW, 1.1f);
        vp->setFunction("fs", fs, osgEarth::ShaderComp::LOCATION_FRAGMENT_COLORING, 0.5f);
        vp->addBindAttribLocation("oe_pbr_tangent", 6);
        stateset->getOrCreateUniform("oe_pbr_color_sampler", osg::Uniform::SAMPLER_2D)->set(0);
        stateset->getOrCreateUniform("oe_pbr_roughness_metal_sampler", osg::Uniform::SAMPLER_2D)->set(1);
        stateset->getOrCreateUniform("oe_pbr_normal", osg::Uniform::SAMPLER_2D)->set(2);
        stateset->getOrCreateUniform("oe_pbr_irradiance", osg::Uniform::SAMPLER_CUBE)->set(3);
        model_group->addChild(modelNode);

        stateset->setDefine("IRRADIANCEMAP");
        stateset->setMode(GL_TEXTURE_CUBE_MAP_SEAMLESS, osg::StateAttribute::ON);
        auto irradiance_texture = loadCubeMap("D:/dev_zone/osgpbr/osgEffect/effectcompositor/data/Assets/Environment2.cubemap/");
        stateset->setTextureAttribute(3, irradiance_texture, osg::StateAttribute::ON);

        //osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile("ibl_brdf_lut.png");
        osg::ref_ptr<osg::Image> image = osgDB::readRefImageFile("D:/dev_zone/osgpbr/osgEffect/effectcompositor/data/brdfLUT.png");
        osg::Texture2D* texture = new osg::Texture2D;
        texture->setImage(image);
        texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::CLAMP_TO_EDGE);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        ///texture->setMaxAnisotropy(16.0f);
        stateset->setTextureAttribute(4, texture, osg::StateAttribute::ON);
        stateset->addUniform(new osg::Uniform("oe_pbr_brdf_lut", 4));

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
