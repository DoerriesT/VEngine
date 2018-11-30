#include "SystemManager.h"

VEngine::SystemManager::SystemManager()
	:m_initialized(false)
{
	for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
	{
		m_systems[i] = nullptr;
	}
}

VEngine::SystemManager::~SystemManager()
{
	for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
	{
		delete m_systems[i];
		m_systems[i] = nullptr;
	}
}

void VEngine::SystemManager::input(double time, double timeDelta)
{
	for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
	{
		if (m_systems[i])
		{
			m_systems[i]->input(time, timeDelta);
		}
	}
}

void VEngine::SystemManager::update(double time, double timeDelta)
{
	for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
	{
		if (m_systems[i])
		{
			m_systems[i]->update(time, timeDelta);
		}
	}
}

void VEngine::SystemManager::render()
{
	for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
	{
		if (m_systems[i])
		{
			m_systems[i]->render();
		}
	}
}

void VEngine::SystemManager::init()
{
	if (!m_initialized)
	{
		m_initialized = true;
		for (size_t i = 0; i < SYSTEM_TYPE_COUNT; ++i)
		{
			if (m_systems[i])
			{
				m_systems[i]->init();
			}
		}
	}
}
