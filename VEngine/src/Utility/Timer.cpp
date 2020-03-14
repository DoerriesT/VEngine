#include "Timer.h"
#include <GLFW/glfw3.h>
#include <entt/entity/registry.hpp>

uint64_t VEngine::Timer::s_ticksPerSecond = 0;

VEngine::Timer::Timer(double timeScale)
	:m_timeScale(timeScale),
	m_paused(false)
{
	if (s_ticksPerSecond == 0)
	{
		s_ticksPerSecond = glfwGetTimerFrequency();
	}

	start();
}

void VEngine::Timer::update()
{
	if (!m_paused)
	{
		m_previousElapsedTicks = m_elapsedTicks;
		m_elapsedTicks = glfwGetTimerValue() - m_startTick;
	}
}

void VEngine::Timer::start()
{
	m_paused = false;
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

void VEngine::Timer::pause()
{
	m_paused = true;
}

void VEngine::Timer::reset()
{
	m_startTick = glfwGetTimerValue();
	m_elapsedTicks = 0;
	m_previousElapsedTicks = 0;
}

bool VEngine::Timer::isPaused() const
{
	return m_paused;
}

uint64_t VEngine::Timer::getTickFrequency() const
{
	return s_ticksPerSecond;
}

uint64_t VEngine::Timer::getElapsedTicks() const
{
	return m_elapsedTicks;
}

uint64_t VEngine::Timer::getTickDelta() const
{
	return m_elapsedTicks - m_previousElapsedTicks;
}

double VEngine::Timer::getTime() const
{
	return (m_elapsedTicks / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}

double VEngine::Timer::getTimeDelta() const
{
	return ((m_elapsedTicks - m_previousElapsedTicks) / static_cast<double>(s_ticksPerSecond)) * m_timeScale;
}
