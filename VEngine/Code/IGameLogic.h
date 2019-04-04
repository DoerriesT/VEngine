#pragma once

namespace VEngine
{
	class Engine;

	class IGameLogic
	{
	public:
		virtual void initialize(Engine *engine) = 0;
		virtual void update(float timeDelta) = 0;
		virtual void shutdown() = 0;
	};

}