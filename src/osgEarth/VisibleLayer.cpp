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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "VisibleLayer"
#include "VirtualProgram"
#include "Utils"
#include "NodeUtils"
#include "ShaderLoader"
#include "SimplePager"
#include "CullingUtils"

#include <osg/BlendFunc>

using namespace osgEarth;

#define LC "[VisibleLayer] \"" << getName() << "\" "

namespace
{
    static osg::Node::NodeMask DEFAULT_LAYER_MASK = 0xffffffff;

    // Shader that just copies the uniform value into a stage global/output
    const char* opacityVS = R"(
        uniform float oe_VisibleLayer_opacityUniform;
        out float oe_layer_opacity;
        void oe_VisibleLayer_initOpacity(inout vec4 vertex)
        {
            oe_layer_opacity = clamp(oe_VisibleLayer_opacityUniform, 0.0, 1.0);
        }
    )";

    // Shader that incorporates range-based opacity (min/max range with attenuation)
    const char* rangeOpacityVS = R"(
        #pragma import_defines(OE_DISABLE_RANGE_OPACITY)
        uniform vec3 oe_VisibleLayer_ranges;
        uniform vec3 oe_Camera; // (vp width, vp height, lodscale)
        out float oe_layer_opacity;

        void oe_VisibleLayer_applyMinMaxRange(inout vec4 vertexView)
        {
          #ifndef OE_DISABLE_RANGE_OPACITY
            float minRange = oe_VisibleLayer_ranges[0];
            float maxRange = oe_VisibleLayer_ranges[1];
            float attRange = oe_VisibleLayer_ranges[2];
            float range = max(-vertexView.z, 0.0) * oe_Camera.z;
            float maxOpaqueRange = maxRange-attRange;
            float minOpaqueRange = minRange+attRange;
            float rangeOpacity =
                minRange >= maxRange ? 1.0 :
                range >= maxRange || (minRange > 0.0 && range < minRange) ? 0.0 :
                range > maxOpaqueRange ? 1.0-((range-maxOpaqueRange)/(maxRange-maxOpaqueRange)) :
                range < minOpaqueRange && minRange > 0.0 ? ((range-minRange)/(minOpaqueRange-minRange)) :
                1.0;
            oe_layer_opacity *= rangeOpacity;
            oe_layer_opacity = clamp(oe_layer_opacity, 0.0, 1.0);
          #endif
        }
    )";

    // Shader that calculates a modulation color based on the "opacity", i.e. intensity
    const char* opacityInterpolateFS = R"(
        #pragma import_defines(OE_USE_ALPHA_TO_COVERAGE)
        #pragma import_defines(OE_SELF_MANAGE_LAYER_OPACITY)
        in float oe_layer_opacity;
        void oe_VisibleLayer_setOpacity(inout vec4 color)
        {
          #if defined(OE_SELF_MANAGE_LAYER_OPACITY) || defined(OE_USE_ALPHA_TO_COVERAGE)
            return;
          #endif

          color.a *= oe_layer_opacity;
        }
    )";

    // Shader that calculates a modulation color based on the "opacity", i.e. intensity
    const char* opacityModulateFS = R"(
        const float OE_MODULATION_EXPOSURE = 2.5;
        in float oe_layer_opacity;
        void oe_VisibleLayer_setOpacity(inout vec4 color)
        {
            vec3 rgbHi = color.rgb * OE_MODULATION_EXPOSURE;
            color.rgb = clamp(mix(vec3(1), rgbHi, oe_layer_opacity), 0.0, 1.0);
            color.a = 1.0;
            oe_layer_opacity = 1.0;
        }
    )";

    const char* debugViewFS = R"(
        #extension GL_NV_fragment_shader_barycentric : enable
        #pragma vp_function oe_vl_debug, fragment_output
        out vec4 frag_out;
        void oe_vl_debug(inout vec4 color) {
            float b = min(gl_BaryCoordNV.x, min(gl_BaryCoordNV.y, gl_BaryCoordNV.z))*32.0;
            vec4 debug_color = mix(vec4(1,0,0,1), color, 0.35);
            frag_out = mix(vec4(1,0,0,1), debug_color, clamp(b,0,1));
        }
    )";

}

//------------------------------------------------------------------------

