#version 330 core

in vec3 FragPos;
in vec3 Normal;
uniform int isGridDraw;
uniform vec3 cameraPos;

out vec4 FragColor;

vec3 lightPos = {0.0,5.0,0.0};

float inRangeProp(float x,float l,float h)
{
    if (x<l)return 0.0;
    if (x>h)return 1.0;
    return (x-l)/(h-l);
}
void main()
{
    if (isGridDraw == 1)
    {
        vec3 color = {0.0,2.5,2.5};
        float r = length(FragPos-cameraPos);
        r = inRangeProp(r,0.0,60.0);
        color = color * pow(1.0-r,2.0);
        FragColor = vec4(color, 1.0);
    }
    else
    {
        vec3 color = {1.0,1.0,1.0};
        
        vec3 lightPass = lightPos-FragPos;
        float dotv = dot(normalize(lightPass),normalize(Normal));
        if (dotv<0)dotv = 0;
        color *= pow(inRangeProp(dotv,-0.1,1.0),0.95);
        
        //vec3 lightPass = lightPos-FragPos;
        //vec3 clv = cross(lightPass,Normal);
        //float lv = length(clv);
        //lv = max(lv, 0.0001);
        //color /= lv;
        
        FragColor = vec4(color,1.0);
    }
}