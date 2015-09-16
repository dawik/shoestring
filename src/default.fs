#version 150

uniform struct Light {
   vec3 position;
   vec3 intensities;
   float attenuation;
   float ambientCoefficient;
} light;

uniform mat4 model;
uniform mat4 camera;
uniform mat4 perspective;
uniform vec4 color;
uniform int texid;
uniform int isHighlighted;
uniform sampler2D tex;

uniform vec3 cameraPosition;
uniform float materialShininess;
uniform vec3 materialSpecularColor;

uniform int sky;

in vec3 vertexFrag;
in vec3 normalFrag;
in vec2 uvFrag;

out vec4 finalColor;

void main() 
{
    vec3 normal = normalize(transpose(inverse(mat3(model))) * normalFrag); 
    //normal = normalFrag;
    vec3 surfacePos = vec3(model * vec4(vertexFrag, 1));
    vec4 surfaceColor = texid > 0 ? texture(tex, uvFrag) : color;
    vec3 surfaceToLight = normalize(light.position - surfacePos);
    vec3 surfaceToCamera = normalize(cameraPosition - surfacePos);
    
    //ambient
    vec3 ambient = light.ambientCoefficient * surfaceColor.rgb * light.intensities;

    //diffuse
    float diffuseCoefficient = max(0.0, dot(normal, surfaceToLight));
    vec3 diffuse = diffuseCoefficient * surfaceColor.rgb * light.intensities;

    //specular
    float specularCoefficient = 0.3;
    specularCoefficient = pow(max(0.0, dot(surfaceToCamera, reflect(-surfaceToLight, normal))), materialShininess);
    vec3 specular = specularCoefficient * materialSpecularColor * light.intensities;
    
    //attenuation
    float distanceToLight = length(light.position - surfacePos);
    float attenuation = 1.0 / (1.0 + light.attenuation * pow(distanceToLight, 2));

    //linear color (color before gamma correction)
    //vec3 linearColor = ambient + attenuation*(diffuse + specular); // Specular buggy
    vec3 linearColor = ambient + attenuation*(diffuse);
    
    //final color (after gamma correction)
    vec3 gamma = vec3(1.0/2.2);

    vec4 tv = camera * model * vec4(vertexFrag,1);

    const float crossRadius = 0.003;
    float d = sqrt(pow(tv.x/tv.z, 2) + pow(tv.y/tv.z, 2));
    if (sky == 0)
    {
            finalColor = surfaceColor;
            finalColor.rgb * gamma;
            finalColor.rgb * gamma;
            finalColor.r = 0.1;
    }
    else if (d < crossRadius)
            finalColor = vec4(0.75,0,0,0.1);
    else
    {
            finalColor = vec4(pow(linearColor, gamma), surfaceColor.a);
            if (isHighlighted > 0)
            {
                    finalColor = color;
                    finalColor.a = 0.5;
            }
    }

    //finalColor = vec4(finalColor.xyz*0.01 + normalFrag, 1.0);
}
