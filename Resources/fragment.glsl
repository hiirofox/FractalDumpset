#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos = vec3(5.0, 5.0, 5.0);
uniform vec3 viewPos   = vec3(0.0, 0.0, 5.0);

uniform vec3 objectColor = vec3(1.0, 1.0, 1.0);
uniform vec3 fogColor    = vec3(0.5, 0.6, 0.7);

uniform float fogStart = 5.0;
uniform float fogEnd   = 20.0;

void main()
{
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);

    float diff = max(dot(N, L), 0.0);

    float ambient = 0.25;

    vec3 color = objectColor * (ambient + diff);

    float dist = length(viewPos - FragPos);

    float fogFactor = (fogEnd - dist) / (fogEnd - fogStart);
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    color = mix(fogColor, color, fogFactor);

    FragColor = vec4(color, 1.0);
}