Config
VisibleLayer::Options::getConfig() const
{
    Config conf = Layer::Options::getConfig();
    conf.set( "visible", visible() );
    conf.set( "opacity", opacity() );
    conf.set( "mask", mask());
    conf.set( "min_range", minVisibleRange() );
    conf.set( "max_range", maxVisibleRange() );
    conf.set( "attenuation_range", attenuationRange() );
    conf.set( "blend", "interpolate", _blend, BLEND_INTERPOLATE );
    conf.set( "blend", "modulate", _blend, BLEND_MODULATE );
    conf.set( "nvgl", useNVGL() );
    return conf;
}

void
VisibleLayer::Options::fromConfig(const Config& conf)
{
    conf.get( "visible", _visible );
    conf.get( "opacity", _opacity);
    conf.get( "min_range", _minVisibleRange );
    conf.get( "max_range", _maxVisibleRange );
    conf.get( "attenuation_range", _attenuationRange );
    conf.get( "mask", _mask );
    conf.get( "blend", "interpolate", _blend, BLEND_INTERPOLATE );
    conf.get( "blend", "modulate", _blend, BLEND_MODULATE );
    conf.get( "nvgl", useNVGL());
}

//........................................................................

VisibleLayer::~VisibleLayer()
{
    //nop
}

void
VisibleLayer::init()
{
    Layer::init();

    _minMaxRangeShaderAdded = false;

    if (options().blend() == BLEND_INTERPOLATE)
    {
        VirtualProgram* vp = VirtualProgram::getOrCreate(getOrCreateStateSet());
        vp->setName(className());
        vp->setFunction("oe_VisibleLayer_setOpacity", opacityInterpolateFS, VirtualProgram::LOCATION_FRAGMENT_COLORING, 1.1f);
    }
}

Status
VisibleLayer::openImplementation()
{
    Status parent = Layer::openImplementation();
    if (parent.isError())
        return parent;

    if (options().visible().isSet() || options().mask().isSet())
    {
        updateNodeMasks();
    }

    return Status::NoError;
}

void
VisibleLayer::prepareForRendering(TerrainEngine* engine)
{
    Layer::prepareForRendering(engine);

    initializeUniforms();

    if (options().minVisibleRange().isSet() || options().maxVisibleRange().isSet())
    {
        initializeMinMaxRangeShader();
    }
}

void
VisibleLayer::setVisible(bool value)
{
    if (_canSetVisible)
    {
        options().visible() = value;

        updateNodeMasks();

        onVisibleChanged.fire(this);

        if (_visibleTiedToOpen)
        {
            if (value && !isOpen())
                open();
            else if (!value && isOpen())
                close();
        }
    }
}

void
VisibleLayer::updateNodeMasks()
{
    // if this layer has a scene graph node, toggle its node mask
    osg::Node* node = getNode();
    if (node)
    {
        if (!_noDrawCallback.valid())
        {
            auto cb = new ToggleVisibleCullCallback();
            node->addCullCallback(cb);
            _noDrawCallback = cb;
        }

        auto cb = dynamic_cast<ToggleVisibleCullCallback*>(_noDrawCallback.get());
        cb->setVisible(options().visible().value());
    }
}

void
VisibleLayer::setColorBlending(ColorBlending value)
{
    options().blend() = value;
    
    if (_opacityU.valid())
    {
        _opacityU = nullptr;
        initializeUniforms();
    }
}

ColorBlending
VisibleLayer::getColorBlending() const
{
    return options().blend().get();
}

osg::Node::NodeMask
VisibleLayer::getMask() const
{
    return options().mask().get();
}

void
VisibleLayer::setMask(osg::Node::NodeMask mask)
{
    // Set the new mask value
    options().mask() = mask;
    updateNodeMasks();
}

osg::Node::NodeMask
VisibleLayer::getDefaultMask()
{
    return DEFAULT_LAYER_MASK;
}

void
VisibleLayer::setDefaultMask(osg::Node::NodeMask mask)
{
    DEFAULT_LAYER_MASK = mask;
}

bool
VisibleLayer::getVisible() const
{
    if (_visibleTiedToOpen)
        return isOpen();
    else
        return options().visible().get();
}

