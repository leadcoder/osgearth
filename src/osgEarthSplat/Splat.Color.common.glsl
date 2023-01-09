#pragma vp_location fragment_coloring
#pragma import_defines(OE_GROUND_COLOR_SAMPLER)
#pragma import_defines(OE_GROUND_COLOR_MATRIX)

uniform sampler2D OE_GROUND_COLOR_SAMPLER;
uniform mat4 OE_GROUND_COLOR_MATRIX;
uniform float oe_ground_color_contrast = 1.0;
uniform float oe_ground_color_brightness = 0.0;
uniform float oe_ground_color_exposure = 1.0;
uniform float oe_ground_color_saturate = 0.5;
uniform float oe_ground_color_hue = 0.5;
uniform float oe_ground_color_v = 0.5;

uniform float oe_detail_distance = 300;
uniform float oe_detail_factor = 2.2;
uniform float oe_detail_ratio = 0;
in vec4 oe_layer_tilec; // unit tile coords

const float Epsilon = 1e-10;

vec3 RGBtoHSV(in vec3 RGB)
{
    vec4  P   = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0/3.0) : vec4(RGB.gb, 0.0, -1.0/3.0);
    vec4  Q   = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
    float C   = Q.x - min(Q.w, Q.y);
    float H   = abs((Q.w - Q.y) / (6.0 * C + Epsilon) + Q.z);
    vec3  HCV = vec3(H, C, Q.x);
    float S   = HCV.y / (HCV.z + Epsilon);
    return vec3(HCV.x, S, HCV.z);
}

