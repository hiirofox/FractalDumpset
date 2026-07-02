#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform int isGridDraw;

uniform float time;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    if (isGridDraw==1)
    {
        // ²¨ÀË£¨Ö»¸Äy£©
        worldPos.y += sin(worldPos.x * 1.0 + time*20.0) * 0.025;
        worldPos.y += cos(worldPos.z * 1.0 + time*20.0) * 0.025;
    }
    FragPos = worldPos.xyz;

    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * worldPos;
}