void
VisibleLayer::initializeUniforms()
{
    if (!_opacityU.valid())
    {
        osg::StateSet* stateSet = getOrCreateStateSet();

        _opacityU = new osg::Uniform("oe_VisibleLayer_opacityUniform", (float)options().opacity().get());
        stateSet->addUniform(_opacityU.get());

        VirtualProgram* vp = VirtualProgram::getOrCreate(stateSet);
        vp->setName(className());

        vp->setFunction("oe_VisibleLayer_initOpacity", opacityVS, VirtualProgram::LOCATION_VERTEX_MODEL);

        if (options().blend() == BLEND_MODULATE)
        {
            vp->setFunction("oe_VisibleLayer_setOpacity", 
                opacityModulateFS, 
                VirtualProgram::LOCATION_FRAGMENT_COLORING,
                1.1f);

            stateSet->setAttributeAndModes(
                new osg::BlendFunc(GL_DST_COLOR, GL_ZERO),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        }
        else
        {
            // In this case the fragment shader of the layer is responsible for
            // incorporating the final value of oe_layer_opacity.

            vp->setFunction("oe_VisibleLayer_setOpacity", 
                opacityInterpolateFS, 
                VirtualProgram::LOCATION_FRAGMENT_COLORING,
                1.1f);

            stateSet->setAttributeAndModes(
                new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA),
                osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
        }
    }    

    if (!_rangeU.valid())
    {
        osg::StateSet* stateSet = getOrCreateStateSet();

        _rangeU = new osg::Uniform("oe_VisibleLayer_ranges", osg::Vec3f(
            (float)options().minVisibleRange().get(),
            (float)options().maxVisibleRange().get(),
            (float)options().attenuationRange().get()));

        stateSet->addUniform(_rangeU.get());
    }
}

void
VisibleLayer::initializeMinMaxRangeShader()
{
    initializeUniforms();

    if (!_minMaxRangeShaderAdded)
    {
        VirtualProgram* vp = VirtualProgram::getOrCreate(getOrCreateStateSet());
        vp->setName(className());
        vp->setFunction("oe_VisibleLayer_applyMinMaxRange", rangeOpacityVS, VirtualProgram::LOCATION_VERTEX_VIEW);
        _minMaxRangeShaderAdded = true;
    }
}

void
VisibleLayer::setOpacity(float value)
{
    options().opacity() = value;
    initializeUniforms();
    _opacityU->set(value);

    onOpacityChanged.fire(this);
}

float
VisibleLayer::getOpacity() const
{
    return options().opacity().get();
}

void
VisibleLayer::setMinVisibleRange( float minVisibleRange )
{
    initializeMinMaxRangeShader();

    options().minVisibleRange() = minVisibleRange;
    _rangeU->set(osg::Vec3f(
        (float)options().minVisibleRange().get(),
        (float)options().maxVisibleRange().get(),
        (float)options().attenuationRange().get()));

    onVisibleRangeChanged.fire(this);
}

float
VisibleLayer::getMinVisibleRange() const
{
    return options().minVisibleRange().get();
}

void
VisibleLayer::setMaxVisibleRange( float maxVisibleRange )
{
    initializeMinMaxRangeShader();

    options().maxVisibleRange() = maxVisibleRange;
    _rangeU->set(osg::Vec3f(
        (float)options().minVisibleRange().get(),
        (float)options().maxVisibleRange().get(),
        (float)options().attenuationRange().get()));

    forEachNodeOfType<SimplePager>(getNode(), 
        [&](SimplePager* node) {
            node->setMaxRange(maxVisibleRange);
        }
    );

    onVisibleRangeChanged.fire(this);
}

float
VisibleLayer::getMaxVisibleRange() const
{
    return options().maxVisibleRange().get();
}

void
VisibleLayer::setAttenuationRange(float value)
{
    initializeMinMaxRangeShader();

    options().attenuationRange() = value;
    _rangeU->set(osg::Vec3f(
        (float)options().minVisibleRange().get(),
        (float)options().maxVisibleRange().get(),
        (float)options().attenuationRange().get()));
}

float
VisibleLayer::getAttenuationRange() const
{
    return options().attenuationRange().get();
}

void
VisibleLayer::setEnableDebugView(bool value)
{
    if (options().debugView().get() != value)
    {
        if (value)
        {
            VirtualProgram* vp = VirtualProgram::getOrCreate(getOrCreateStateSet());
            ShaderLoader::load(vp, debugViewFS);
        }
        else
        {
            VirtualProgram* vp = VirtualProgram::get(getStateSet());
            ShaderLoader::unload(vp, debugViewFS);
        }
        options().debugView() = value;
    }
}

bool
VisibleLayer::getEnableDebugView() const
{
    return options().debugView() == true;
}