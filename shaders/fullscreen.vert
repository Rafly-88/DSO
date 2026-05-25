#version 330 core
// Fullscreen triangle generated from gl_VertexID — no VBO needed.
// vUV is emitted with origin top-left so the fragment-shader logic ported
// from the original Flutter shaders (which used FlutterFragCoord, top-left)
// works without further coordinate flips.
out vec2 vUV;
void main() {
    vec2 pos = vec2((gl_VertexID == 2) ? 3.0 : -1.0,
                    (gl_VertexID == 1) ? 3.0 : -1.0);
    vUV = vec2((pos.x + 1.0) * 0.5, 1.0 - (pos.y + 1.0) * 0.5);
    gl_Position = vec4(pos, 0.0, 1.0);
}
