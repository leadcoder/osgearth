/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2016 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "BuildingPager"
#include "Analyzer"
#include <osgEarth/Registry>
#include <osgEarth/CullingUtils>
#include <osgEarth/Query>
#include <osgEarth/MetadataNode>
#include <osgEarth/StyleSheet>
#include <osgEarth/Metrics>
#include <osgEarth/Utils>
#include <osgEarth/NodeUtils>
#include <osgEarth/Chonk>
#include <osgEarth/Capabilities>

#include <osgUtil/Optimizer>
#include <osgUtil/Statistics>
#include <osg/Version>
#include <osg/CullFace>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osgUtil/RenderBin>

#include <osgDB/WriteFile>

//#include <osg/ConcurrencyViewerMacros>

#define LC "[BuildingPager] "

using namespace osgEarth::Buildings;
using namespace osgEarth;

#define OE_TEST OE_DEBUG

#define USE_OSGEARTH_ELEVATION_POOL

#ifndef GL_CLIP_DISTANCE0
#define GL_CLIP_DISTANCE0 0x3000
#endif

namespace
{
    struct ArtCache : public osgDB::ObjectCache
    {
        unsigned size() const { return this->_objectCache.size(); }
    };
}

BuildingPager::CacheManager::CacheManager() :
    osg::Group(),
    _renderLeaves(0),
    _cullCompleted(false),
    _renderLeavesDetected(false)
{
    setCullingActive(false);
    ADJUST_UPDATE_TRAV_COUNT(this, +1);

    // An object cache for shared resources like textures, atlases, and instanced models.
    _artCache = new ArtCache();

    // Texture object cache
    _texCache = new TextureCache();

    // Shared stateset cache for shader generation
    _stateSetCache = new StateSetCache();
    _stateSetCache->setMaxSize(~0);
}

void
BuildingPager::CacheManager::releaseGLObjects(osg::State* state) const
{
    if (_artCache.valid())
    {
        _artCache->releaseGLObjects(state);
        _artCache->clear();
    }

    if (_texCache.valid())
    {
        _texCache->releaseGLObjects(state);
        _texCache->clear();
    }

    if (_stateSetCache.valid())
    {
        _stateSetCache->releaseGLObjects(state);
        _stateSetCache->clear();
    }
    
    OE_DEBUG << LC << "Cleared all internal caches" << std::endl;

    // invoke parent's implementation
    osg::Group::releaseGLObjects(state);
}

void
BuildingPager::CacheManager::resizeGLObjectBuffers(unsigned size)
{
    if (_texCache.valid())
    {
        _texCache->resizeGLObjectBuffers(size);
    }

    if (_stateSetCache.valid())
    {
        //_stateSetCache->resizeGLObjectBuffers(size);
    }

    osg::Group::resizeGLObjectBuffers(size);
}

void
BuildingPager::CacheManager::traverse(osg::NodeVisitor& nv)
{
    if (nv.getVisitorType() == nv.CULL_VISITOR)
    {
        if (nv.getFrameStamp())
        {
            _artCache->updateTimeStampOfObjectsInCacheWithExternalReferences(
                nv.getFrameStamp()->getReferenceTime());

            _artCache->removeExpiredObjectsInCache(10.0);
        }

        osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>(&nv);
        int before = RenderBinUtils::getTotalNumRenderLeaves(cv->getCurrentRenderBin());

        osg::Group::traverse(nv);

        int after = RenderBinUtils::getTotalNumRenderLeaves(cv->getCurrentRenderBin());
        int newLeaves = after - before;
        if (newLeaves > 0)
        {
            _renderLeaves.fetch_add(newLeaves);
            _renderLeavesDetected.exchange(true);
        }

        _cullCompleted.exchange(true);
    }

    else if (nv.getVisitorType() == nv.UPDATE_VISITOR)
    {
        // if nothing was culled, clear out the caches and release their memory.
        // _cullCompleted = so it will work if update is called more than once
        //                  between culls
        if (_cullCompleted.exchange(false))
        {
            if (_renderLeavesDetected && _renderLeaves == 0)
            {
                releaseGLObjects(nullptr);
                _renderLeavesDetected = false;
            }
            osg::Group::traverse(nv);
        }
        _renderLeaves = 0;
    }

    else
    {
        osg::Group::traverse(nv);
    }
}



//...................................................................


