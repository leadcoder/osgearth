
#pragma import_defines(OE_GROUND_COLOR_SAMPLER)
#pragma import_defines(OE_GROUND_COLOR_MATRIX)
#pragma import_defines(OE_CSPLAT_NOISE_SAMPLER)

#define DETAIL_TERRAIN_LOD 21

uniform sampler2D OE_GROUND_COLOR_SAMPLER;
uniform mat4 OE_GROUND_COLOR_MATRIX;

//color correction values
uniform float oe_csplat_imagery_contrast = 1.0;
uniform float oe_csplat_imagery_brightness = 0.0;
uniform float oe_csplat_imagery_exposure = 1.0;
uniform float oe_csplat_imagery_hue = 0.5;
uniform float oe_csplat_imagery_saturate = 0.5;
uniform float oe_csplat_imagery_value = 0.5;

//used to distort imagery tex-coords 
uniform float oe_csplat_imagery_noise_scale = 0.2;
uniform float oe_csplat_imagery_noise_lod = 22;

uniform sampler2DArray oe_csplat_detail_sampler;
uniform float oe_csplat_detail_distance = 300;
uniform float oe_csplat_detail_ratio = 0;
uniform float oe_csplat_detail_contrast = 1.0;
uniform float oe_csplat_detail_brightness = 1.0;
uniform float oe_csplat_detail_exposure = 1.0;






//from terrain sdk
uniform vec4 oe_tile_key_u;
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
    color.rgb = ((color.rgb - 0.5)*oe_csplat_imagery_contrast + 0.5) + oe_csplat_imagery_brightness;
    color.rgb = vec3(1) - exp(color.rgb * -oe_csplat_imagery_exposure);

    vec3 col_hsv = RGBtoHSV(color.rgb);
    col_hsv.y *= (oe_csplat_imagery_saturate * 2.0);
    col_hsv.x *= (oe_csplat_imagery_hue * 2.0);
#if 0 //test shadow suppression
    if(col_hsv.z < oe_ground_color_v)
    {
        float ratio = 1.0 - (oe_ground_color_v - col_hsv.z)/oe_ground_color_v;
        col_hsv.z = oe_ground_color_v;
        col_hsv.y *= ratio;
    }
#else
    col_hsv.z *= (oe_csplat_imagery_value * 2.0);
#endif
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
uniform float green_amp = 2;
uniform float red_amp = 2;
uniform float blue_amp = 2;

uniform float green_threshold = 1;

uniform float oe_shadow_lum  = 0.1;
uniform float oe_shadow_blue = 0.1;

vec4 oe_getMaterial(vec4 color)
{
    vec3 hsl = RGBtoHSL(color.rgb);

     const float red = 0.0;
     const float green = 0.3333333;
     const float blue = 0.6666667;

     // amplification factors for greenness and redness,
     // obtained empirically
     //const float green_amp = 2.0;
     //const float red_amp = 2.0;
	 //const float blue_amp = 1;

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
	 
	 float dist_to_blue = abs(blue - hsl[0]);
     if (dist_to_blue > 0.5)
         dist_to_blue = 1.0 - dist_to_blue;
     float blueness = 1.0 - 2.0 * dist_to_blue;

     if (hsl[1] < saturation_threshold)
     {
         greenness *= hsl[1] / saturation_threshold;
         redness *= hsl[1] / saturation_threshold;
		 blueness *= hsl[1] / saturation_threshold;
     }
     if (hsl[2] < lightness_threshold)
     {
         greenness *= hsl[2] / lightness_threshold;
         redness *= hsl[2] / lightness_threshold;
		 blueness *= hsl[2] / lightness_threshold;
     }

     greenness = pow(greenness, green_amp);
     redness = pow(redness, red_amp);
	 blueness = pow(blueness, blue_amp);
     
     color.g = greenness;
	 color.b = blueness;
	 color.r = redness;
     //color.b = greenness * (1.0 - hsl.z); // lighter green is less lush.
     //if(hsl.z > green_threshold)
	//	color.b = greenness;
	 //else
	//	color.b = 0;
	
	if(blueness > oe_shadow_blue && hsl.z < oe_shadow_lum)
	{
		color.g = blueness;
	}
	 
     color.a = hsl.z;
     return color;
}

