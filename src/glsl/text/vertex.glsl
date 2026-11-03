in vec2 vCoord;
in vec2 vTex;
in vec2 vVis;
in vec4 vFill;
in vec4 vLine;

out vec2 fTex;
out vec2 fVis;
out vec4 fFill;
out vec4 fLine;

void main()
{
	gl_Position = vec4(vCoord, 0.0, 1.0);
	fTex = vTex;
	fVis = vVis;
	fFill = vFill;
	fLine = vLine;
}