BuildingPager::BuildingPager(const Map* map, const Profile* profile, bool useNVGLIfSupported) :
    SimplePager(map, profile),
    _index(nullptr),
    _filterUsage(FILTER_USAGE_NORMAL),
    _verboseWarnings(false),
    _residentTiles(std::make_shared<std::atomic_int>(0))
{
    _profile = ::getenv("OSGEARTH_BUILDINGS_PROFILE") != nullptr;

    _caches = new CacheManager();
    _caches->setName("BuildingPager Cache Manager");

    osg::StateSet* ss = getOrCreateStateSet();

    // Disable backface culling?
    ss->setAttributeAndModes(
        new osg::CullFace(), osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);

    ss->setMode(GL_BLEND, 1);

    _usingNVGL = useNVGLIfSupported && GLUtils::useNVGL();

    if (_usingNVGL)
    {
        OE_INFO << LC << "Using NVIDIA GL rendering" << std::endl;

        // Texture arena for the entire layer
        _textures = new TextureArena();
        _textures->setName("BuildingPager");
        _textures->setBindingPoint(1);
        _textures->setAutoRelease(true);
        ss->setAttribute(_textures);

        // Stores weak pointers to chonks for sharing.
        _residentData = std::make_shared<ResidentData>();
    }
}

void
BuildingPager::build()
{
    _caches = new CacheManager();
    _caches->addChild(buildRootNode());
    addChild(_caches.get());
}

void
BuildingPager::setSession(Session* session)
{
    _session = session;

    if ( session )
    {
        _compiler = new BuildingCompiler(session);

        _compiler->setUsage(_filterUsage);

        // Analyze the styles to determine the min and max LODs.
        // Styles are named by LOD.
        if ( _session->styles() )
        {
            optional<unsigned> minLOD(0u), maxLOD(0u);
            for(unsigned i=0; i<30; ++i)
            {
                std::string styleName = Stringify() << i;
                const Style* style = _session->styles()->getStyle(styleName, false);
                if ( style )
                {
                    if ( !minLOD.isSet() )
                    {
                        minLOD = i;
                    }
                    else if ( !maxLOD.isSet() || maxLOD.get() < i)
                    {
                        maxLOD = i;
                    }
                }
            }
            if ( minLOD.isSet() && !maxLOD.isSet() )
                maxLOD = minLOD.get();

            setMinLevel( minLOD.get() );
            setMaxLevel( maxLOD.get() );

            OE_INFO << LC << "Min level = " << getMinLevel() << "; max level = " << getMaxLevel() << std::endl;
        }
    }
}

void
BuildingPager::setFeatureSource(FeatureSource* features, FeatureFilterChain&& filters)
{
    _features = features;
    _filters = filters;
}

void
BuildingPager::setCatalog(BuildingCatalog* catalog)
{
    _catalog = catalog;
}

void
BuildingPager::setCompilerSettings(const CompilerSettings& settings)
{
    _compilerSettings = settings;

    // Apply the range factor from the settings:
    if (_compilerSettings.rangeFactor().isSet())
    {
        this->setRangeFactor(_compilerSettings.rangeFactor().get());
    }
}

void BuildingPager::setIndex(FeatureIndexBuilder* index)
{
    _index = index;
}

void
BuildingPager::setElevationPool(ElevationPool* pool)
{
    _elevationPool = pool;
}

void BuildingPager::setFilterUsage(FilterUsage usage)
{
   _filterUsage = usage;
}

void BuildingPager::setVerboseWarnings(bool value)
{
    _verboseWarnings = value;
}

bool
BuildingPager::cacheReadsEnabled(const osgDB::Options* readOptions) const
{
    CacheSettings* cacheSettings = CacheSettings::get(readOptions);
    return
        cacheSettings && 
        cacheSettings->getCacheBin() &&
        cacheSettings->cachePolicy()->isCacheReadable();
}

bool
BuildingPager::cacheWritesEnabled(const osgDB::Options* writeOptions) const
{
    CacheSettings* cacheSettings = CacheSettings::get(writeOptions);
    return
        cacheSettings &&
        cacheSettings->getCacheBin() &&
        cacheSettings->cachePolicy()->isCacheWriteable();
}

