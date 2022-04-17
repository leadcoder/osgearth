
#include <osg/Vec3f>
#include <osg/Image>
#include <osg/Texture2D>

#include <algorithm>

//const float PI = 3.14159265358979323846264338327950288;
class LUTGenerator
{ 
public:
float RadicalInverse_VdC(unsigned int bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

osg::Vec2f Hammersley(unsigned int i, unsigned int N)
{
	return osg::Vec2f(float(i) / float(N), RadicalInverse_VdC(i));
}

osg::Vec3f ImportanceSampleGGX(osg::Vec2f Xi, float roughness, osg::Vec3f N)
{
	float a = roughness * roughness;

	float phi = 2.0 * osg::PI * Xi.x();
	float cosTheta = sqrt((1.0 - Xi.y()) / (1.0 + (a * a - 1.0) * Xi.y()));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	// from spherical coordinates to cartesian coordinates
	osg::Vec3f H;
	H.x() = cos(phi) * sinTheta;
	H.y() = sin(phi) * sinTheta;
	H.z() = cosTheta;

	// from tangent-space vector to world-space sample vector
	osg::Vec3f up = std::abs(N.z()) < 0.999 ? osg::Vec3f(0.0, 0.0, 1.0) : osg::Vec3f(1.0, 0.0, 0.0);
	osg::Vec3f tangent = (up ^ N);
	tangent.normalize();
	osg::Vec3f bitangent = N ^ tangent;

	osg::Vec3f sampleVec = tangent * H.x() + bitangent * H.y() + N * H.z();
	sampleVec.normalize();
	return sampleVec;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float a = roughness;
	float k = (a * a) / 2.0;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float GeometrySmith(float roughness, float NoV, float NoL)
{
	float ggx2 = GeometrySchlickGGX(NoV, roughness);
	float ggx1 = GeometrySchlickGGX(NoL, roughness);

	return ggx1 * ggx2;
}

osg::Vec2f IntegrateBRDF(float NdotV, float roughness, unsigned int samples)
{
	osg::Vec3f V;
	V.x() = sqrt(1.0 - NdotV * NdotV);
	V.y() = 0.0;
	V.z() = NdotV;

	float A = 0.0;
	float B = 0.0;

	osg::Vec3f N = osg::Vec3f(0.0, 0.0, 1.0);

	for (unsigned int i = 0u; i < samples; ++i)
	{
		osg::Vec2f Xi = Hammersley(i, samples);
		osg::Vec3f H = ImportanceSampleGGX(Xi, roughness, N);
		osg::Vec3f L = (H* (2.0f * (V * H))) - V;
		L.normalize();
		float NoL = std::max(L.z(), 0.0f);
		float NoH = std::max(H.z(), 0.0f);
		float VoH = std::max((V * H), 0.0f);
		float NoV = std::max((N * V), 0.0f);

		if (NoL > 0.0)
		{
			float G = GeometrySmith(roughness, NoV, NoL);

			float G_Vis = (G * VoH) / (NoH * NoV);
			float Fc = pow(1.0 - VoH, 5.0);

			A += (1.0 - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}

	return osg::Vec2f(A / float(samples), B / float(samples));
}

osg::Texture2D* generateLUT()
{
	int size = 128;
	osg::Image* image = new osg::Image();
	image->allocateImage(
		size,
		size,
		1,
		GL_RGBA,
		GL_FLOAT);
	//osg::Texture2D* image = new osg::Texture2D();
	
	int samples = 1024;
	using ptype = float;
	int comps = 4;
	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			float NoV = (y + 0.5f) * (1.0f / size);
			float roughness = (x + 0.5f) * (1.0f / size);
			ptype* prt = (ptype*)image->data();
			int px = y;
			int py = size - 1 - x;
			ptype* pixel = prt + py * size* comps + px * comps;
			auto sample = IntegrateBRDF(NoV, roughness, samples);
			pixel[0] = sample.x();
			pixel[1] = sample.y();
			pixel[2] = 0;
			pixel[3] = 1.0;

			//if (bits == 16)
			//	tex.store<glm::uint32>({ y, size - 1 - x }, 0, gli::packHalf2x16(IntegrateBRDF(NoV, roughness, samples)));
			//if (bits == 32)
			//	tex.store<glm::vec2>({ y, size - 1 - x }, 0, IntegrateBRDF(NoV, roughness, samples));
		}
	}
	auto tex = new osg::Texture2D(image);
	tex->setInternalFormat(GL_RGBA32F_ARB);
	tex->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::CLAMP_TO_EDGE);
	tex->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::CLAMP_TO_EDGE);
	tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
	tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
	return tex;
}
};