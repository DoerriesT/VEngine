#pragma once
#include <vector>
#include <functional>

namespace VEngine
{
	template<typename Type>
	class GlobalVar
	{
	public:
		GlobalVar(const Type &value);
		Type get() const;
		void set(const Type &value);
		void addListener(std::function<void(const Type &)> listener);
		operator Type() const;

	private:
		Type m_value;
		std::vector<std::function<void(const Type &)>> m_listeners;
	};

	template<typename Type>
	inline GlobalVar<Type>::GlobalVar(const Type &value)
		:m_value(value)
	{
	}

	template<typename Type>
	inline Type GlobalVar<Type>::get() const
	{
		return m_value;
	}

	template<typename Type>
	inline void GlobalVar<Type>::set(const Type &value)
	{
		if (value != m_value)
		{
			m_value = value;
			for (auto &listener : m_listeners)
			{
				listener(m_value);
			}
		}
	}

	template<typename Type>
	inline void GlobalVar<Type>::addListener(std::function<void(const Type&)> listener)
	{
		m_listeners.push_back(listener);
	}

	template<typename Type>
	inline GlobalVar<Type>::operator Type() const
	{
		return m_value;
	}

	template<typename Type>
	class GlobalConst
	{
	public:
		GlobalConst(const Type &value);
		Type get() const;
		operator Type() const;

	private:
		Type m_value;
	};

	template<typename Type>
	inline GlobalConst<Type>::GlobalConst(const Type & value)
		:m_value(value)
	{
	}

	template<typename Type>
	inline Type GlobalConst<Type>::get() const
	{
		return m_value;
	}

	template<typename Type>
	inline GlobalConst<Type>::operator Type() const
	{
		return m_value;
	}

	extern GlobalVar<unsigned int> g_windowWidth;
	extern GlobalVar<unsigned int> g_windowHeight;
	extern GlobalVar<bool> g_TAAEnabled;
	extern GlobalVar<float> g_TAABicubicSharpness;
	extern GlobalVar<float> g_TAATemporalContrastThreshold;
	extern GlobalVar<float> g_TAALowStrengthAlpha;
	extern GlobalVar<float> g_TAAHighStrengthAlpha;
	extern GlobalVar<float> g_TAAAntiFlickeringAlpha;
	extern GlobalVar<bool> g_FXAAEnabled;
	extern GlobalVar<float> g_FXAAQualitySubpix;
	extern GlobalVar<float> g_FXAAQualityEdgeThreshold;
	extern GlobalVar<float> g_FXAAQualityEdgeThresholdMin;
	extern GlobalConst<unsigned int> g_shadowAtlasSize;
	extern GlobalConst<unsigned int> g_shadowAtlasMinTileSize;
	extern GlobalConst<unsigned int> g_shadowCascadeSize;
	extern GlobalConst<unsigned int> g_shadowCascadeCount;
	extern GlobalConst<bool> g_vulkanDebugCallBackEnabled;
}