osg::ref_ptr<osg::Node>
BuildingPager::createNode(const TileKey& tileKey, ProgressCallback* progress)
{
    if ( !_session.valid() || !_compiler.valid() || !_features.valid() )
    {
        OE_WARN << LC << "Misconfiguration error; make sure Session and FeatureSource are set\n";
        return nullptr;
    }

    // validate the map exists
    osg::ref_ptr<const Map> map = _session->getMap();
    if (!map.valid())
        return nullptr;

    OE_PROFILING_ZONE;
    unsigned numFeatures = 0;
    
    std::string activityName("Load building tile " + tileKey.str());
    //Registry::instance()->startActivity(activityName);

    //osg::CVMarkerSeries series("PagingThread");
    //osg::CVSpan UpdateTick(series, 4, activityName.c_str());

    // result:
    osg::ref_ptr<osg::Node> node;


    // I/O Options to use throughout the build process.
    // Install an "art cache" in the read options so that images can be 
    // shared throughout the creation process. This is critical for sharing 
    // textures and especially for texture atlas usage.
    osg::ref_ptr<osgDB::Options> readOptions = Registry::cloneOrCreateOptions(_session->getDBOptions());
    readOptions->setObjectCache(_caches->_artCache.get());
    readOptions->setObjectCacheHint(osgDB::Options::CACHE_IMAGES);

    // TESTING:
    //Registry::instance()->startActivity("Bld art cache", Stringify()<<((ArtCache*)(_artCache.get()))->size());
    //Registry::instance()->startActivity("Bld tex cache", Stringify() << _texCache->_cache.size());
    //Registry::instance()->startActivity("RCache skins", Stringify() << _session->getResourceCache()->getSkinStats()._entries);
    //Registry::instance()->startActivity("RCache insts", Stringify() << _session->getResourceCache()->getInstanceStats()._entries);

    osg::ref_ptr< osgEarth::MetadataNode > metadata = new MetadataNode;

    // Holds all the final output.
    CompilerOutput output;
    output.setName(tileKey.str());
    output.setTileKey(tileKey);
    output.setIndex(_index);
    output.setMetadata(metadata.get());
    output.setTextureCache(_caches->_texCache.get());
    output.setStateSetCache(_caches->_stateSetCache.get());
    output.setFilterUsage(_filterUsage);
    // GL4/NV support:
    output.setTextureArena(_textures.get());
    output.setResidentData(_residentData);
    
    bool canceled = false;
    bool caching = true;

    //osg::CVMarkerSeries series2("SubloadParentTask");
    // Try to load from the cache.
    if (cacheReadsEnabled(readOptions.get()) && !canceled)
    {
        //osg::CVSpan UpdateTick(series2, 4, "ReadFromCache");

        node = output.readFromCache(readOptions.get(), progress);
    }

    bool fromCache = node.valid();

    canceled = canceled || (progress && progress->isCanceled());

    if (!node.valid() && !canceled)
    {     
        // fetch the style for this LOD:
        std::string styleName = Stringify() << tileKey.getLOD();
        const Style* style = _session->styles() ? _session->styles()->getStyle(styleName) : nullptr;

        // Create a cursor to iterator over the feature data:
        osg::ref_ptr<FeatureCursor> cursor = _features->createFeatureCursor(tileKey, _filters, nullptr, progress);
        if (cursor.valid() && cursor->hasMore() && !canceled)
        {
           //osg::CVSpan UpdateTick(series, 4, "buildFromScratch");
           
           osg::ref_ptr<BuildingFactory> factory = new BuildingFactory();

            factory->setSession(_session.get());
            factory->setCatalog(_catalog.get());
            factory->setOutputSRS(map->getSRS());

            // Envelope is a localized environment for optimized clamping performance:
            ElevationPool::Envelope envelope;

            Distance clampingResolution;
            UnitsType units = tileKey.getProfile()->getSRS()->getUnits();

            const AltitudeSymbol* alt = style ? style->getSymbol<AltitudeSymbol>() : nullptr;
            if (alt && alt->clampingResolution().isSet())
            {
                // use the resolution in the symbology if available
                clampingResolution.set(alt->clampingResolution()->getValue(), units);
            }
            else
            {
                // otherwise use the tilekey's resolution
                std::pair<double, double> resPair = tileKey.getResolution(osgEarth::ELEVATION_TILE_SIZE);
                clampingResolution.set(resPair.second, tileKey.getProfile()->getSRS()->getUnits());
            }

            map->getElevationPool()->prepareEnvelope(
                envelope,
                tileKey.getExtent().getCentroid(),
                clampingResolution);

            while (cursor->hasMore() && !canceled)
            {
                Feature* feature = cursor->nextFeature();
                numFeatures++;
                
                BuildingVector buildings;
                if (!factory->create(feature, tileKey.getExtent(), envelope, style, buildings, readOptions.get(), progress))
                {
                    canceled = true;
                }

                if (!canceled && !buildings.empty())
                {
                    if (output.getLocalToWorld().isIdentity())
                    {
                        output.setLocalToWorld(buildings.front()->getReferenceFrame());
                    }

                    // for indexing, if enabled:
                    output.setCurrentFeature(feature);

                    _compiler->setUsage(_filterUsage);

                    if (!_compiler->compile(buildings, output, readOptions.get(), progress))
                    {
                        canceled = true;
                    }
                }
            }

            if (!canceled)
            {
                // if we're notifying, dump out any warning messages we got building this tile
                if (_verboseWarnings && progress && !progress->message().empty())
                {
                    OE_WARN << LC 
                        << "Warnings generated for tile " << tileKey.str() << ":\n" 
                        << progress->message() << std::endl;
                }

                // set the distance at which details become visible.
                osg::BoundingSphered tileBound = getBounds(tileKey);
                output.setRange(tileBound.radius() * getRangeFactor());

                node = output.createSceneGraph(_session.get(), _compilerSettings, readOptions.get(), progress);

                // skip this if we are using NV -gw
#if 0
                if (!_usingNVGL)
                {
                    osg::MatrixTransform* mt = dynamic_cast<osg::MatrixTransform *> (node.get());
                    if (mt)
                    {
                        osg::ref_ptr<osg::Group> oqn;
                        if (osgEarth::OcclusionQueryNodeFactory::_occlusionFactory) {
                            oqn = osgEarth::OcclusionQueryNodeFactory::_occlusionFactory->createQueryNode();
                        }
                        if (oqn.get())
                        {
                            oqn->setName("BuildingPager::oqn");
                            //oqn.get()->setDebugDisplay(true);
                            while (mt->getNumChildren()) {
                                oqn.get()->addChild(mt->getChild(0));
                                mt->removeChild(mt->getChild(0));
                            }
                            mt->addChild(oqn.get());
                        }
                    }
                }
#endif
            }
            else
            {
                //OE_INFO << LC << "Tile " << tileKey.str() << " was canceled " << progress->message() << "\n";
            }
        }

        // This can go here now that we can serialize DIs and TBOs.
        if (node.valid() && !canceled)
        {
            //osg::CVSpan UpdateTick(series2, 4, "postProcess");

            // apply render symbology, if it exists.
            if (style)
                applyRenderSymbology(node.get(), *style);

            output.postProcess(node.get(), _compilerSettings, progress);
        }

        if (node.valid() && cacheWritesEnabled(readOptions.get()) && !canceled)
        {
            //osg::CVSpan UpdateTick(series2, 4, "writeToCache");

            output.writeToCache(node.get(), readOptions.get(), progress);
        }

        if (node.valid() && node->getBound().valid())
        {
            node->getOrCreateUserDataContainer()->addUserObject(new Util::TrackerTag(_residentTiles));
        }
    }

    Registry::instance()->endActivity(activityName);

    if (canceled)
    {
        OE_DEBUG << LC << "Building tile " << tileKey.str() << " - canceled" << std::endl;
        return nullptr;
    }
    else
    {
        if (metadata && node)
        {
            metadata->addChild(node);
            metadata->finalize();
            return metadata;
        }
        else
        {
            return node;
        }
    }
}