vec3 RGBtoHSL( in vec3 c ){
 
	const vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 A = vec4(c.b, c.g, K.w, K.z);
    vec4 B = vec4(c.g, c.b, K.x, K.y);
    vec4 p = mix(A, B, step(c.b, c.g));
    A = vec4(p.x, p.y, p.w, c.r);
    B = vec4(c.r, p.y, p.z, p.x);
    vec4 q = mix(A, B, step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    const float e = 1.0e-10;
    return vec3(
        abs(q.z + (q.w - q.y) / (6.0*d + e)),
        d / (q.x + e),
        q.x);
}


vec3 HSVtoRGB(in vec3 HSV)
{
    float H   = HSV.x;
    float R   = abs(H * 6.0 - 3.0) - 1.0;
    float G   = 2.0 - abs(H * 6.0 - 2.0);
    float B   = 2.0 - abs(H * 6.0 - 4.0);
    vec3  RGB = clamp( vec3(R,G,B), 0.0, 1.0 );
    return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}


uniform sampler2DArray oe_splat_detail_sampler;
uniform vec4 oe_tile_key_u;

// cannot use the SDK version because it is VS only
vec2 oe_scaleToRefLOD2(in vec2 tc, in float refLOD)
{
    float dL = oe_tile_key_u.z - refLOD;
    float factor = exp2(dL);
    float invFactor = 1.0 / factor;
    vec2 result = tc * vec2(invFactor);

    vec2 a = floor(oe_tile_key_u.xy * invFactor);
    vec2 b = a * factor;
    vec2 c = b + factor;

    float m = floor(clamp(factor, 0.0, 1.0)); // if factor>=1.0
    result += m * (oe_tile_key_u.xy - b) / (c - b);

	//Scale to avoid stretched tiling in more northern latitudes  
    result.x *= 0.5;
    return result;
}

vec4 oe_colorCorrect(vec4 color)
{
    // brightness and contrast
    color.rgb = ((color.rgb - 0.5)*oe_ground_color_contrast + 0.5) + oe_ground_color_brightness;
    color.rgb = vec3(1) - exp(color.rgb * -oe_ground_color_exposure);

    vec3 col_hsv = RGBtoHSV(color.rgb);
    col_hsv.y *= (oe_ground_color_saturate * 2.0);
    col_hsv.x *= (oe_ground_color_hue * 2.0);
    col_hsv.z *= (oe_ground_color_v * 2.0);
    //col_hsv.z = max(col_hsv.z ,oe_ground_color_v);
    color.rgb = HSVtoRGB(col_hsv.rgb);
    return color;
}
    

vec4 oe_getGroundColor()
{
    vec4 color = texture(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st);
    color = oe_colorCorrect(color);
    return color;
}

vec4 oe_getGroundColorLod(float lod)
{
    vec4 color = textureLod(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st, lod);
    color = oe_colorCorrect(color);
    return color;
}

vec4 oe_getMaterial(vec4 color)
{
    vec3 hsl = RGBtoHSL(color.rgb);

     const float red = 0.0;
     const float green = 0.3333333;
     const float blue = 0.6666667;

     // amplification factors for greenness and redness,
     // obtained empirically
     const float green_amp = 2.0;
     const float red_amp = 5.0;

     // Set lower limits for saturation and lightness, because
     // when these levels get too low, the HUE channel starts to
     // introduce math errors that can result in bad color values
     // that we do not want. (We determined these empirically
     // using an interactive shader.)
     const float saturation_threshold = 0.2f;
     const float lightness_threshold = 0.03f;

     // "Greenness" implies vegetation
     float dist_to_green = abs(green - hsl[0]);
     if (dist_to_green > 0.5)
         dist_to_green = 1.0 - dist_to_green;
     float greenness = 1.0 - 2.0 * dist_to_green;

     // "redness" implies ruggedness/rock
     float dist_to_red = abs(red - hsl[0]);
     if (dist_to_red > 0.5)
         dist_to_red = 1.0 - dist_to_red;
     float redness = 1.0 - 2.0 * dist_to_red;

     if (hsl[1] < saturation_threshold)
     {
         greenness *= hsl[1] / saturation_threshold;
         redness *= hsl[1] / saturation_threshold;
     }
     if (hsl[2] < lightness_threshold)
     {
         greenness *= hsl[2] / lightness_threshold;
         redness *= hsl[2] / lightness_threshold;
     }

     greenness = pow(greenness, green_amp);
     redness = pow(redness, red_amp);
     
     color.g = greenness;
     color.b = greenness * (1.0 - hsl.z); // lighter green is less lush.
     color.r = redness;
     return color;
}

#define DETAIL_TERRAIN_LOD 21

vec4 oe_getColorAtDistance(float distance, int detail_lod)
{
     vec4 base_color = texture(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st);
     base_color = oe_colorCorrect(base_color);
     vec4 ground_mat = oe_getMaterial(base_color);
	 //return vec4(ground_mat.b, ground_mat.b, ground_mat.b,1.0);
     vec4 color = oe_colorCorrect(base_color);
     vec2 detail_coords = oe_scaleToRefLOD2(oe_layer_tilec.st, DETAIL_TERRAIN_LOD);

     vec4 detail_color = detail_lod > 0 ? textureLod(oe_splat_detail_sampler, vec3(detail_coords,0), detail_lod) : texture(oe_splat_detail_sampler, vec3(detail_coords,0));
	 vec4 dc1 = detail_lod > 0 ? textureLod(oe_splat_detail_sampler, vec3(detail_coords,1), detail_lod) : texture(oe_splat_detail_sampler, vec3(detail_coords,1));
	 detail_color = mix(detail_color, dc1, max(ground_mat.g,ground_mat.b));
	
     vec3 detail_mono = vec3(detail_color.r*0.2126 + detail_color.g*0.7152 + detail_color.b*0.0722);

     color.rgb = oe_detail_factor * mix(base_color.rgb * detail_mono.rgb, detail_color.rgb, oe_detail_ratio);
     float fade = clamp(distance, 0, oe_detail_distance)/oe_detail_distance;
     color.rgb = mix(color.rgb, base_color.rgb ,fade);
     return color;
}

vec4 oe_getGroundColorAtDistance(float distance)
{
     return oe_getColorAtDistance(distance, -1);
}

vec4 oe_getGrassColorAtDistance(float distance)
{
     return oe_getColorAtDistance(distance, 4);
}

vec4 oe_getTreeColorAtDistance(float distance)
{
     return oe_getColorAtDistance(distance, 4);
}
