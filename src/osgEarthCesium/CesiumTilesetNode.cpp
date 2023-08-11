/* -*-c++-*- */
/* osgEarth - Geospatial SDK for OpenSceneGraph
* Copyright 2008-2012 Pelican Mapping
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
#include "CesiumTilesetNode"

#include <CesiumGltf/Material.h>

#include "AssetAccessor"
#include "PrepareRenderResources"
#include "TaskProcessor"

#include <osgEarth/Notify>
#include <osgUtil/CullVisitor>

#include <gsl/gsl>
#include <Cesium3DTilesSelection/GltfUtilities.h>
#include <Cesium3DTilesSelection/registerAllTileContentTypes.h>
#include <CesiumGltf/AccessorView.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/IonRasterOverlay.h>

#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>
#include <mutex>

using namespace osgEarth::Cesium;

// TODO:  Replace this with the default key from Cesium
static std::string CESIUM_KEY = "";

std::string osgEarth::Cesium::getCesiumIonKey()
{
    return CESIUM_KEY;
}

void osgEarth::Cesium::setCesiumIonKey(const std::string& key)
{
    CESIUM_KEY = key;
}

class Context
{
public:

    Context()
    {
        Cesium3DTilesSelection::registerAllTileContentTypes();
        assetAccessor = std::make_shared<AssetAccessor>();
        taskProcessor = std::make_shared<TaskProcessor>();
        prepareRenderResources = std::make_shared< PrepareRendererResources >();
        logger = spdlog::default_logger();        
        creditSystem = std::make_shared<Cesium3DTilesSelection::CreditSystem>();

        // Get the key from an environment variable
        const char* key = ::getenv("OSGEARTH_CESIUMION_KEY");
        if (key)
        {
            setCesiumIonKey(std::string(key));
        }
    }

    ~Context()
    {
    }

    static Context& instance()
    {
        static Context s_context;
        return s_context;
    }

    std::shared_ptr< PrepareRendererResources > prepareRenderResources;
    std::shared_ptr<AssetAccessor> assetAccessor;
    std::shared_ptr<TaskProcessor> taskProcessor;
    std::shared_ptr< spdlog::logger > logger;
    std::shared_ptr< Cesium3DTilesSelection::CreditSystem > creditSystem;

    std::unique_ptr<Context> context;
};

CesiumTilesetNode::CesiumTilesetNode(unsigned int assetID, std::vector<int> overlays)
{ 
    CesiumAsync::AsyncSystem asyncSystem(Context::instance().taskProcessor);
    Cesium3DTilesSelection::TilesetExternals externals{
        Context::instance().assetAccessor, Context::instance().prepareRenderResources, asyncSystem, Context::instance().creditSystem, Context::instance().logger, nullptr
    };

    Cesium3DTilesSelection::TilesetOptions options;
    Cesium3DTilesSelection::Tileset* tileset = new Cesium3DTilesSelection::Tileset(externals, assetID, getCesiumIonKey(), options);

    // TODO:  This needs reworked, just a quick way to get overlays working to test.
    for (auto& overlay = overlays.begin(); overlay != overlays.end(); ++overlay)
    {
        Cesium3DTilesSelection::RasterOverlayOptions rasterOptions;
        const auto ionRasterOverlay = new Cesium3DTilesSelection::IonRasterOverlay("", 2, getCesiumIonKey(), rasterOptions);
        tileset->getOverlays().add(ionRasterOverlay);
    }    
    _tileset = tileset;

    setCullingActive(false);
}

CesiumTilesetNode::CesiumTilesetNode(const std::string& url)
{
    CesiumAsync::AsyncSystem asyncSystem(Context::instance().taskProcessor);
    Cesium3DTilesSelection::TilesetExternals externals{
        Context::instance().assetAccessor, Context::instance().prepareRenderResources, asyncSystem, Context::instance().creditSystem, Context::instance().logger, nullptr
    };

    Cesium3DTilesSelection::TilesetOptions options;
    Cesium3DTilesSelection::Tileset* tileset = new Cesium3DTilesSelection::Tileset(externals, url, options);
    _tileset = tileset;

    setCullingActive(false);
}

void
CesiumTilesetNode::traverse(osg::NodeVisitor& nv)
{
    if (nv.getVisitorType() == nv.CULL_VISITOR)
    {
        const osgUtil::CullVisitor* cv = nv.asCullVisitor();
        osg::Vec3d osgEye, osgCenter, osgUp;
        cv->getModelViewMatrix()->getLookAt(osgEye, osgCenter, osgUp);
        osg::Vec3d osgDir = osgCenter - osgEye;
        osgDir.normalize();

        glm::dvec3 pos(osgEye.x(), osgEye.y(), osgEye.z());
        glm::dvec3 dir(osgDir.x(), osgDir.y(), osgDir.z());
        glm::dvec3 up(osgUp.x(), osgUp.y(), osgUp.z());
        glm::dvec2 viewportSize(cv->getViewport()->width(), cv->getViewport()->height());

        double vfov, ar, znear, zfar;
        cv->getProjectionMatrix()->getPerspective(vfov, ar, znear, zfar);
        vfov = osg::DegreesToRadians(vfov);
        double hfov = 2 * atan(tan(vfov / 2) * (ar));

        // TODO:  Multiple views
        std::vector<Cesium3DTilesSelection::ViewState> viewStates;
        Cesium3DTilesSelection::ViewState viewState = Cesium3DTilesSelection::ViewState::create(pos, dir, up, viewportSize, hfov, vfov);
        viewStates.push_back(viewState);
        Cesium3DTilesSelection::Tileset* tileset = (Cesium3DTilesSelection::Tileset*)_tileset;
        auto updates = tileset->updateView(viewStates);

        removeChildren(0, getNumChildren());
        for (auto tile : updates.tilesToRenderThisFrame)
        {
            if (tile->getContent().isRenderContent())
            {
                MainThreadResult* result = reinterpret_cast<MainThreadResult*>(tile->getContent().getRenderContent()->getRenderResources());
                if (result && result->node.valid()) {
                    addChild(result->node.get());
                }
            }
        }

    }
    else if (nv.getVisitorType() == nv.UPDATE_VISITOR)
    {        
        osg::Group::traverse(nv);
    }

    osg::Group::traverse(nv);
}