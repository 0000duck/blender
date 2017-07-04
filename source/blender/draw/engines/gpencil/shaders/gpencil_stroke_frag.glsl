uniform int color_type;
uniform sampler2D myTexture;

in vec4 mColor;
in vec2 mTexCoord;
out vec4 fragColor;

#define texture2D texture

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID   0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

void main()
{
	/* Solid */
	if (color_type == GPENCIL_COLOR_SOLID) {
		fragColor = mColor;
	}
	/* texture */
	if (color_type == GPENCIL_COLOR_TEXTURE) {
		fragColor =  texture2D(myTexture, mTexCoord);
	}
	/* pattern */
	if (color_type == GPENCIL_COLOR_PATTERN) {
		vec4 text_color = texture2D(myTexture, mTexCoord);
		/* normalize texture color */
		float nvalue = 1.0 - ((text_color.x + text_color.y + text_color.z) / 3.0);
		fragColor = mix(vec4(0.0, 0.0, 0.0, 0.0), mColor, nvalue);
	}
}
