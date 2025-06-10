
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
#include "CompilerOutput"
#include "InstancedModelNode"
#include "ElevationsLodNode"
//#include <osgEarth/OEAssert>
#include <osg/LOD>
#include <osg/MatrixTransform>
#include <osg/ProxyNode>
#include <osgUtil/Optimizer>
#include <osgEarth/ShaderGenerator>
#include <osgEarth/DrawInstanced>
#include <osgEarth/Registry>
#include <osgEarth/ImageUtils>
#include <osgEarth/Session>
#include <osgEarth/ResourceCache>
#include <osgEarth/MeshFlattener>
#include <osgEarth/Chonk>
#include <osgEarth/Capabilities>
#include <osgDB/WriteFile>
#include <set>

using namespace osgEarth;
using namespace osgEarth::Buildings;

#define LC "[CompilerOutput] "

#define GEODES_ROOT           "_oeb_geo"
#define EXTERNALS_ROOT        "_oeb_ext"
#define INSTANCES_ROOT        "_oeb_inr"
#define INSTANCE_MODEL_GROUP  "_oeb_img"
#define INSTANCE_MODEL        "_oeb_inm"
#define DEBUG_ROOT            "_oeb_deb"

#define USE_LODS 1

CompilerOutput::CompilerOutput() :
_range( FLT_MAX ),
_index( 0L ),
_currentFeature( 0L ),
_filterUsage(FILTER_USAGE_NORMAL)
{
    _externalModelsGroup = new osg::Group();
    _externalModelsGroup->setName(EXTERNALS_ROOT);

    _debugGroup = new osg::Group();
    _debugGroup->setName(DEBUG_ROOT);
}

void
CompilerOutput::setLocalToWorld(const osg::Matrix& m)
{
    _local2world = m;
    _world2local.invert(m);
}

void
CompilerOutput::addDrawable(osg::Drawable* drawable)
{
    addDrawable( drawable, "" );
}

void CompilerOutput::setFilterUsage(FilterUsage usage)
{
   _filterUsage = usage;
}

void
CompilerOutput::addDrawable(osg::Drawable* drawable, const std::string& tag)
{
    if ( !drawable )
        return;

    osg::ref_ptr<osg::Geode>& geode = _geodes[tag];
    if ( !geode.valid() )
    {
        geode = new osg::Geode();
    }
    geode->addDrawable( drawable );

    if ( _index && _currentFeature )
    {
        _index->tagDrawable( drawable, _currentFeature );
    }

    if (_metadata && _currentFeature)
    {     
        auto id = _metadata->add(_currentFeature, true);
        _metadata->tagDrawable(drawable, id);
    }
}

void
CompilerOutput::addInstance(ModelResource* model, const osg::Matrix& matrix)
{
    _instances[model].push_back( std::make_pair(matrix, _currentFeature));    
}

std::string
CompilerOutput::createCacheKey() const
{
    if (_key.valid())
    {
        return Stringify() << _key.getLOD() << "_" << _key.getTileX() << "_" << _key.getTileY();
    }
    else if (!_name.empty())
    {
        return _name;
    }
    else
    {
        return "";
    }
}

namespace
{
    struct ConsolidateTextures : public TextureAndImageVisitor
    {
        TextureCache* _cache;
        ConsolidateTextures(TextureCache* cache) : _cache(cache) { }
        void apply(osg::StateSet& stateSet)
        {
            osg::StateSet::TextureAttributeList& a = stateSet.getTextureAttributeList();
            for (osg::StateSet::TextureAttributeList::iterator i = a.begin(); i != a.end(); ++i)
            {
                osg::StateSet::AttributeList& b = *i;
                for (osg::StateSet::AttributeList::iterator j = b.begin(); j != b.end(); ++j)
                {
                    osg::StateAttribute* sa = j->second.first.get();
                    if (sa)
                    {
                        osg::Texture* tex = dynamic_cast<osg::Texture*>(sa);
                        if (tex)
                        {
                            auto sharedTex = _cache->getOrInsert(tex);
                            if (sharedTex.valid() && sharedTex.get() != tex)
                            {
                                j->second.first = sharedTex;
                            }
                        }
                    }
                }
            }
        }
    };
}

