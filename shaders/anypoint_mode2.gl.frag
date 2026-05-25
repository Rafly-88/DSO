#version 330 core
// Desktop-OpenGL port of anypoint_mode2.frag.

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

uniform float thetaX_degree;
uniform float thetaY_degree;
uniform float zoom;
uniform sampler2D tex;

#define PI 3.14159265359
#define PCT_UNIT_WIDTH 1.27
#define PCT_UNIT_HEIGHT 1.27
#define FOCAL_LENGTH_FOR_ZOOM 455.9

void main() {
    vec2 uv = vUV;

    float positionX = uv.x * camera_imageWidth;
    float positionY = (1.0 - uv.y) * camera_imageHeight;

    float icx = camera_iCx * camera_ratio;
    float icy = camera_iCy * camera_ratio;
    float dcx = camera_imageWidth / 2.0 * camera_ratio;
    float dcy = camera_imageHeight / 2.0 * camera_ratio;

    float thetaX = thetaX_degree * (PI / 180.0);
    float thetaY = thetaY_degree * (PI / 180.0);

    float widthCosB       = PCT_UNIT_WIDTH  * cos(thetaY);
    float heightSinASinB  = PCT_UNIT_HEIGHT * sin(thetaX) * sin(thetaY);
    float flZoomCosASinB  = FOCAL_LENGTH_FOR_ZOOM * zoom * cos(thetaX) * sin(thetaY);
    float heightCosA      = PCT_UNIT_HEIGHT * cos(thetaX);
    float flZoomSinA      = FOCAL_LENGTH_FOR_ZOOM * zoom * sin(thetaX);
    float widthSinB       = PCT_UNIT_WIDTH  * sin(thetaY);
    float heightSinACosB  = PCT_UNIT_HEIGHT * sin(thetaX) * cos(thetaY);
    float flZoomCosACosB  = FOCAL_LENGTH_FOR_ZOOM * zoom * cos(thetaX) * cos(thetaY);

    float tempX =  (positionX - dcx) * widthCosB + (positionY - dcy) * heightSinASinB + flZoomCosASinB;
    float tempY =  (positionY - dcy) * heightCosA - flZoomSinA;
    float tempZ = -(positionX - dcx) * widthSinB + (positionY - dcy) * heightSinACosB + flZoomCosACosB;

    tempX = -tempX;
    tempY = -tempY;

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
