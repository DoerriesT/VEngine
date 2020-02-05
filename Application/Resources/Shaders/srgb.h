#ifndef SRGB_H
#define SRGB_H

vec3 accurateSRGBToLinear(in vec3 sRGBCol)
{
	vec3 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec3 linearRGBHi = pow((sRGBCol + vec3(0.055)) * vec3(1.0 / 1.055), vec3(2.4));
	vec3 linearRGB = mix(linearRGBLo, linearRGBHi, vec3(greaterThan(sRGBCol, vec3(0.04045))));
	return linearRGB;
}

vec4 accurateSRGBToLinear(in vec4 sRGBCol)
{
	vec4 linearRGBLo = sRGBCol * (1.0 / 12.92);
	vec4 linearRGBHi = pow((sRGBCol + vec4(0.055)) * vec4(1.0 / 1.055), vec4(2.4));
	vec4 linearRGB = mix(linearRGBLo, linearRGBHi, vec4(greaterThan(sRGBCol, vec4(0.04045))));
	return linearRGB;
}

vec3 accurateLinearToSRGB(in vec3 linearCol)
{
	vec3 sRGBLo = linearCol * 12.92;
	vec3 sRGBHi = (pow(abs(linearCol), vec3(1.0/2.4)) * 1.055) - 0.055;
	vec3 sRGB = mix(sRGBLo, sRGBHi, vec3(greaterThan(linearCol, vec3(0.0031308))));
	return sRGB;
}

vec4 accurateLinearToSRGB(in vec4 linearCol)
{
	vec4 sRGBLo = linearCol * 12.92;
	vec4 sRGBHi = (pow(abs(linearCol), vec4(1.0/2.4)) * 1.055) - 0.055;
	vec4 sRGB = mix(sRGBLo, sRGBHi, vec4(greaterThan(linearCol, vec4(0.0031308))));
	return sRGB;
}

#endif // SRGB_H