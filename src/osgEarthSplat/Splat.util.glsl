#pragma vp_location fragment_coloring
#pragma import_defines(OE_GROUND_COLOR_SAMPLER)
#pragma import_defines(OE_GROUND_COLOR_MATRIX)


// Number of LOD range. Do not increase this past 25; doing so will result in precision errors
// and rendering artifacts when the camera is very close to the ground.
#define LOD_COUNT 26

const float oe_SplatRanges[26] = float[](
       100000000.0, // 0
        75000000.0, // 1
        50000000.0, // 2
        10000000.0, // 3
         7500000.0, // 4
         5000000.0, // 5
         2500000.0, // 6
         1000000.0, // 7
          500000.0, // 8
          225000.0, // 9
          150000.0, // 10
           80000.0, // 11
           30000.0, // 12
           14000.0, // 13
            4000.0, // 14
            2500.0, // 15
            1000.0, // 16
             500.0, // 17
             250.0, // 18
             125.0, // 19
              50.0, // 20
              25.0, // 21
              12.0, // 22
               6.0, // 23
               3.0, // 24
               1.0  // 25
    );

/**
 * Given a camera distance, return the two LODs it falls between and
 * the blend factor [0..1] between then.
 * in  range   = camera distace to fragment
 * in  baseLOD = LOD at which texture scale is 1.0
 * out LOD0    = near LOD
 * out LOD1    = far LOD
 * out blend   = Blend factor between LOD0 and LOD1 [0..1]
 */
void
oe_splat_getLodBlend(in float range, out float out_LOD0, out float out_rangeOuter, out float out_rangeInner, out float out_clampedRange)
{
    out_clampedRange = clamp(range, oe_SplatRanges[LOD_COUNT-1], oe_SplatRanges[0]);

    out_LOD0 = 0;

    for(int i=0; i<LOD_COUNT-1; ++i)
    {
        if ( out_clampedRange < oe_SplatRanges[i] && out_clampedRange >= oe_SplatRanges[i+1] )
        {
            out_LOD0 = float(i); //   + baseLOD;
            break;
        }
    }

    out_rangeOuter = oe_SplatRanges[int(out_LOD0)];
    out_rangeInner = oe_SplatRanges[int(out_LOD0)+1];
}

#ifdef OE_GROUND_COLOR_SAMPLER
uniform sampler2D OE_GROUND_COLOR_SAMPLER;
uniform mat4 OE_GROUND_COLOR_MATRIX;
uniform float oe_ground_color_contrast = 1.0;
uniform float oe_ground_color_brightness = 0.0;
uniform float oe_ground_color_exposure = 1.0;
uniform float oe_ground_color_saturate = 0.5;
uniform float oe_ground_color_hue = 0.5;
uniform float oe_ground_color_v = 0.5;
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


vec3 HSVtoRGB(in vec3 HSV)
{
    float H   = HSV.x;
    float R   = abs(H * 6.0 - 3.0) - 1.0;
    float G   = 2.0 - abs(H * 6.0 - 2.0);
    float B   = 2.0 - abs(H * 6.0 - 4.0);
    vec3  RGB = clamp( vec3(R,G,B), 0.0, 1.0 );
    return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}
    

vec4 oe_getGroundColor()
{
    vec4 color = texture(OE_GROUND_COLOR_SAMPLER, (OE_GROUND_COLOR_MATRIX*oe_layer_tilec).st);
    // brightness and contrast
    color.rgb = ((color.rgb - 0.5)*oe_ground_color_contrast + 0.5) + oe_ground_color_brightness;
    color.rgb = vec3(1) - exp(color.rgb * -oe_ground_color_exposure);

     vec3 col_hsv = RGBtoHSV(color.rgb);
     col_hsv.y *= (oe_ground_color_saturate * 2.0);
     col_hsv.x *= (oe_ground_color_hue * 2.0);
     col_hsv.z *= (oe_ground_color_v * 2.0);
     color.rgb = HSVtoRGB(col_hsv.rgb);


    return color;
}

#endif
