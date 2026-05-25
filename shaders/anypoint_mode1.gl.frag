#version 330 core
// Desktop-OpenGL port of anypoint_mode1.frag (Flutter version).
// FlutterFragCoord/uResolution replaced with vUV from fullscreen.vert.

in  vec2 vUV;
out vec4 fragmentColor;

uniform float camera_SensorWidth;
uniform float camera_SensorHeight;
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

uniform float alphaOffset;
uniform float betaOffset;
uniform float zoom;
uniform sampler2D tex;

#define PI 3.14159265359
#define PCT_UNIT_WIDTH 1.27
#define PCT_UNIT_HEIGHT 1.27
#define FOCAL_LENGTH_FOR_ZOOM 410.0

void main() {
    vec2 uv = vUV;

    float positionX = uv.x * camera_imageWidth;
    float positionY = (1.0 - uv.y) * camera_imageHeight;

    float icx = camera_iCx * camera_ratio;
    float icy = camera_iCy * camera_ratio;
    float dcx = camera_imageWidth / 2.0 * camera_ratio;
    float dcy = camera_imageHeight / 2.0 * camera_ratio;

    float mBetaOffset  = (betaOffset + 180.0) * (PI / 180.0);
    float mAlphaOffset =  alphaOffset         * (PI / 180.0);

    float widthCosB        = PCT_UNIT_WIDTH  * cos(mBetaOffset);
    float heightCosASinB   = PCT_UNIT_HEIGHT * cos(mAlphaOffset) * sin(mBetaOffset);
    float flZoomSinASinB   = FOCAL_LENGTH_FOR_ZOOM * zoom * sin(mAlphaOffset) * sin(mBetaOffset);
    float widthSinB        = PCT_UNIT_WIDTH  * sin(mBetaOffset);
    float heightCosACosB   = PCT_UNIT_HEIGHT * cos(mAlphaOffset) * cos(mBetaOffset);
    float flZoomSinACosB   = FOCAL_LENGTH_FOR_ZOOM * zoom * sin(mAlphaOffset) * cos(mBetaOffset);
    float heightSinA       = PCT_UNIT_HEIGHT * sin(mAlphaOffset);
    float flZoomCosA       = FOCAL_LENGTH_FOR_ZOOM * zoom * cos(mAlphaOffset);

    float tempX = (positionX - dcx) * widthCosB - (positionY - dcy) * heightCosASinB + flZoomSinASinB;
    float tempY = (positionX - dcx) * widthSinB + (positionY - dcy) * heightCosACosB - flZoomSinACosB;
    float tempZ = (positionY - dcy) * heightSinA + flZoomCosA;

    float alpha = atan(sqrt(tempX * tempX + tempY * tempY), tempZ);
    float beta  = 0.0;
    if (tempX != 0.0) beta = atan(tempY, tempX);
    else              beta = (tempY >= 0.0) ? PI / 2.0 : -PI / 2.0;

    float a2 = alpha * alpha;
    float a3 = a2 * alpha;
    float a4 = a3 * alpha;
    float a5 = a4 * alpha;
    float a6 = a5 * alpha;
    float poly = camera_para0 * a6 + camera_para1 * a5
               + camera_para2 * a4 + camera_para3 * a3
               + camera_para4 * a2 + camera_para5 * alpha;

    float senH = icx * camera_SensorWidth  * camera_ratio
               - poly * camera_calibrationRatio * camera_SensorHeight * camera_ratio * cos(beta);
    float senV = icy * camera_SensorHeight
               - poly * camera_calibrationRatio * camera_SensorHeight * camera_ratio * sin(beta);

    float origPostionX = senH / (camera_SensorWidth * camera_ratio);
    float origPostionY = senV /  camera_SensorHeight;

    if (origPostionX >= 0.0 && origPostionX < camera_imageWidth &&
        origPostionY >= 0.0 && origPostionY < camera_imageHeight) {
        vec2 sampleUV = vec2(origPostionX / camera_imageWidth,
                             1.0 - (origPostionY / camera_imageHeight));
        fragmentColor = texture(tex, vec2(sampleUV.x, 1.0 - sampleUV.y));
    } else {
        fragmentColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