osg::Node*
CompilerOutput::readFromCache(const osgDB::Options* readOptions, ProgressCallback* progress) const
{
   //GNP restored this code now works
    // This means that indirect is on. So don't use cache. Indirect cannot handle it
//    if (_filterUsage == FILTER_USAGE_ZERO_WORK_CALLBACK_BASED)
//    {
//        return 0L;;
//    }
    CacheSettings* cacheSettings = CacheSettings::get(readOptions);

    if ( !cacheSettings || !cacheSettings->getCacheBin() )
        return 0L;

    std::string cacheKey = createCacheKey();
    if (cacheKey.empty())
        return 0L;

    // read from the cache.
    osg::ref_ptr<osg::Node> node;

    osgEarth::ReadResult result = cacheSettings->getCacheBin()->readObject(cacheKey, readOptions);
    if (result.succeeded())
    {
        if (cacheSettings->cachePolicy()->isExpired(result.lastModifiedTime()))
        {
            OE_DEBUG << LC << "Tile " << _name << " is cached but expired.\n";
            return 0L;
        }

        ConsolidateTextures consolidate(_texCache.get());
        result.getNode()->accept(consolidate);

        OE_DEBUG << LC << "Loaded " << _name << " from the cache (key = " << cacheKey << ")\n";
        return result.releaseNode();
    }

    else
    {
        return 0L;
    }
}

osg::StateSet*
CompilerOutput::getSkinStateSet(SkinResource* skin, const osgDB::Options* readOptions)
{
    osg::ref_ptr<osg::StateSet>& ss = _skinStateSetCache[skin->imageURI()->full()];
    if (!ss.valid())
    {
        ss = new osg::StateSet();
        auto tex = _texCache->getOrCreate(skin, readOptions);
        if (tex.valid())
        {
            ss->setTextureAttributeAndModes(0, tex, osg::StateAttribute::ON);
        }
        //OE_INFO << LC << "Cached stateset for texture " << skin->getName() << "\n";
    }
    return ss.get();
}

void
CompilerOutput::writeToCache(osg::Node* node, const osgDB::Options* writeOptions, ProgressCallback* progress) const
{
   //GNP restored this code now works
    // This means that indirect is on. So don't use cache. Indirect cannot handle it
    //if (_filterUsage==FILTER_USAGE_ZERO_WORK_CALLBACK_BASED)
    //{
        //return;
    //}
    CacheSettings* cacheSettings = CacheSettings::get(writeOptions);

    if ( !node || !cacheSettings || !cacheSettings->getCacheBin() )
        return;

    std::string cacheKey = createCacheKey();
    if (cacheKey.empty())
        return;

    cacheSettings->getCacheBin()->writeNode(cacheKey, node, Config(), writeOptions);

    OE_DEBUG << LC << "Wrote " << _name << " to cache (key = " << cacheKey << ")\n";
}

