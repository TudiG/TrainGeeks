#version 330

uniform vec3 object_color;
uniform float station_fullness;

out vec4 out_color;

void main()
{
    float darkness = 1.0 - (station_fullness * 0.5);
    vec3 final_color = object_color * darkness;
    
    out_color = vec4(final_color, 1.0);
}