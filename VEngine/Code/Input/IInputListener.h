#pragma once
#include "InputTokens.h"

namespace VEngine
{
	class ICharListener
	{
	public:
		virtual void onChar(InputKey key) = 0;
	};

	class IKeyListener
	{
	public:
		virtual void onKey(InputKey key, InputAction action) = 0;
	};

	class IMouseButtonListener
	{
	public:
		virtual void onMouseButton(InputMouse mouseButton, InputAction action) = 0;
	};

	class IScrollListener
	{
	public:
		virtual void onScroll(double xOffset, double yOffset) = 0;
	};

	class IInputListener
		: public ICharListener,
		public IKeyListener,
		public IMouseButtonListener,
		public IScrollListener
	{

	};
}