void CompilerOutput::addInstancesNormal(osg::MatrixTransform* root, Session* session, const CompilerSettings& settings, const osgDB::Options* readOptions, ProgressCallback*) const
{
#ifdef USE_LODS
   // group to hold all instanced models:
   osg::LOD* instances = new osg::LOD();
#else
   osg::Group* instances = new osg::Group();
#endif
   instances->setName(INSTANCES_ROOT);

   // keeps one copy of each instanced model per resource:
   typedef std::map< ModelResource*, osg::ref_ptr<osg::Node> > ModelNodes;
   ModelNodes modelNodes;

   for (InstanceMap::const_iterator i = _instances.begin(); i != _instances.end(); ++i)
   {
      ModelResource* res = i->first.get();

      // look up or create the node corresponding to this instance:
      osg::ref_ptr<osg::Node>& modelNode = modelNodes[res];
      if (!modelNode.valid())
      {
         // Instance models use the GLOBAL resource cache, so that an instance model gets
         // loaded only once. Then it's cloned for each tile. That way the shader generator
         // will never touch live data. (Note that texture images are memory-cached in the
         // readOptions.)
         //
         // TODO: even though the images are shared, the texture object itself is not.
         // So it would be great to post-process this node and consolidate its texture
         // references with those in the global texture cache, thereby reducing the GPU
         // memory footprint.
         if (!session->getResourceCache()->cloneOrCreateInstanceNode(res, modelNode, readOptions))
         {
            OE_WARN << LC << "Failed to materialize resource " << res->uri()->full() << "\n";
         }
      }

      if (modelNode.valid())
      {
         modelNode->setName(INSTANCE_MODEL);

         // remove any transforms since these will screw up instancing.
         osgUtil::Optimizer optimizer;
         optimizer.optimize(
            modelNode.get(),
            //optimizer.INDEX_MESH |
            optimizer.STATIC_OBJECT_DETECTION | optimizer.FLATTEN_STATIC_TRANSFORMS);

         osg::Group* modelGroup = new osg::Group();
         modelGroup->setName(INSTANCE_MODEL_GROUP);

         // Build a normal scene graph based on MatrixTransforms, and then convert it 
         // over to use instancing if it's available.
         const InstanceVector& instanceVector = i->second;
         for (InstanceVector::const_iterator m = instanceVector.begin(); m != instanceVector.end(); ++m)
         {
            osg::MatrixTransform* modelxform = new osg::MatrixTransform(m->first);
            modelxform->addChild(modelNode.get());
            if (_index && m->second.valid())
            {
                _index->tagNode(modelxform, m->second.get());
            }

            if (_metadata && m->second.valid())
            {
                auto id = _metadata->add(m->second.get(), true);
                _metadata->tagNode(modelxform, id);
            }
            modelGroup->addChild(modelxform);
         }

#ifdef USE_LODS
         // check for a display bin for this model resource:
         const CompilerSettings::LODBin* bin = settings.getLODBin(res->tags());
         float lodScale = bin ? bin->lodScale : 1.0f;

         float maxRange = _range*lodScale;

         // find the LOD range to add it to, or create a new one if neccesary:
         bool added = false;
         for (unsigned i = 0; i < instances->getNumChildren() && !added; ++i)
         {
            if (instances->getMaxRange(i) == maxRange)
            {
               instances->getChild(i)->asGroup()->addChild(modelGroup);
               added = true;
            }
         }

         if (!added)
         {
            osg::Group* parent = new osg::Group();
            instances->addChild(parent, 0.0, maxRange);
            parent->addChild(modelGroup);
         }
#else
         instances->addChild(modelGroup);
#endif
      }
   }

   // finally add all the instance groups.
   root->addChild(instances);
}
void CompilerOutput::addInstancesZeroWorkCallbackBased(osg::MatrixTransform* root, Session* session, const CompilerSettings& settings, const osgDB::Options* readOptions, ProgressCallback* progress) const
{
   osg::Group* instances = new osg::Group();
   instances->setName(INSTANCES_ROOT);

   InstancedModelNode* instancedModelNode = new InstancedModelNode();
   instances->addChild(instancedModelNode);
   //instances->setUserData(instancedModelNode);

   for (InstanceMap::const_iterator it = _instances.begin(); it != _instances.end(); ++it)
   {
      const ModelResource* res = it->first;
      const CompilerSettings::LODBin* bin = settings.getLODBin(res->tags());
      float lodScale = bin ? bin->lodScale : 1.0f;
      float maxRange = _range*lodScale;

      const URI& uri = res->uri().value().full();
      const InstanceVector& srcMatricees = it->second;

      InstancedModelNode::Instances& dstInstances = instancedModelNode->_mapModelToInstances[uri.full()];
      InstancedModelNode::MatrixdVector& dstMatrices = dstInstances.matrices;
      InstancedModelNode::ObjectIdVector& dstObjectIds = dstInstances.objectIds;

      dstInstances.minRange = 0.0f;
      dstInstances.maxRange = maxRange;

      for (int matrixIndex = 0; matrixIndex < srcMatricees.size(); ++matrixIndex)
      {
         dstMatrices.push_back(srcMatricees[matrixIndex].first);

         if (_metadata)
         {
            osg::ref_ptr< Feature > feature = srcMatricees[matrixIndex].second;
            ObjectID id = feature.valid() ? _metadata->add(feature.get(), true) : ObjectID(0);
            dstObjectIds.push_back(id);
         }
      }
   }

   // finally add all the instance groups.
   root->addChild(instances);
}