#ifdef OE_CSPLAT_NOISE_SAMPLER
uniform sampler2D OE_CSPLAT_NOISE_SAMPLER;
vec4 oe_splat_getNoise(in vec2 tc)
{
    return texture(OE_CSPLAT_NOISE_SAMPLER, tc.st);
}
#else
vec4 oe_splat_getNoise(in vec2 tc)
{
    return vec4(0.0);
}
#endif

vec4 getDetail(vec2 detail_coords, float detail_layer, float detail_lod)
{
    vec4 detail_color = detail_lod > 0 ? textureLod(oe_csplat_detail_sampler, vec3(detail_coords,detail_layer),detail_lod) : texture(oe_csplat_detail_sampler, vec3(detail_coords,detail_layer));
	return detail_color;
}
//#define DESHADOW
#ifdef DESHADOW
uniform float oe_deshadow_factor = 2.9;
uniform float oe_deshadow_exp = 2.3;
#endif

vec4 oe_getColorAtDistance(float camera_distance, float detail_lod)
{
     vec2 color_tex_coord = (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st;

#ifdef OE_CSPLAT_NOISE_SAMPLER
	 float dL = oe_tile_key_u.z - oe_csplat_imagery_noise_lod;
     float lod_factor = exp2(dL);
	 vec2 noise_coords = oe_scaleToRefLOD2(oe_layer_tilec.st, oe_csplat_imagery_noise_lod);
	 vec2 offset = oe_csplat_imagery_noise_scale * lod_factor * (vec2(oe_splat_getNoise(noise_coords).x) - vec2(0.5));
     color_tex_coord += offset;
#endif
	 vec4 ground_color = texture(OE_GROUND_COLOR_SAMPLER, color_tex_coord);
     vec4 ground_mat = oe_getMaterial(ground_color);


#ifdef DESHADOW
	//try to deshadow
	const float lum_threshold = 0.3;
	const float lum = ground_mat.a;
	if(lum < lum_threshold)
	{
		float shadow_contrast = (1 - pow((oe_deshadow_factor*(lum_threshold-lum)), oe_deshadow_exp));
		ground_color.rgb = ((ground_color.rgb - 0.5) * shadow_contrast + 0.5);
	}
#endif
	

	 //try to blend upper lod to deshadow
	 //vec2 lod = textureQueryLod(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st);
	 //vec4 base_color_low = textureLod(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st,lod.y+5);
	 //base_color = ground_mat.a < 0.5 ? base_color_low : base_color;
	 //base_color.rgb = mix(base_color_low, base_color, ).rgb;
	 //return base_color;
	 //return vec4(ground_mat.a, ground_mat.a, ground_mat.a,1.0);
     
     vec2 detail_coords = oe_scaleToRefLOD2(oe_layer_tilec.st, DETAIL_TERRAIN_LOD);
     vec4 detail_color = getDetail(detail_coords,0,detail_lod);
	 vec4 dc1 = getDetail(detail_coords,1,detail_lod);
	 vec4 dc2 = getDetail(detail_coords,2,detail_lod);
	
	 detail_color = mix(detail_color, dc2, ground_mat.g);
	 detail_color = mix(detail_color, dc1, ground_mat.r); 
	
	 // brightness and contrast
     detail_color.rgb = ((detail_color.rgb - 0.5)*oe_csplat_detail_contrast + 0.5) * oe_csplat_detail_brightness;
     detail_color.rgb = vec3(1) - exp(detail_color.rgb * -oe_csplat_detail_exposure);
	 
     vec3 detail_mono = vec3(detail_color.r*0.2126 + detail_color.g*0.7152 + detail_color.b*0.0722);
     vec4 ground_color_corrected = oe_colorCorrect(ground_color);
     vec3 ground_mono = vec3(ground_color_corrected.r*0.2126 + ground_color_corrected.g*0.7152 + ground_color_corrected.b*0.0722);
     detail_color.rgb = 2.2 * mix(ground_color_corrected.rgb * detail_mono.rgb, detail_color.rgb * ground_mono, oe_csplat_detail_ratio);
    
	 //detail_color = vec4(ground_mat.r,ground_mat.g,ground_mat.b,1);
	 float fade = clamp(camera_distance, 0.0, oe_csplat_detail_distance)/oe_csplat_detail_distance;
     vec3 color = mix(detail_color.rgb, ground_color.rgb ,fade);
	 
     return vec4(color,1);
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
