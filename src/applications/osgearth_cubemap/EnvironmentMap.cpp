
#include "EnvironmentMap.h"
#include <osg/TextureCubeMap>
#include <osg/Camera>
#include <osg/Texture2D>

namespace osgEarth
{
    namespace
    {
        // Create a cubemap texture
        osg::ref_ptr<osg::TextureCubeMap> createCubeMapTexture(int size)
        {
            osg::ref_ptr<osg::TextureCubeMap> cubemap = new osg::TextureCubeMap();
            cubemap->setInternalFormat(GL_RGBA);
            cubemap->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            cubemap->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            cubemap->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            cubemap->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            cubemap->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);

            for (unsigned int face = 0; face < 6; ++face)
            {
                cubemap->setImage(static_cast<osg::TextureCubeMap::Face>(face), new osg::Image());
                cubemap->getImage(face)->allocateImage(size, size, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            }

            return cubemap;
        }

        // Create a camera for rendering to a texture
        osg::Camera* createCubeMapCamera(osg::TextureCubeMap* cubemap, int face, int size, osg::Texture2D* texture = nullptr)
        {
            osg::Camera* camera = new osg::Camera();
            camera->setName("CubeCam" + std::to_string(face));
            // Set up the camera to render into the cubemap texture
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
            if (texture)
            {
                camera->attach(osg::Camera::COLOR_BUFFER, texture, 0, 0);
            }
            else
                camera->attach(osg::Camera::COLOR_BUFFER, cubemap, 0, face);

            camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            camera->setClearColor(osg::Vec4(0.0, 0.0, 0.0, 1.0));
            camera->setRenderOrder(osg::Camera::PRE_RENDER);
            // Setup viewport and projection
            camera->setViewport(0, 0, size, size);
            camera->setProjectionMatrixAsPerspective(90.0f, 1.0f, 100.0f, 1000000.0f);
            //camera->setCullMask(~2U);
            camera->setStateSet(new osg::StateSet());
            return camera;
        }
    }

    EnvironmentMap::EnvironmentMap(osg::Node* scene, int size):
        _cubeMap(createCubeMapTexture(size))
    {
        for (int face = 0; face < 6; ++face)
        {
            osg::Texture2D* tex = new osg::Texture2D();
            tex->setInternalFormat(GL_RGBA);
            tex->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            tex->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            tex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            tex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            tex->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
            tex->setImage(new osg::Image());
            tex->getImage()->allocateImage(size, size, 1, GL_RGBA, GL_UNSIGNED_BYTE);
            osg::Camera* camera = createCubeMapCamera(_cubeMap, face, size, tex);
            camera->addChild(scene);
            _cameras.push_back(camera);
            addChild(camera);
            _textures.push_back(tex);
        }
    }

    void EnvironmentMap::traverse(osg::NodeVisitor& nv)
    {
        if (nv.getVisitorType() == nv.UPDATE_VISITOR)
        {
            // compute the position of the center of the reflector subgraph
            typedef std::pair<osg::Vec3, osg::Vec3> ImageData;
            const ImageData id[] =
            {
                ImageData(osg::Vec3(1,  0,  0), osg::Vec3(0, -1,  0)), // +X
                ImageData(osg::Vec3(-1,  0,  0), osg::Vec3(0, -1,  0)), // -X
                ImageData(osg::Vec3(0,  1,  0), osg::Vec3(0,  0,  1)), // +Y
                ImageData(osg::Vec3(0, -1,  0), osg::Vec3(0,  0, -1)), // -Y
                ImageData(osg::Vec3(0,  0,  1), osg::Vec3(0, -1,  0)), // +Z
                ImageData(osg::Vec3(0,  0, -1), osg::Vec3(0, -1,  0))  // -Z
            };

            for (size_t i = 0; i < 6 && i < _cameras.size(); ++i)
            {
                auto position = _position;// getMatrix().getTrans();
                _cameras[i]->setReferenceFrame(osg::Camera::ABSOLUTE_RF_INHERIT_VIEWPOINT);
                _cameras[i]->setCullingMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
                _cameras[i]->setProjectionMatrixAsPerspective(90.0f, 1.0f, 100.0f, 100000.0f);
                _cameras[i]->setViewMatrixAsLookAt(position, position + id[i].first, id[i].second);
                osg::Uniform* uniform = _cameras[i]->getOrCreateStateSet()->getOrCreateUniform("osg_ViewMatrix", osg::Uniform::FLOAT_MAT4);
                uniform->set(_cameras[i]->getViewMatrix());
            }
        }
        osg::MatrixTransform::traverse(nv);
    }
}
