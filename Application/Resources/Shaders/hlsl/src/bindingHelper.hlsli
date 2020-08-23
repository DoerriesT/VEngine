#ifndef BINDING_HELPER_H
#define BINDING_HELPER_H

#ifndef VULKAN
#define VULKAN 0
#endif // VULKAN

// these do the actual register declaration
#define REGISTER_SRV_ACTUAL(binding, set) register(t##binding, space##set)
#define REGISTER_SAMPLER_ACTUAL(binding, set) register(s##binding, space##set)
#define REGISTER_UAV_ACTUAL(binding, set) register(u##binding, space##set)
#define REGISTER_CBV_ACTUAL(binding, set) register(b##binding, space##set)

// these are used to expand the macro used as argument; use these in shader code
#define REGISTER_SRV(binding, set) REGISTER_SRV_ACTUAL(binding, set)
#define REGISTER_SAMPLER(binding, set) REGISTER_SAMPLER_ACTUAL(binding, set)
#define REGISTER_UAV(binding, set) REGISTER_UAV_ACTUAL(binding, set)
#define REGISTER_CBV(binding, set) REGISTER_CBV_ACTUAL(binding, set)

#if VULKAN
#define PUSH_CONSTS(push_const_type, name) [[vk::push_constant]] push_const_type name
#else
#define PUSH_CONSTS(push_const_type, name) ConstantBuffer<push_const_type> name : register(b0, space500);
#endif // VULKAN

#endif //BINDING_HELPER_H