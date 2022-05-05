
#ifndef OSGEARTHPBR_MATERIAL
#define OSGEARTHPBR_MATERIAL
#include "Export"
#include <osgEarth/VirtualProgram>
#include <osgDB/ReadFile>
#include <osg/TextureCubeMap>
#include <osg/Texture2D>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>

namespace osgEarthPBR
{
	osg::ref_ptr<osg::TextureCubeMap> loadCubeMap(const std::string& filePath);

	class OSGEARTHPBR_EXPORT PbrUberMaterial : public osg::StateSet
	{
	public:
		enum class TexUnits
		{
			COLOR = 0,
			ROUGHNESS_METAL = 1,
			NORMAL = 2,
			IBL_IRRADIANCE = 3,
			IBL_RADIANCE = 4,
			IBL_BRDF_LUT = 5,
			EMISSIVE = 6
		};

		osg::Texture2D* m_LutTexture = nullptr;

		enum class VertexAttrib
		{
			TANGENT = 6,
		};

		PbrUberMaterial(osg::Texture2D* lut_tex);

		//Debug methods

		void setIBLEnabled(bool value)
		{
			setDefine("PBR_IRRADIANCE_MAP", osg::StateAttribute::OVERRIDE | (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		void setColorMapEnabled(bool value)
		{
			setDefine("PBR_COLOR_MAP", osg::StateAttribute::OVERRIDE | (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		void setNormalMapEnabled(bool value)
		{
			setDefine("PBR_NORMAL_MAP", osg::StateAttribute::OVERRIDE | (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}
		
		void setEmissiveMapEnabled(bool value)
		{
			setDefine("PBR_EMISSIVE_MAP", osg::StateAttribute::OVERRIDE | (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}
	};

	class PbrMaterial : public osg::StateSet
	{
	public:
		void setColorMap(osg::Texture2D* tex)
		{
			if (tex)
			{
				setDefine("PBR_COLOR_MAP");
				setTextureAttributeAndModes((int)PbrUberMaterial::TexUnits::COLOR, tex);
			}
		}

		osg::Texture2D* getColorMap()
		{
			return dynamic_cast<osg::Texture2D*>(getTextureAttribute((int)PbrUberMaterial::TexUnits::COLOR, osg::StateAttribute::TEXTURE));
		}

		void setColorMapEnabled(bool value)
		{
			if (hasColorMap())
				setDefine("PBR_COLOR_MAP", (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		bool hasColorMap() const
		{
			return getDefinePair("PBR_COLOR_MAP") != nullptr;
		}

		bool getColorMapEnabled() const
		{
			auto def = getDefinePair("PBR_COLOR_MAP");
			if (!def)
				return false;
			return def->second == osg::StateAttribute::ON;
		}

		void setColorFactor(const osg::Vec3f& value)
		{
			getOrCreateUniform("oe_pbr_color_factor", osg::Uniform::FLOAT_VEC3)->set(value);
		}

		osg::Vec3f getColorFactor() const
		{
			osg::Vec3f value(1.0f, 1.0f, 1.0f);
			auto uniform = getUniform("oe_pbr_color_factor");
			if (uniform)
				uniform->get(value);
			return value;
		}


		void setMetalRoughnessMap(osg::Texture2D* tex)
		{
			if (tex)
			{
				setDefine("PBR_METALROUGHNESS_MAP");
				setTextureAttributeAndModes((int)PbrUberMaterial::TexUnits::ROUGHNESS_METAL, tex);
			}
		}

		osg::Texture2D* getMetalRoughnessMap()
		{
			return dynamic_cast<osg::Texture2D*>(getTextureAttribute((int)PbrUberMaterial::TexUnits::ROUGHNESS_METAL, osg::StateAttribute::TEXTURE));
		}

		void setMetalRoughnessMapEnabled(bool value)
		{
			if(hasMetalRoughness())
				setDefine("PBR_METALROUGHNESS_MAP", (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		bool hasMetalRoughness() const
		{
			return getDefinePair("PBR_METALROUGHNESS_MAP") != nullptr;
		}

		bool getMetalRoughnessMapEnabled() const
		{
			auto def = getDefinePair("PBR_METALROUGHNESS_MAP");
			if (!def)
				return false;
			return def->second == osg::StateAttribute::ON;
		}

		void setRoughnessFactor(float value)
		{
			getOrCreateUniform("oe_pbr_roughness_factor", osg::Uniform::FLOAT)->set(value);
		}

		float getRoughnessFactor() const
		{
			float value = 1.0f;
			auto uniform = getUniform("oe_pbr_roughness_factor");
			if (uniform)
				uniform->get(value);
			return value;
		}

		void setMetalFactor(float value)
		{
			getOrCreateUniform("oe_pbr_metal_factor", osg::Uniform::FLOAT)->set(value);
		}

		float getMetalFactor() const
		{
			float value = 1.0f;
			auto uniform = getUniform("oe_pbr_metal_factor");
			if (uniform)
				uniform->get(value);
			return value;
		}

		void setNormalMap(osg::Texture2D* tex)
		{
			if (tex)
			{
				setDefine("PBR_NORMAL_MAP");
				setTextureAttributeAndModes((int)PbrUberMaterial::TexUnits::NORMAL, tex);
			}
		}

		osg::Texture2D* getNormalMap()
		{
			return dynamic_cast<osg::Texture2D*>(getTextureAttribute((int)PbrUberMaterial::TexUnits::NORMAL, osg::StateAttribute::TEXTURE));
		}

		void setNormalMapEnabled(bool value)
		{
			if (hasNormalMap())
				setDefine("PBR_NORMAL_MAP", (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		bool hasNormalMap() const
		{
			return getDefinePair("PBR_NORMAL_MAP") != nullptr;
		}

		bool getNormalMapEnabled() const
		{
			auto def = getDefinePair("PBR_NORMAL_MAP");
			if (!def)
				return false;
			return def->second == osg::StateAttribute::ON;
		}

		void setEmissiveMap(osg::Texture2D* tex)
		{
			if (tex)
			{
				setDefine("PBR_EMISSIVE_MAP");
				setTextureAttributeAndModes((int)PbrUberMaterial::TexUnits::EMISSIVE, tex);
			}
		}

		osg::Texture2D* getEmmisiveMap()
		{
			return dynamic_cast<osg::Texture2D*>(getTextureAttribute((int)PbrUberMaterial::TexUnits::EMISSIVE, osg::StateAttribute::TEXTURE));
		}

		void setEmissiveMapEnabled(bool value)
		{
			if(hasEmmisiveMap())
				setDefine("PBR_EMISSIVE_MAP", (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		bool getEmmisiveMapEnabled() const
		{
			auto def = getDefinePair("PBR_EMISSIVE_MAP");
			if (!def)
				return false;
			return def->second == osg::StateAttribute::ON;
		}

		bool hasEmmisiveMap() const
		{
			return getDefinePair("PBR_EMISSIVE_MAP") != nullptr;
		}

		void setEmissiveFactor(const osg::Vec3& value)
		{
			getOrCreateUniform("oe_pbr_emissive_factor", osg::Uniform::FLOAT_VEC3)->set(value);
		}

		osg::Vec3f getEmissiveFactor() const
		{
			
			osg::Vec3f value(1.0f, 1.0f, 1.0f);
			auto uniform = getUniform("oe_pbr_emissive_factor");
			if (uniform)
				uniform->get(value);
			return value;
		}


		void setBrightness(float value)
		{
			getOrCreateUniform("oe_pbr_brightness", osg::Uniform::FLOAT)->set(value);
		}

		float getBrightness() const
		{
			float value = 1.0f;
			auto uniform = getUniform("oe_pbr_brightness");
			if (uniform)
				uniform->get(value);
			return value;
		}

		void setContrast(float value)
		{
			getOrCreateUniform("oe_pbr_contrast", osg::Uniform::FLOAT)->set(value);
		}

		float getContrast() const
		{
			float value = 1.0f;
			auto uniform = getUniform("oe_pbr_contrast");
			if (uniform)
				uniform->get(value);
			return value;
		}

		void setOcclusionMapEnabled(bool value)
		{
			setDefine("PBR_OCCLUSION_IN_METALROUGHNESS_MAP", (value ? osg::StateAttribute::ON : osg::StateAttribute::OFF));
		}

		bool getOcclusionMapEnabled() const
		{
			auto def = getDefinePair("PBR_OCCLUSION_IN_METALROUGHNESS_MAP");
			if (!def)
				return false;
			return def->second == osg::StateAttribute::ON;
		}

		bool hasOcclusionMap() const
		{
			return getDefinePair("PBR_OCCLUSION_IN_METALROUGHNESS_MAP") != nullptr;
		}

	};
}


#endif 
