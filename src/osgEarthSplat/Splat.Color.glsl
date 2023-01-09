#pragma vp_entryPoint oe_splat_color_vertex
#pragma vp_location   vertex_view

out vec4 oe_layer_tilec;
out float oe_lod_cam_dist;
uniform vec3 oe_Camera; // (vp width, vp height, LOD scale)

void oe_splat_color_vertex(inout vec4 VertexVIEW)
{
    // range from camera to vertex
    oe_lod_cam_dist = -VertexVIEW.z * oe_Camera.z; // apply LOD scale
}

[break]

#pragma vp_entryPoint oe_splat_color_fragment
#pragma vp_location   fragment_coloring
#pragma vp_order      1.1

#pragma include Splat.Color.common.glsl

vec4 oe_getGroundColor();
vec4 oe_getGroundColorAtDistance(float distance);
in float oe_lod_cam_dist;
void oe_splat_color_fragment(inout vec4 color)
{
    color.rgb = oe_getGroundColorAtDistance(oe_lod_cam_dist).rgb;
}
