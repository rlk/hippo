#version 150

in  vec4 color;
out vec4 fragment;

void main()
{
    vec2 d = gl_PointCoord - vec2(0.5);

    vec3 k = exp(-29.556 * dot(d, d) / color.rgb);

    fragment = vec4(k, 1.0);
}