void CompilerOutput::addInstances(osg::MatrixTransform* root, Session* session, const CompilerSettings& settings, const osgDB::Options* readOptions, ProgressCallback* progress) const
{
   // install the model instances, creating one instance group for each model.

   if (_instances.empty())
   {
      return;
   }

   if (_filterUsage==FILTER_USAGE_NORMAL)
   {
      addInstancesNormal(root, session, settings, readOptions, progress);
   }
   else
   {
      addInstancesZeroWorkCallbackBased(root, session, settings, readOptions, progress);
   }
}
osg::Node*
CompilerOutput::createSceneGraph(
    Session* session,
    const CompilerSettings& settings,
    const osgDB::Options* readOptions,
    ProgressCallback* progress) const
{
    if (_textures.valid() && _residentData)
    {
        return createSceneGraphUnifiedNV(session, settings, readOptions, progress);
    }
    else
    {
        return createSceneGraphLegacy(session, settings, readOptions, progress);
    }
}

osg::Node*
CompilerOutput::createSceneGraphUnifiedNV(
    Session* session,
    const CompilerSettings& settings,
    const osgDB::Options* readOptions,
    ProgressCallback* progress) const
{
    osg::ref_ptr<ChonkDrawable> drawable = new ChonkDrawable();

    // This object will convert OSG geometry into Chonks,
    // (which are indirect rendering units)
    ChonkFactory factory(_textures.get());

    // This user function will ensure that we can share arena textures
    // across tiles in the pager.
    ResidentData::Ptr rd = _residentData;
    auto get_or_create = [rd](osg::Texture* osgTex, bool& isNew)
    {
        std::lock_guard<std::mutex> lock(rd->_m);

        Texture::WeakPtr& weak = rd->_textures[osgTex];
        Texture::Ptr arena_tex = weak.lock();
        isNew = (arena_tex == nullptr);
        if (isNew)
        {
            arena_tex = Texture::create(osgTex);
            weak = arena_tex;
        }
        return arena_tex;
    };
    factory.setGetOrCreateFunction(get_or_create);
    
    // Parametric geometry:
#if 0
    // per-building. This works nicely but renders slowly.
    // todo: consider chopping it up into subsections using
    // an RTREE, for culling purposes?
    for (auto& geode : _geodes)
    {
        auto& group = geode.second;
        for (unsigned i = 0; i < group->getNumChildren(); ++i)
        {
            Chonk::Ptr c = Chonk::create();
            c->add(group->getChild(i), 1.0f, FLT_MAX, factory);
            drawable->add(c);

            // TODO: lods
        }
    }
#else
    // per-geode group. Renders faster since every building is a unique geometry
    for (auto& taggedGeode : _geodes)
    {
        auto& tag = taggedGeode.first;
        auto& group = taggedGeode.second;

        float far_pixel_scale = 1.0f;

        const CompilerSettings::LODBin* bin = settings.getLODBin(tag);
        if (bin)
        {
            if (bin->lodScale > 0.0f)
                far_pixel_scale = far_pixel_scale / bin->lodScale;
        }
        
        Chonk::Ptr c = Chonk::create();
        c->add(group.get(), far_pixel_scale, FLT_MAX, factory);
        drawable->add(c);
    }
#endif

    // External models:
    for (unsigned i = 0; i < _externalModelsGroup->getNumChildren(); ++i)
    {
        auto node = _externalModelsGroup->getChild(i);
        if (node)
        {
            Chonk::Ptr c = Chonk::create();
            c->add(node, 1.0f, FLT_MAX, factory);
            drawable->add(c);
        }
    }

    // Instanced models
    for (auto iter : _instances)
    {
        auto& resource = iter.first;
        auto& matrices = iter.second;

        _residentData->_m.lock();
        Chonk::Ptr chonk = _residentData->_chonks[resource.get()].lock();
        _residentData->_m.unlock();

        if (chonk == nullptr)
        {
            osg::ref_ptr<osg::Node> model;
            if (session->getResourceCache()->cloneOrCreateInstanceNode(resource, model, readOptions))
            {
                chonk = Chonk::create();
                chonk->add(model.get(), 1.0f, FLT_MAX, factory);

                _residentData->_m.lock();
                _residentData->_chonks[resource.get()] = chonk;
                _residentData->_m.unlock();
            }
            else
            {
                OE_WARN << LC << "Failed to load " << resource->uri()->full() << std::endl;
            }
        }

        if (chonk)
        {
            for (auto& matrix : matrices)
            {
                drawable->add(chonk, matrix.first * getWorldToLocal());
            }
        }
    }

    if (drawable.valid())
    {
        osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform(getLocalToWorld());
        
        root->setName("oe.BuildingLayer.root");

#if 1
        root->addChild(drawable);
#else
        // TEST code to make a bounding box
        auto geom = new osg::Geometry();

        osg::BoundingBox box = drawable->getBoundingBox();
        auto verts = new osg::Vec3Array();
        for(int i=0; i<8; ++i)
            verts->push_back(box.corner(i));
        geom->setVertexArray(verts);

        const GLushort elements[36] = {
            0,1,3, 3,2,0,
            1,5,7, 7,3,1,
            5,4,6, 6,7,5,
            4,0,2, 2,6,4,
            2,3,7, 7,6,2,
            4,5,1, 1,0,4 };
        geom->addPrimitiveSet(new osg::DrawElementsUShort(GL_TRIANGLES, 36, elements));

        Chonk::Ptr c = Chonk::create();
        c->add(geom, factory);
        ChonkDrawable* d = new ChonkDrawable();
        d->add(c);
        root->addChild(d);

        //root->addChild(geom);
#endif
        return root.release();
    }
    else
    {
        return nullptr;
    }
}


