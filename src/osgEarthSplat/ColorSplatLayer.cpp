
#include "ColorSplatLayer"
#include "SplatShaders"
#include "NoiseTextureFactory"
#include <osgEarth/VirtualProgram>
#include <osgEarth/TerrainEngineNode>
#include <osgUtil/CullVisitor>
#include <osg/BlendFunc>
#include <osg/Drawable>
#include <cstdlib> // getenv

#define LC "[ColorSplatLayer] " << getName() << ": "

#define SPLAT_SAMPLER    "oe_splatTex"

using namespace osgEarth::Splat;

REGISTER_OSGEARTH_LAYER(colorsplatimage, ColorSplatLayer);

osgEarth::Config ColorSplatLayer::Options::getConfig() const
{
	Config conf = VisibleLayer::Options::getConfig();
	colorLayer().set(conf, "color_layer");
	conf.set("detail_base_image", _detailBaseImageURI);
	conf.set("detail_green_image", _detailGreenImageURI);
	return conf;
}

void
ColorSplatLayer::Options::fromConfig(const Config& conf)
{
	colorLayer().get(conf, "color_layer");
	conf.get("detail_base_image", _detailBaseImageURI);
	conf.get("detail_green_image", _detailGreenImageURI);
}


//........................................................................

void
ColorSplatLayer::init()
{
	VisibleLayer::init();
	setRenderType(osgEarth::Layer::RENDERTYPE_TERRAIN_SURFACE);
}


osgEarth::Status
ColorSplatLayer::openImplementation()
{
	if (GLUtils::useNVGL())
	{
		return Status(Status::ResourceUnavailable, "Layer is not compatible with NVGL");
	}

	return VisibleLayer::openImplementation();
}

void
ColorSplatLayer::addedToMap(const Map* map)
{
	VisibleLayer::addedToMap(map);

	options().colorLayer().addedToMap(map);

	if (getColorLayer())
	{
		OE_INFO << LC << "Color modulation layer is \"" << getColorLayer()->getName() << "\"" << std::endl;
		if (getColorLayer()->isShared() == false)
		{
			OE_WARN << LC << "Color modulation is not shared and is therefore being disabled." << std::endl;
			options().colorLayer().removedFromMap(map);
		}
	}
}

void
ColorSplatLayer::removedFromMap(const Map* map)
{
	VisibleLayer::removedFromMap(map);
}

void
ColorSplatLayer::prepareForRendering(TerrainEngine* engine)
{
	VisibleLayer::prepareForRendering(engine);

	TerrainResources* res = engine->getResources();
	if (res)
	{
		if (_detailBinding.valid() == false)
		{
			if (res->reserveTextureImageUnit(_detailBinding, "Detail sampler") == false)
			{
				OE_WARN << LC << "No texture unit available for splatting Noise function\n";
			}

			// Load the image
			if (options()._detailGreenImageURI.isSet())
			{
				 auto stateset = engine->getTerrainStateSet();
				 // Create the uniform for the sampler.
				 std::vector<URI> detail_textures;
				 detail_textures.push_back(options()._detailBaseImageURI.get());
				 detail_textures.push_back(options()._detailGreenImageURI.get());
				 const int num_detail_tex = detail_textures.size();
				 int sizeX = 0, sizeY = 0;
				 osg::Texture2DArray* tex = new osg::Texture2DArray();
				 float s = -1.0f, t = -1.0f;
				 for (unsigned i = 0; i < detail_textures.size(); ++i)
				 {
					 const URI& uri = detail_textures[i];
					 osg::ref_ptr<osg::Image> image = uri.getImage();
					 osg::ref_ptr<osg::Image> temp;

					 // make sure the texture array is POT - required now for mipmapping to work
					 if (s < 0)
					 {
						 s = image->s();
						 t = image->t();
						 tex->setTextureDepth(detail_textures.size());
						 tex->setInternalFormat(image->getInternalTextureFormat());
						 tex->setSourceFormat(image->getPixelFormat());
						 tex->setTextureSize(s, t, detail_textures.size());
					 }

					 if (image->s() != s || image->t() != t)
					 {
						 ImageUtils::resizeImage(image, s, t, temp);
					 }
					 else
					 {
						 temp = image;
					 }
					 tex->setImage(i, temp.get());
				 }
				 tex->setFilter(tex->MIN_FILTER, tex->NEAREST_MIPMAP_LINEAR);
				 tex->setFilter(tex->MAG_FILTER, tex->LINEAR);
				 tex->setWrap(tex->WRAP_S, tex->REPEAT);
				 tex->setWrap(tex->WRAP_T, tex->REPEAT);
				 tex->setUnRefImageDataAfterApply(true);
				 tex->setResizeNonPowerOfTwoHint(false);
				 stateset->setTextureAttribute(_detailBinding.unit(), tex);
				 stateset->addUniform(new osg::Uniform("oe_csplat_detail_sampler", _detailBinding.unit()));
			}
		}

		if (_noiseBinding.valid() == false)
		{
			auto stateset = engine->getTerrainStateSet();
			if (res->reserveTextureImageUnit(_noiseBinding, "Splat noise sampler") == false)
			{
				OE_WARN << LC << "No texture unit available for splatting Noise function\n";
			}

			NoiseTextureFactory noise;
			osg::ref_ptr<osg::Texture> noiseTexture = noise.create(256u, 4u);
			stateset->setTextureAttribute(_noiseBinding.unit(), noiseTexture.get());
			stateset->addUniform(new osg::Uniform("oe_csplat_noise_tex_sampler", _noiseBinding.unit()));
			stateset->setDefine("OE_CSPLAT_NOISE_SAMPLER", "oe_csplat_noise_tex_sampler");
		}

		osg::StateSet* stateset = this->getOrCreateStateSet();

		if (getColorLayer())
		{
			//stateset->addUniform(new osg::Uniform("oe_csplat_color_ratio", 1.0f));
			stateset->setDefine("OE_GROUND_COLOR_SAMPLER", getColorLayer()->getSharedTextureUniformName());
			stateset->setDefine("OE_GROUND_COLOR_MATRIX", getColorLayer()->getSharedTextureMatrixUniformName());
		}

		ColorSplattingShaders splatting;
		VirtualProgram* vp = VirtualProgram::getOrCreate(stateset);
		vp->setName(typeid(*this).name());
		splatting.load(vp, splatting.SplatTerrain);
		OE_DEBUG << LC << "Statesets built!! Ready!\n";
	}
}

osgEarth::ImageLayer* ColorSplatLayer::getColorLayer() const
{
	return options().colorLayer().getLayer();
}

void
ColorSplatLayer::resizeGLObjectBuffers(unsigned maxSize)
{
	VisibleLayer::resizeGLObjectBuffers(maxSize);
}

void
ColorSplatLayer::releaseGLObjects(osg::State* state) const
{
	VisibleLayer::releaseGLObjects(state);
}


osgEarth::Config
ColorSplatLayer::getConfig() const
{
	osgEarth::Config c = VisibleLayer::getConfig();
	return c;
}
