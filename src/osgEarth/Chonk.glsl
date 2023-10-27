#pragma vp_function oe_chonk_default_vertex_model, vertex_model, 0.0
#pragma import_defines(OE_IS_SHADOW_CAMERA)
#pragma import_defines(OE_IS_DEPTH_CAMERA)
#pragma import_defines(OE_CHONK_MAX_LOD_FOR_NORMAL_MAPS)
#pragma import_defines(OE_CHONK_MAX_LOD_FOR_PBR_MAPS)

#ifndef OE_CHONK_MAX_LOD_FOR_NORMAL_MAPS
#define OE_CHONK_MAX_LOD_FOR_NORMAL_MAPS 99
#endif

#ifndef OE_CHONK_MAX_LOD_FOR_PBR_MAPS
#define OE_CHONK_MAX_LOD_FOR_PBR_MAPS 99
#endif

struct Instance
{
    mat4 xform;
    vec2 local_uv;
    uint lod;
    float visibility[2]; // per LOD
    float radius;
    float alpha_cutoff;
    uint first_lod_cmd_index;
};
layout(binding = 0, std430) buffer Instances {
    Instance instances[];
};
layout(binding = 1, std430) buffer TextureArena {
    uint64_t textures[];
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in uint normal_technique;
layout(location = 3) in vec4 color;
layout(location = 4) in vec2 uv;
layout(location = 5) in vec3 flex;
layout(location = 6) in int albedo_index;
layout(location = 7) in int normalmap_index;
layout(location = 8) in int pbr_index;
layout(location = 9) in ivec2 extended_materials;

#define NT_DEFAULT 0
#define NT_ZAXIS 1
#define NT_HEMISPHERE 2 

// stage global
mat3 xform3;
uint chonk_lod;

// outputs
out vec3 vp_Normal;
out vec4 vp_Color;
out float oe_fade;
out vec2 oe_tex_uv;
out vec3 oe_position_vec;
out vec3 oe_position_view;
flat out uint oe_normal_technique;
flat out float oe_alpha_cutoff;
flat out uint64_t oe_albedo_tex;
flat out uint64_t oe_normal_tex;
flat out uint64_t oe_pbr_tex;
flat out ivec2 oe_extended_materials;

void oe_chonk_default_vertex_model(inout vec4 vertex)
{
    int i = gl_BaseInstance + gl_InstanceID;

    chonk_lod = instances[i].lod;

    vertex = instances[i].xform * vec4(position, 1.0);
    vp_Color = color;
    xform3 = mat3(instances[i].xform);
    vp_Normal = xform3 * normal;
    oe_normal_technique = normal_technique;
    oe_tex_uv = uv;
    oe_alpha_cutoff = instances[i].alpha_cutoff;
    oe_fade = instances[i].visibility[chonk_lod];
    oe_albedo_tex = albedo_index >= 0 ? textures[albedo_index] : 0;
    oe_extended_materials = extended_materials;

#if defined(OE_IS_SHADOW_CAMERA) || defined(OE_IS_DEPTH_CAMERA)
    oe_fade = 1.0;
    return;
#endif

    // stuff we need only for a non-depth or non-shadow camera

    if (oe_normal_technique == NT_HEMISPHERE)
    {
        // Position vector scaled by the (scaled) radius of the instance
        oe_position_vec = gl_NormalMatrix *
            ((xform3 * position.xyz) / instances[i].radius);
    }

    // FS needs this to make a TBN
    oe_position_view = (gl_ModelViewMatrix * vertex).xyz;

    // disable/ignore normal maps as directed:
    oe_normal_tex = 0;
    if (normalmap_index >= 0 && chonk_lod <= OE_CHONK_MAX_LOD_FOR_NORMAL_MAPS)
    {
        oe_normal_tex = textures[normalmap_index];
    }

    oe_pbr_tex = 0;
    if (pbr_index >= 0 && chonk_lod <= OE_CHONK_MAX_LOD_FOR_PBR_MAPS)
    {
        oe_pbr_tex = textures[pbr_index];
    }
}

[break]
#pragma vp_function oe_chonk_default_fragment, fragment
#pragma import_defines(OE_IS_SHADOW_CAMERA)
#pragma import_defines(OE_IS_DEPTH_CAMERA)
#pragma import_defines(OE_USE_ALPHA_TO_COVERAGE)
#pragma import_defines(OE_GL_RG_COMPRESSED_NORMALS)
#pragma import_defines(OE_GPUCULL_DEBUG)
#pragma import_defines(OE_CHONK_SINGLE_SIDED)

struct OE_PBR {
    float roughness;
    float ao;
    float metal;
    float brightness;
    float contrast;
} oe_pbr;

// inputs
in vec3 vp_Normal;
in vec3 oe_position_vec;
in vec3 oe_position_view;
in vec2 oe_tex_uv;
in vec3 oe_UpVectorView;
in float oe_fade;
flat in uint64_t oe_albedo_tex;
flat in uint64_t oe_normal_tex;
flat in uint64_t oe_pbr_tex;
flat in float oe_alpha_cutoff;

flat in uint oe_normal_technique;
#define NT_DEFAULT 0
#define NT_ZAXIS 1
#define NT_HEMISPHERE 2 

const float oe_normal_attenuation = 0.65;

// make a TBN from normal, vertex position, and texture uv
// https://gamedev.stackexchange.com/a/86543
mat3 make_tbn(vec3 N, vec3 p, vec2 uv)
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    // solve the linear system
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame 
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void oe_chonk_default_fragment(inout vec4 color)
{
    // When simulating normals, we invert the texture coordinates
    // for backfacing geometry
    if (!gl_FrontFacing && oe_normal_technique != NT_DEFAULT)
    {
        oe_tex_uv.s = 1.0 - oe_tex_uv.s;
    }

    // Apply the base color:
    if (oe_albedo_tex > 0)
    {
        color *= texture(sampler2D(oe_albedo_tex), oe_tex_uv);
    }

#if defined(OE_IS_SHADOW_CAMERA) || defined(OE_IS_DEPTH_CAMERA)

    // force alpha to 0 or 1 and threshold it.
    color.a = step(oe_alpha_cutoff, color.a * oe_fade);
    if (color.a < oe_alpha_cutoff)
        discard;

#else // !OE_IS_SHADOW_CAMERA && !OE_IS_DEPTH_CAMERA

#if OE_GPUCULL_DEBUG

    // apply the high fade from the instancer
    if (oe_fade <= 1.0) color.a *= oe_fade;
    else if (oe_fade <= 2.0) color.rgb = vec3(1, 0, 0);
    else if (oe_fade <= 3.0) color.rgb = vec3(1, 1, 0);
    else if (oe_fade <= 4.0) color.rgb = vec3(0, 1, 0);
    else color.rgb = vec3(1, 0, 1); // should never happen :)

#elif defined(OE_USE_ALPHA_TO_COVERAGE)

    // Adjust the alpha based on the calculated mipmap level.
    // Looks better and actually helps performance a bit as well.
    // https://bgolus.medium.com/anti-aliased-alpha-test-the-esoteric-alpha-to-coverage-8b177335ae4f
    // https://tinyurl.com/fhu4zdxz
    if (oe_albedo_tex > 0UL)
    {
        ivec2 tsize = textureSize(sampler2D(oe_albedo_tex), 0);
        vec2 cf = vec2(float(tsize.x)*oe_tex_uv.s, float(tsize.y)*oe_tex_uv.t);
        vec2 dx_vtc = dFdx(cf);
        vec2 dy_vtc = dFdy(cf);
        float delta_max_sqr = max(dot(dx_vtc, dx_vtc), dot(dy_vtc, dy_vtc));
        float miplevel = max(0, 0.5 * log2(delta_max_sqr));

        color.a *= (1.0 + miplevel * oe_alpha_cutoff);
    }

    color.a *= oe_fade;

#else // if !OE_USE_ALPHA_TO_COVERAGE

    // force alpha to 0 or 1 and threshold it.
    color.a = step(oe_alpha_cutoff, color.a * oe_fade);
    if (color.a < oe_alpha_cutoff)
        discard;

#endif

    vec3 normal_view = vp_Normal;

    if (oe_normal_technique == NT_HEMISPHERE)
    {
        // for billboarded normals, adjust the normal so its coverage
        // is a hemisphere facing the viewer. Should we recalculate the TBN here?
        // Probably, but let's not if it already looks good enough.
        vec3 v3d = oe_position_vec; // do not normalize!
        vec3 v2d = vec3(v3d.x, v3d.y, 0.0);
        float size2d = length(v2d) * 1.2021; // adjust for radius, bbox diff
        size2d = mix(0.0, oe_normal_attenuation, clamp(size2d, 0.0, 1.0));
        normal_view = mix(vec3(0, 0, 1), normalize(v2d), size2d);
    }

    vec3 pixel_normal = vec3(0, 0, 1);

    if (oe_normal_tex > 0)
    {
        vec4 n = texture(sampler2D(oe_normal_tex), oe_tex_uv);

        if (n.a == 0) // swizzled in GLUtils::storage2D to indicate a compressed normal
        {
            n.xy = n.xy*2.0 - 1.0;
            n.z = 1.0 - abs(n.x) - abs(n.y);
            float t = clamp(-n.z, 0, 1);
            n.x += (n.x > 0) ? -t : t;
            n.y += (n.y > 0) ? -t : t;
            pixel_normal = n.xyz;
        }
        else
        {
            pixel_normal = normalize(n.xyz*2.0 - 1.0);
        }
    }

    //pixel_normal = vec3(0, 0, 1); // testing


    mat3 TBN = make_tbn(
        normalize(normal_view),
        oe_position_view,
        oe_tex_uv);

    vp_Normal = TBN * pixel_normal;

    if (oe_normal_technique == NT_ZAXIS)
    {
        // attenuate the normal to a z-up orientation
        vec3 world_up = gl_NormalMatrix * vec3(0,0,1);
        vec3 face_up = TBN[1].xyz;
        vp_Normal = normalize(mix(world_up, face_up, 0.25));
    }

    if (oe_pbr_tex > 0)
    {
        // apply PBR maps:
        vec4 texel = texture(sampler2D(oe_pbr_tex), oe_tex_uv);
        oe_pbr.metal = clamp(oe_pbr.metal + texel[0], 0, 1);
        oe_pbr.roughness *= (1.0 - texel[1]);
        oe_pbr.ao *= texel[2];
    }

#endif // !OE_IS_SHADOW_CAMERA
}
