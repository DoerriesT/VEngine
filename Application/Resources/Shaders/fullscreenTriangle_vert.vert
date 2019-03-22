#version 450 core

layout(location = 0) out vec2 vTexCoords;

void main()
{
	float x = -1.0 + float((gl_VertexIndex & 1) << 2);
	float y = -1.0 + float((gl_VertexIndex & 2) << 1);
	vTexCoords.x = (x+1.0)*0.5;
	vTexCoords.y = (y+1.0)*0.5;
	gl_Position = vec4(x, y, 0, 1);
}