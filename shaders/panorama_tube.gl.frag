#version 330 core
// Desktop-OpenGL port of panorama_tube.frag.

in  vec2 vUV;
out vec4 fragColor;

uniform float camera_iCx;
uniform float camera_iCy;
uniform float camera_ratio;
uniform float camera_imageWidth;
uniform float camera_imageHeight;
uniform float camera_calibrationRatio;

uniform float camera_para0;
uniform float camera_para1;
uniform float camera_para2;
uniform float camera_para3;
uniform float camera_para4;
uniform float camera_para5;

uniform float param1; // alpha_min
uniform float param2; // alpha_max
uniform float param3; // unused

uniform sampler2D tex;

#define PI 3.14159265359

void main() {
    vec2 uv = vUV;
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

    float a2 = a_alpha_rad * a_alpha_rad;
    float a3 = a2 * a_alpha_rad;
    float a4 = a3 * a_alpha_rad;
    float a5 = a4 * a_alpha_rad;
    float a6 = a5 * a_alpha_rad;
    float poly = camera_para0 * a6 + camera_para1 * a5
               + camera_para2 * a4 + camera_para3 * a3
               + camera_para4 * a2 + camera_para5 * a_alpha_rad;

    float alpha_ih = poly * camera_calibrationRatio * camera_ratio;

    float origPostionX = floor(icx - alpha_ih * cos(a_beta) + 0.5);
    float origPostionY = floor(icy - alpha_ih * sin(a_beta) + 0.5);

    float clampedX = clamp(origPostionX, 0.0, camera_imageWidth  - 1.0);
    float clampedY = clamp(origPostionY, 0.0, camera_imageHeight - 1.0);

    vec2 sampleUV = vec2(clampedX / camera_imageWidth, clampedY / camera_imageHeight);
    fragColor = texture(tex, sampleUV);
}
