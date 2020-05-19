#ifndef COMMON_ATMOSPHERIC_SCATTERING_TYPES_H
#define COMMON_ATMOSPHERIC_SCATTERING_TYPES_H

struct DensityProfileLayer
{
	float width;
	float exp_term;
	float exp_scale;
	float linear_term;
	float constant_term;
	float pad0;
	float pad1;
	float pad2;
};

struct DensityProfile
{
	DensityProfileLayer layers[2];
};

struct AtmosphereParameters
{
	// The solar irradiance at the top of the atmosphere.
	float3 solar_irradiance;
	float pad0;
	// The sun's angular radius. Warning: the implementation uses approximations
	// that are valid only if this angle is smaller than 0.1 radians.
	float sun_angular_radius;
	// The distance between the planet center and the bottom of the atmosphere.
	float bottom_radius;
	// The distance between the planet center and the top of the atmosphere.
	float top_radius;
	float pad1;
	// The density profile of air molecules, i.e. a function from altitude to
	// dimensionless values between 0 (null density) and 1 (maximum density).
	DensityProfile rayleigh_density;
	// The scattering coefficient of air molecules at the altitude where their
	// density is maximum (usually the bottom of the atmosphere), as a function of
	// wavelength. The scattering coefficient at altitude h is equal to
	// 'rayleigh_scattering' times 'rayleigh_density' at this altitude.
	float3 rayleigh_scattering;
	float pad2;
	// The density profile of aerosols, i.e. a function from altitude to
	// dimensionless values between 0 (null density) and 1 (maximum density).
	DensityProfile mie_density;
	// The scattering coefficient of aerosols at the altitude where their density
	// is maximum (usually the bottom of the atmosphere), as a function of
	// wavelength. The scattering coefficient at altitude h is equal to
	// 'mie_scattering' times 'mie_density' at this altitude.
	float3 mie_scattering;
	float pad3;
	// The extinction coefficient of aerosols at the altitude where their density
	// is maximum (usually the bottom of the atmosphere), as a function of
	// wavelength. The extinction coefficient at altitude h is equal to
	// 'mie_extinction' times 'mie_density' at this altitude.
	float3 mie_extinction;
	// The asymetry parameter for the Cornette-Shanks phase function for the
	// aerosols.
	float mie_phase_function_g;
	// The density profile of air molecules that absorb light (e.g. ozone), i.e.
	// a function from altitude to dimensionless values between 0 (null density)
	// and 1 (maximum density).
	DensityProfile absorption_density;
	// The extinction coefficient of molecules that absorb light (e.g. ozone) at
	// the altitude where their density is maximum, as a function of wavelength.
	// The extinction coefficient at altitude h is equal to
	// 'absorption_extinction' times 'absorption_density' at this altitude.
	float3 absorption_extinction;
	float pad4;
	// The average albedo of the ground.
	float3 ground_albedo;
	// The cosine of the maximum Sun zenith angle for which atmospheric scattering
	// must be precomputed (for maximum precision, use the smallest Sun zenith
	// angle yielding negligible sky light radiance values. For instance, for the
	// Earth case, 102 degrees is a good choice - yielding mu_s_min = -0.2).
	float mu_s_min;
};

#endif // COMMON_ATMOSPHERIC_SCATTERING_TYPES_H