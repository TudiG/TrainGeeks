#version 330

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;

uniform mat4 Model;
uniform mat4 View;
uniform mat4 Projection;

out vec3 frag_normal;

void main()
{
    frag_normal = mat3(transpose(inverse(Model))) * v_normal;
    gl_Position = Projection * View * Model * vec4(v_position, 1.0);
}