void
BuildingPager::applyRenderSymbology(osg::Node* node, const Style& style) const
{
    const RenderSymbol* render = style.get<RenderSymbol>();
    if ( render )
    {
        if ( render->depthTest().isSet() )
        {
            node->getOrCreateStateSet()->setMode(
                GL_DEPTH_TEST,
                (render->depthTest() == true? osg::StateAttribute::ON : osg::StateAttribute::OFF) | osg::StateAttribute::OVERRIDE );
        }

        if ( render->backfaceCulling().isSet() )
        {
            node->getOrCreateStateSet()->setMode(
                GL_CULL_FACE,
                (render->backfaceCulling() == true? osg::StateAttribute::ON : osg::StateAttribute::OFF) | osg::StateAttribute::OVERRIDE );
        }

#ifndef OSG_GLES2_AVAILABLE
        if ( render->clipPlane().isSet() )
        {
            GLenum mode = GL_CLIP_DISTANCE0 + render->clipPlane().value();
            node->getOrCreateStateSet()->setMode(mode, 1);
        }
#endif

        if ( render->order().isSet() || render->renderBin().isSet() )
        {
            osg::StateSet* ss = node->getOrCreateStateSet();
            int binNumber = render->order().isSet() ? (int)render->order()->eval() : ss->getBinNumber();
            std::string binName =
                render->renderBin().isSet() ? render->renderBin().get() :
                ss->useRenderBinDetails() ? ss->getBinName() : "DepthSortedBin";
            ss->setRenderBinDetails(binNumber, binName);
        }

        if ( render->minAlpha().isSet() )
        {
            DiscardAlphaFragments().install( node->getOrCreateStateSet(), render->minAlpha().value() );
        }
        

        if ( render->transparent() == true )
        {
            osg::StateSet* ss = node->getOrCreateStateSet();
            ss->setRenderingHint( ss->TRANSPARENT_BIN );
        }
    }
}