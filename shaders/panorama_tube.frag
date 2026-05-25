#include <flutter/runtime_effect.glsl>

precision highp float;

uniform float u_nativeWidth;      // 0
uniform float u_nativeHeight;     // 1
uniform float u_sensorWidth;      // 2 (Unused)
uniform float u_sensorHeight;     // 3 (Unused)
uniform float camera_iCx;         // 4
uniform float camera_iCy;         // 5
uniform float camera_ratio;       // 6
uniform float camera_imageWidth;  // 7
uniform float camera_imageHeight; // 8
uniform float camera_calibrationRatio; // 9
uniform float camera_para0;       // 10
uniform float camera_para1;       // 11
uniform float camera_para2;       // 12
uniform float camera_para3;       // 13
uniform float camera_para4;       // 14
uniform float camera_para5;       // 15

// Unified mapped parameters
uniform float param1; // 16 -> alpha_min (e.g., 10.0)
uniform float param2; // 17 -> alpha_max (e.g., 110.0)
uniform float param3; // 18 -> unused

uniform sampler2D tex;
out vec4 fragColor;

#define PI 3.14159265359

void main() {
    vec2 uv = FlutterFragCoord().xy / vec2(u_nativeWidth, u_nativeHeight);

    float alpha_min = param1;
    float alpha_max = param2;

    float current_row_ratio = uv.y;
    float current_col_ratio = uv.x;

    float icx = camera_iCx * camera_ratio;
    float icy = camera_iCy * camera_ratio;

    float D2R = PI / 180.0;
    float R2D = 180.0 / PI;

    float h_bound_0 = 90.0 - alpha_min;
    float h_bound_1 = 90.0 - alpha_max;

    float zone_0 = tan(h_bound_0 * D2R);
    float zone_1 = tan(h_bound_1 * D2R);
    float zone_length = zone_0 - zone_1;

    float a_alpha = 90.0 - atan(zone_0 - zone_length * current_row_ratio) * R2D;
    float a_alpha_rad = a_alpha * D2R;

    float a_beta = current_col_ratio * 2.0 * PI;
    a_beta = PI / 2.0 - a_beta;

    float poly = camera_para0 * pow(a_alpha_rad, 6.0) + 
                 camera_para1 * pow(a_alpha_rad, 5.0) +
                 camera_para2 * pow(a_alpha_rad, 4.0) + 
                 camera_para3 * pow(a_alpha_rad, 3.0) +
                 camera_para4 * pow(a_alpha_rad, 2.0) + 
                 camera_para5 * a_alpha_rad;

    float alpha_ih = poly * camera_calibrationRatio * camera_ratio;

    float origPostionX = round(icx - alpha_ih * cos(a_beta));
    float origPostionY = round(icy - alpha_ih * sin(a_beta));

    float clampedX = clamp(origPostionX, 0.0, camera_imageWidth - 1.0);
    float clampedY = clamp(origPostionY, 0.0, camera_imageHeight - 1.0);

    vec2 sampleUV = vec2(clampedX / camera_imageWidth, clampedY / camera_imageHeight);
    fragColor = texture(tex, sampleUV);
}