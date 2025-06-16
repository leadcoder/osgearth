/* osgEarth
 * Copyright 2025 Pelican Mapping
 * MIT License
 */
#include <osgEarth/TiledFeatureModelLayer>
#include <osgEarth/Registry>
#include <osgEarth/FeatureStyleSorter>

using namespace osgEarth;

#define LC "[TiledFeatureModelLayer] "

#define OE_TEST OE_NULL

REGISTER_OSGEARTH_LAYER(TiledFeatureModel, TiledFeatureModelLayer);

//...........................................................................

TiledFeatureModelLayer::Options::Options() :
    TiledModelLayer::Options(),
    FeatureModelOptions(),
    GeometryCompilerOptions()
{
    fromConfig(_conf);
}

TiledFeatureModelLayer::Options::Options(const ConfigOptions& options) :
    TiledModelLayer::Options(options),
    FeatureModelOptions(options),
    GeometryCompilerOptions(options)
{
    fromConfig(_conf);
}

void TiledFeatureModelLayer::Options::fromConfig(const Config& conf)
{
    features().get(conf, "features");
    conf.get("crop_features", cropFeaturesToTile());
}

Config
TiledFeatureModelLayer::Options::getConfig() const
{
    Config conf = TiledModelLayer::Options::getConfig();

    Config fmConf = FeatureModelOptions::getConfig();
    conf.merge(fmConf);

    Config gcConf = GeometryCompilerOptions::getConfig();
    conf.merge(gcConf);

    features().set(conf, "features");

    conf.set("crop_features", cropFeaturesToTile());

    return conf;
}

void TiledFeatureModelLayer::Options::mergeConfig(const Config& conf)
{
    TiledModelLayer::Options::mergeConfig(conf);
    fromConfig(conf);
}

//...........................................................................

OE_LAYER_PROPERTY_IMPL(TiledFeatureModelLayer, bool, AlphaBlending, alphaBlending);
OE_LAYER_PROPERTY_IMPL(TiledFeatureModelLayer, bool, EnableLighting, enableLighting);

TiledFeatureModelLayer::~TiledFeatureModelLayer()
{
    //NOP
}

void
TiledFeatureModelLayer::init()
{
    TiledModelLayer::init();
}

Config
TiledFeatureModelLayer::getConfig() const
{
    Config conf = TiledModelLayer::getConfig();
    return conf;
}

void
TiledFeatureModelLayer::setFeatureSource(FeatureSource* source)
{
    if (getFeatureSource() != source)
    {
        options().features().setLayer(source);

        if (source && source->getStatus().isError())
        {
            setStatus(source->getStatus());
            return;
        }

        dirty();
    }
}

FeatureSource*
TiledFeatureModelLayer::getFeatureSource() const
{
    return options().features().getLayer();
}

const FeatureProfile*
TiledFeatureModelLayer::getFeatureProfile() const
{
    FeatureSource* fs = getFeatureSource();
    return fs ? fs->getFeatureProfile() : nullptr;
}

void
TiledFeatureModelLayer::setStyleSheet(StyleSheet* value)
{
    if (getStyleSheet() != value)
    {
        options().styleSheet().setLayer(value);
        dirty();
    }
}

StyleSheet*
TiledFeatureModelLayer::getStyleSheet() const
{
    return options().styleSheet().getLayer();
}

Status
TiledFeatureModelLayer::openImplementation()
{
    Status parent = TiledModelLayer::openImplementation();
    if (parent.isError())
        return parent;

    Status fsStatus = options().features().open(getReadOptions());
    if (fsStatus.isError())
        return fsStatus;

    Status ssStatus = options().styleSheet().open(getReadOptions());
    if (ssStatus.isError())
        return ssStatus;

    return Status::NoError;
}

Status
TiledFeatureModelLayer::closeImplementation()
{
    super::closeImplementation();
    options().features().close();
    options().styleSheet().close();
    return getStatus();
}

const GeoExtent&
TiledFeatureModelLayer::getExtent() const
{
    static GeoExtent s_invalid;

    auto* fp = getFeatureProfile();
    return fp ? fp->getExtent() : s_invalid;
}

