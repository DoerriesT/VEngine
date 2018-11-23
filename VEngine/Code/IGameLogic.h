#pragma once

namespace VEngine
{
	class IGameLogic
	{
	public:
		virtual void init() = 0;
		virtual void input(double time, double timeDelta) = 0;
		virtual void update(double time, double timeDelta) = 0;
		virtual void render() = 0;
	};

}