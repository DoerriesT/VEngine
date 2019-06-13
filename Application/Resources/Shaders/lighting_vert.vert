#version 450 core

layout(location = 0) out vec2 vTexCoords;

void main()
{
	vTexCoords.x = float(((uint(gl_VertexIndex) + 2u) / 3u) % 2u); 
    vTexCoords.y = float(((uint(gl_VertexIndex) + 1u) / 3u) % 2u);
    gl_Position = vec4(vTexCoords * 2.0 - 1.0, 0.0, 1.0);
}