void
TiledFeatureModelLayer::addedToMap(const Map* map)
{
    OE_TEST << LC << "addedToMap" << std::endl;
    
    options().features().addedToMap(map);
    options().styleSheet().addedToMap(map);

    if (getFeatureSource() && getStyleSheet())
    {
        if (getFeatureSource()->getStatus().isError())
        {
            setStatus(getFeatureSource()->getStatus());
            return;
        }

        // Save a reference to the map since we'll need it to
        // create a new session object later.
        _session = new Session(
            map,
            getStyleSheet(),
            getFeatureSource(),
            getReadOptions());

        // connect the session to the features:
        _session->setFeatureSource(getFeatureSource());
        _session->setResourceCache(new ResourceCache());

        if (options().featureIndexing()->enabled() == true)
        {
            FeatureSourceIndexOptions indexOptions;
            indexOptions.enabled() = true;
            indexOptions.embedFeatures() = true;

            _featureIndex = new FeatureSourceIndex(
                getFeatureSource(),
                Registry::objectIndex(),
                indexOptions);
        }
    }
    else
    {
        OE_WARN << LC << "Missing either feature source or stylesheet - nothing will render" << std::endl;
    }

    _filters = FeatureFilterChain::create(options().filters(), getReadOptions());

    // invoke the superclass, where the SimplePager will get created.
    TiledModelLayer::addedToMap(map);    
}

void
TiledFeatureModelLayer::removedFromMap(const Map* map)
{
    super::removedFromMap(map);

    _filters.clear();
    _session = nullptr;
    _featureIndex = nullptr;

    options().features().removedFromMap(map);
    options().styleSheet().removedFromMap(map);
}

osg::ref_ptr<osg::Node>
TiledFeatureModelLayer::createTileImplementation(const TileKey& key, ProgressCallback* progress) const
{
    if (progress && progress->isCanceled())
        return nullptr;

    // Get features for this key
    // set up for feature indexing if appropriate:
    osg::ref_ptr<FeatureSourceIndexNode> index = 0L;
    if (_featureIndex.valid())
    {
        index = new FeatureSourceIndexNode(_featureIndex.get());
    }

    GeomFeatureNodeFactory factory(options());

    if (progress && progress->isCanceled())
        return nullptr;

    OE_SOFT_ASSERT_AND_RETURN(getFeatureProfile(), {});
    
    FilterContext context(_session.get(), getFeatureProfile(), key.getExtent(), index);
    Query query(key);


    osg::ref_ptr<osg::Group> group = new osg::Group();

    auto compile = [&](const Style& style, FeatureList& features, ProgressCallback* progress)
        {
            if (options().cropFeaturesToTile() == true)
            {
                FeatureList temp;
                temp.swap(features);

                auto extent = key.getExtent().transform(getFeatureProfile()->getSRS());
                for (auto& feature : temp)
                {
                    auto cropped = feature->getGeometry()->crop(extent.bounds());
                    if (cropped)
                    {
                        feature->setGeometry(cropped);
                        features.emplace_back(feature);
                    }
                }
            }

            for (auto& feature : features)
            {
                feature->set("level", (long long)key.getLOD());
            }

            osg::ref_ptr<osg::Node> node;
            FeatureListCursor cursor(features);
            if (factory.createOrUpdateNode(&cursor, style, context, node, query))
            {
                group->addChild(node);
            }
        };

    FeatureStyleSorter().sort(key, {}, _session.get(), _filters, nullptr, compile, progress);

    if (group->getNumChildren() == 0 || group->getBound().valid() == false)
    {
        return {};
    }

    if (index.valid())
    {
        index->addChild(group);
        group = index;
    }

    return group;
}

const Profile*
TiledFeatureModelLayer::getProfile() const
{
    static const Profile* s_fallback = Profile::create(Profile::GLOBAL_GEODETIC);

    auto* fs = getFeatureSource();
    OE_SOFT_ASSERT_AND_RETURN(fs, nullptr);

    OE_SOFT_ASSERT_AND_RETURN(getFeatureProfile(), nullptr);

    // first try the tiling profile if there is one.
    auto profile = getFeatureProfile()->getTilingProfile();

    // otherwise, this is an untiled source (like a local shapefile) so will
    // try to construct a profile from its extent
    if (!profile && getFeatureProfile()->getExtent().isValid())
    {
        profile = Profile::create(getFeatureProfile()->getExtent());
    }

    // failing all that, fall back on a default.
    return profile ? profile : s_fallback;
}

unsigned
TiledFeatureModelLayer::getMinLevel() const
{
    if (options().minLevel().isSet())
    {
        return options().minLevel().value();
    }
    else
    {
        OE_SOFT_ASSERT_AND_RETURN(getFeatureProfile(), 0);
        return getFeatureProfile()->getFirstLevel();
    }
}

unsigned
TiledFeatureModelLayer::getMaxLevel() const
{
    if (options().maxLevel().isSet())
    {
        return options().maxLevel().value();
    }
    else
    {
        OE_SOFT_ASSERT_AND_RETURN(getFeatureProfile(), 0);

        if (getFeatureProfile()->isTiled())
        {
            return getFeatureProfile()->getMaxLevel();
        }
        else
        {
            // in the case of an un-tiled feature source (like a local shapefile
            // or geojson file), min level should == the max level so there is 
            // only one level of detail.
            return getMinLevel();
        }
    }
}
