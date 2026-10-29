#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    vec4 params; // x: 1 = BT.709, 0 = BT.601; y: 1 = full range, 0 = limited (16-235)
} ubuf;

layout(binding = 1) uniform sampler2D texY;
layout(binding = 2) uniform sampler2D texU;
layout(binding = 3) uniform sampler2D texV;

void main()
{
    float y = texture(texY, v_uv).r;
    float u = texture(texU, v_uv).r - 0.5;
    float v = texture(texV, v_uv).r - 0.5;
    if (ubuf.params.y < 0.5) {
        y = (y - 16.0 / 255.0) * (255.0 / 219.0);
        u *= 255.0 / 224.0;
        v *= 255.0 / 224.0;
    }
    vec3 rgb;
    if (ubuf.params.x > 0.5)
        rgb = vec3(y + 1.5748 * v, y - 0.1873 * u - 0.4681 * v, y + 1.8556 * u);
    else
        rgb = vec3(y + 1.402 * v, y - 0.344136 * u - 0.714136 * v, y + 1.772 * u);
    fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