osg::Node*
CompilerOutput::createSceneGraphLegacy(
    Session*                session,
    const CompilerSettings& settings,
    const osgDB::Options*   readOptions,
    ProgressCallback*       progress) const
{
    // install the master matrix for this graph:
    osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform( getLocalToWorld() );
    root->setName("BuildingSceneGraphNode");

    if (_geodes.empty() == false)
    {
        // The Geode LOD holds each geode in its range.
        osg::LOD* elevationsLod = new osg::LOD();
        elevationsLod->setName(GEODES_ROOT);

        const GeoCircle bc = _key.getExtent().computeBoundingGeoCircle();

        for (TaggedGeodes::const_iterator g = _geodes.begin(); g != _geodes.end(); ++g)
        {
            const std::string& tag = g->first;
            const CompilerSettings::LODBin* bin = settings.getLODBin(tag);
            float minRange = bin && bin->minLodScale > 0.0f ? bc.getRadius() + _range * bin->minLodScale : 0.0f;
            float maxRange = bin ? bc.getRadius() + _range * bin->lodScale : FLT_MAX;
            elevationsLod->addChild(g->second.get(), minRange, maxRange);
        }

        if (_filterUsage == FILTER_USAGE_NORMAL)
        {
            // normal usage: just add the data directly.
            root->addChild(elevationsLod);
        }
        else
        {
            // Indirect prep: Instead of adding the geometry directly to the scene graph,
            // put in in a container node that the VRV indirect engine can find and process.

            // because the default merge limit is 10000 and there's no other way to change it
            osgUtil::Optimizer::MergeGeometryVisitor mergeGeometry;
            mergeGeometry.setTargetMaximumNumberOfVertices(250000u);
            elevationsLod->accept(mergeGeometry);

            ElevationsLodNode* elevationsLodNode = new ElevationsLodNode();
            elevationsLodNode->setName("BuildingElevationsNode");
            elevationsLodNode->elevationsLOD = elevationsLod;
            elevationsLodNode->xform = getLocalToWorld();

            root->addChild(elevationsLodNode);
        }
    }

    if ( _externalModelsGroup->getNumChildren() > 0 )
    {
        root->addChild( _externalModelsGroup.get() );
    }
    
    // Run an optimization pass before adding any debug data or models
    // NOTE: be careful; don't mess with state during optimization.
    {
        // because the default merge limit is 10000 and there's no other way to change it
        osgUtil::Optimizer::MergeGeometryVisitor mergeGeometry;
        mergeGeometry.setTargetMaximumNumberOfVertices( 250000u );
        root->accept( mergeGeometry );
    }

    addInstances(root, session, settings, readOptions, progress);

    return root.release();
}

