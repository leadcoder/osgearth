#pragma once
#include <osg/TextureCubeMap>
#include <osg/Camera>
#include <osg/Texture2D>
#include <osg/MatrixTransform>

namespace osgEarth
{
    class EnvironmentMap : public osg::MatrixTransform
    {
    public:
        EnvironmentMap(osg::Node* scene, int size = 512);
        void traverse(osg::NodeVisitor& nv) override;

        osg::ref_ptr<osg::TextureCubeMap> _cubeMap;
        std::vector<osg::Camera*> _cameras;
        osg::Vec3d _position;

        std::vector<osg::Texture2D*> _textures;
    };
}
