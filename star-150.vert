#version 150

uniform mat4 P;
uniform mat4 M;
uniform float brightness;
uniform sampler2D spectrum;

in vec4 Position;
in vec2 Magnitude;

out vec4 color;

void main()
{
    float d0 = length(vec3(    Position)) * 0.306594845;
    float d1 = length(vec3(M * Position)) * 0.306594845;

    float bv =               0.850 * (Magnitude.x - Magnitude.y);
    float m0 = Magnitude.y - 0.090 * (Magnitude.x - Magnitude.y);
    float m1 = (m0 * log(10.0) - 5.0 * log(d0) + 5.0 * log(d1)) / log(10.0);

    color = mix(vec4(0.7), vec4(1.0), texture(spectrum, vec2((bv + 0.3) / 1.7, 0.0)));

    gl_PointSize = pow(10.0, -0.15 * m1) * brightness;
    gl_Position  = P * M * Position;
}
