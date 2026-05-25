#include <flutter/runtime_effect.glsl>

precision highp float;

uniform float u_nativeWidth;      // 0
uniform float u_nativeHeight;     // 1
uniform float u_sensorWidth;      // 2 (Unused, alignment padding)
uniform float u_sensorHeight;     // 3 (Unused, alignment padding)
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
uniform float param1; // 16 -> alpha_max (e.g., 110.0)
uniform float param2; // 17 -> iC_alpha_degree (e.g., 80.0)
uniform float param3; // 18 -> iC_beta_degree (e.g., 0.0)

uniform sampler2D tex;
out vec4 fragColor;

#define PI 3.14159265359

void main() {
    vec2 uv = FlutterFragCoord().xy / vec2(u_nativeWidth, u_nativeHeight);

    // Hardcoded Python configurations (can be exposed as uniforms later if needed)
    float flip_h = 0.0;
    float left_crop = 0.0;
    float right_crop = 0.75;
    float top_crop = 0.0;
    float bottom_crop = 1.0;

    float alpha_max = param1;
    float iC_alpha_degree = param2;
    float iC_beta_degree = param3;

    // Flutter uv.y is already top-to-bottom.
    float current_row_ratio = top_crop + uv.y * (bottom_crop - top_crop);
    float current_col_ratio = left_crop + uv.x * (right_crop - left_crop);

    float icx = camera_iCx * camera_ratio;
    float icy = camera_iCy * camera_ratio;

    float iC_alpha_pivot = iC_alpha_degree * PI / 180.0;
    float iC_beta_pivot = -iC_beta_degree * PI / 180.0;

    float kx = sin(iC_alpha_pivot) * cos(iC_beta_pivot);
    float ky = sin(iC_alpha_pivot) * sin(iC_beta_pivot);
    float kz = cos(iC_alpha_pivot);

    float ing_alpha = current_row_ratio * alpha_max * PI / 180.0;
    float target_alpha = iC_alpha_pivot + ing_alpha;

    float Vx = sin(target_alpha) * cos(iC_beta_pivot);
    float Vy = sin(target_alpha) * sin(iC_beta_pivot);
    float az = cos(target_alpha);

    float kxa_x = ky * az - kz * Vy;
    float kxa_y = kz * Vx - kx * az;
    float kxa_z = kx * Vy - ky * Vx;
    float k_a = kx * Vx + ky * Vy + kz * az;

    float ing_beta = current_col_ratio * 2.0 * PI;
    float ing_sin_beta = sin(ing_beta);
    float ing_cos_beta = cos(ing_beta);

    float V_rot_x = ing_cos_beta * Vx + kxa_x * ing_sin_beta + kx * k_a * (1.0 - ing_cos_beta);
    float V_rot_y = ing_cos_beta * Vy + kxa_y * ing_sin_beta + ky * k_a * (1.0 - ing_cos_beta);
    float V_rot_z = ing_cos_beta * az + kxa_z * ing_sin_beta + kz * k_a * (1.0 - ing_cos_beta);

    float fish_beta = atan(V_rot_y, V_rot_x);
    float fish_alpha = atan(sqrt(pow(V_rot_x, 2.0) + pow(V_rot_y, 2.0)), V_rot_z);
    fish_beta = PI / 2.0 - fish_beta;

    float poly = camera_para0 * pow(fish_alpha, 6.0) + 
                 camera_para1 * pow(fish_alpha, 5.0) +
                 camera_para2 * pow(fish_alpha, 4.0) + 
                 camera_para3 * pow(fish_alpha, 3.0) +
                 camera_para4 * pow(fish_alpha, 2.0) + 
                 camera_para5 * fish_alpha;

    float alpha_ih = poly * camera_calibrationRatio * camera_ratio;

    float origPostionX;
    if (flip_h > 0.5) {
        origPostionX = round(icx + alpha_ih * cos(fish_beta));
    } else {
        origPostionX = round(icx - alpha_ih * cos(fish_beta));
    }
    float origPostionY = round(icy - alpha_ih * sin(fish_beta));

    if (origPostionX >= 0.0 && origPostionX < camera_imageWidth && origPostionY >= 0.0 && origPostionY < camera_imageHeight) {
        // Map directly into Flutter's 0-1 canvas space
        vec2 sampleUV = vec2(origPostionX / camera_imageWidth, origPostionY / camera_imageHeight);
        fragColor = texture(tex, sampleUV);
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}