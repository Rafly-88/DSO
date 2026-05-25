#version 330 core
// Desktop-OpenGL port of panorama_car.frag.

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

uniform float param1; // alpha_max
uniform float param2; // iC_alpha_degree
uniform float param3; // iC_beta_degree

uniform sampler2D tex;

#define PI 3.14159265359

void main() {
    vec2 uv = vUV;

    float flip_h      = 0.0;
    float left_crop   = 0.0;
    float right_crop  = 0.75;
    float top_crop    = 0.0;
    float bottom_crop = 1.0;

    float alpha_max       = param1;
    float iC_alpha_degree = param2;
    float iC_beta_degree  = param3;

    float current_row_ratio = top_crop  + uv.y * (bottom_crop - top_crop);
    float current_col_ratio = left_crop + uv.x * (right_crop  - left_crop);

    float icx = camera_iCx * camera_ratio;
    float icy = camera_iCy * camera_ratio;

    float iC_alpha_pivot =  iC_alpha_degree * PI / 180.0;
    float iC_beta_pivot  = -iC_beta_degree  * PI / 180.0;

    float kx = sin(iC_alpha_pivot) * cos(iC_beta_pivot);
    float ky = sin(iC_alpha_pivot) * sin(iC_beta_pivot);
    float kz = cos(iC_alpha_pivot);

    float ing_alpha    = current_row_ratio * alpha_max * PI / 180.0;
    float target_alpha = iC_alpha_pivot + ing_alpha;

    float Vx = sin(target_alpha) * cos(iC_beta_pivot);
    float Vy = sin(target_alpha) * sin(iC_beta_pivot);
    float az = cos(target_alpha);

    float kxa_x = ky * az - kz * Vy;
    float kxa_y = kz * Vx - kx * az;
    float kxa_z = kx * Vy - ky * Vx;
    float k_a   = kx * Vx + ky * Vy + kz * az;

    float ing_beta     = current_col_ratio * 2.0 * PI;
    float ing_sin_beta = sin(ing_beta);
    float ing_cos_beta = cos(ing_beta);

    float V_rot_x = ing_cos_beta * Vx + kxa_x * ing_sin_beta + kx * k_a * (1.0 - ing_cos_beta);
    float V_rot_y = ing_cos_beta * Vy + kxa_y * ing_sin_beta + ky * k_a * (1.0 - ing_cos_beta);
    float V_rot_z = ing_cos_beta * az + kxa_z * ing_sin_beta + kz * k_a * (1.0 - ing_cos_beta);

    float fish_beta  = atan(V_rot_y, V_rot_x);
    float fish_alpha = atan(sqrt(V_rot_x * V_rot_x + V_rot_y * V_rot_y), V_rot_z);
    fish_beta = PI / 2.0 - fish_beta;

    float a2 = fish_alpha * fish_alpha;
    float a3 = a2 * fish_alpha;
    float a4 = a3 * fish_alpha;
    float a5 = a4 * fish_alpha;
    float a6 = a5 * fish_alpha;
    float poly = camera_para0 * a6 + camera_para1 * a5
               + camera_para2 * a4 + camera_para3 * a3
               + camera_para4 * a2 + camera_para5 * fish_alpha;

    float alpha_ih = poly * camera_calibrationRatio * camera_ratio;

    float origPostionX = (flip_h > 0.5)
        ? floor(icx + alpha_ih * cos(fish_beta) + 0.5)
        : floor(icx - alpha_ih * cos(fish_beta) + 0.5);
    float origPostionY = floor(icy - alpha_ih * sin(fish_beta) + 0.5);

    if (origPostionX >= 0.0 && origPostionX < camera_imageWidth &&
        origPostionY >= 0.0 && origPostionY < camera_imageHeight) {
        vec2 sampleUV = vec2(origPostionX / camera_imageWidth, origPostionY / camera_imageHeight);
        fragColor = texture(tex, sampleUV);
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
