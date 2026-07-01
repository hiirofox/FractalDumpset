#version 330 core

out vec4 FragColor;

in vec3 FragPos;   // 世界空间位置（来自vertex）
in vec3 Normal;    // 世界空间法线（来自vertex）

uniform vec3 lightPos = vec3(5.0, 5.0, 5.0);
uniform vec3 objectColor = vec3(1.0);

void main()
{
    // 1. 标准化法线
    vec3 N = normalize(Normal);

    // 2. 光方向（关键：必须在 world space）
    vec3 L = normalize(lightPos - FragPos);

    // 3. Lambert diffuse
    float diff = max(dot(N, L), 0.0);

    // 4. 环境光（避免全黑）
    float ambient = 0.25;

    float light = ambient + diff;

    // 5. 输出颜色（灰度/材质）
    vec3 color = objectColor * light;

    FragColor = vec4(color, 1.0);
}