namespace
{
    /**
     * Performs all the shader component installation on the scene graph. 
     * Once this is done the model is ready to render.
     */
    struct PostProcessNodeVisitor : public osg::NodeVisitor
    {
        osg::ref_ptr<StateSetCache> _sscache;
        unsigned _models, _instanceGroups, _geodes;
        bool _useDrawInstanced;
        ProgressCallback* _progress;
        const CompilerSettings* _settings;

        PostProcessNodeVisitor(StateSetCache* stateSetCache) : osg::NodeVisitor()
        {
            setTraversalMode(TRAVERSE_ALL_CHILDREN);
            setNodeMaskOverride(~0);

            if (stateSetCache)
            {
                _sscache = stateSetCache;
            }
            else
            {
                _sscache = new StateSetCache();
                _sscache->setMaxSize(~0);
            }

            _models = 0;
            _instanceGroups = 0;
            _geodes = 0;
            _useDrawInstanced = false;
            _progress = NULL;
            _settings = NULL;
        }

        void apply(osg::Node& node)
        {
            if (node.getName() == GEODES_ROOT)
            {
                _geodes++;
                Registry::instance()->shaderGenerator().run(&node, "Building geodes", _sscache.get());
                // no traverse necessary
            }

            else if (node.getName() == INSTANCES_ROOT && _useDrawInstanced)
            {
                DrawInstanced::install(node.getOrCreateStateSet());
                traverse(node);
            }

            else if (node.getName() == INSTANCE_MODEL_GROUP && _useDrawInstanced)
            {
                _instanceGroups++;
                DrawInstanced::convertGraphToUseDrawInstanced(node.asGroup());
                traverse(node);   
            }
            
            else if (node.getName() == INSTANCE_MODEL && _useDrawInstanced)
            {
                _models++;
                Registry::instance()->shaderGenerator().run(&node, "Resource Model", _sscache.get());
                // no traverse necessary
            }

            else if (node.getName() == INSTANCES_ROOT && !_useDrawInstanced)
            {
                // Clustering:
                osg::Group* group = node.asGroup();

                if (group)
                {
#ifdef USE_LODS
                    // Flatten each LOD range individually.
                    for (unsigned i = 0; i < group->getNumChildren(); ++i)
                    {
                        osg::Group* instanceGroup = group->getChild(i)->asGroup();

                        if (instanceGroup)
                        {
                            if (_settings->maxVertsPerCluster().isSet())
                                osgEarth::MeshFlattener::run(instanceGroup, _settings->maxVertsPerCluster().get());
                            else
                                osgEarth::MeshFlattener::run(instanceGroup);
                        }
                    }
#else
                    osgEarth::MeshFlattener::run(group);
#endif
                }

                // Generate shaders afterwards.
                Registry::instance()->shaderGenerator().run(&node, "Instances Root", _sscache.get());

                // no traverse necessary
            }

            else if (dynamic_cast<ElevationsLodNode*>(&node))
            {
                // The presence of an ElevationLodNode means this class was flagged
                // for indirect rendering. In this case we do not need to generate
                // any shaders (the target app will do so) AND we also do not need
                // to traverse down into the node.

                // NOP (no traverse)
            }

            else
            {
                traverse(node);
            }
        }
    };
}

void
CompilerOutput::postProcess(osg::Node* graph, const CompilerSettings& settings, ProgressCallback* progress) const
{
    if (!graph) return;

    PostProcessNodeVisitor ppnv(_stateSetCache.get());
    ppnv._useDrawInstanced = !settings.useClustering().get();
    ppnv._progress = progress;
    ppnv._settings = &settings;

    graph->accept(ppnv);

    //if (ppnv._sscache.valid())
    //    ppnv._sscache->protect();
}


