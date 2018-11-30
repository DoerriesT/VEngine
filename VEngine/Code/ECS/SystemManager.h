#pragma once
#include "System/System.h"

namespace VEngine
{
	class SystemManager
	{
	public:
		explicit SystemManager();
		~SystemManager();
		void input(double time, double timeDelta);
		void update(double time, double timeDelta);
		void render();
		template<typename SystemType, typename ...Args>
		SystemType *addSystem(Args &&...args);
		template<typename SystemType>
		SystemType *getSystem();
		void init();

	private:
		bool m_initialized;
		ISystem *m_systems[SYSTEM_TYPE_COUNT];
	};

	template<typename SystemType, typename ...Args>
	inline SystemType *SystemManager::addSystem(Args &&...args)
	{
		assert(SystemType::getTypeId() < SYSTEM_TYPE_COUNT);
		SystemType *system = new SystemType(std::forward<Args>(args)...);
		m_systems[SystemType::getTypeId()] = system;
		return system;
	}

	template<typename SystemType>
	inline SystemType *SystemManager::getSystem()
	{
		assert(SystemType::getTypeId() < SYSTEM_TYPE_COUNT);
		return static_cast<SystemType *>(m_systems[SystemType::getTypeId()]);
	}
}