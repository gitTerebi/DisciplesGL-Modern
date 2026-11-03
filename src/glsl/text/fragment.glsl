uniform sampler2D tex01;
uniform sampler2D tex02;
uniform int useVis;
uniform vec2 texScale;
uniform vec2 sdf;

in vec2 fTex;
in vec2 fVis;
in vec4 fFill;
in vec4 fLine;

out vec4 FRAG_COLOR;

void main()
{
	float dist = texture(tex01, fTex).r * sdf.x - sdf.y + 0.114;
	float aa = max(0.7071 * length(fwidth(fTex) * texScale), 0.0001);
	float fill = clamp(dist / aa + 0.5, 0.0, 1.0);
	float edge = clamp((dist + 1.0) / aa + 0.5, 0.0, 1.0);
	float vis = useVis != 0 ? texture(tex02, fVis).r : 1.0;
	float line = edge * fLine.a * (1.0 - fill);
	FRAG_COLOR = vec4(fFill.rgb * fill + fLine.rgb * line, fill + line) * (fFill.a * vis);
}
