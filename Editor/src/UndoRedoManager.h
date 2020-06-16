#pragma once
#include <vector>

namespace VEditor
{
	class UndoRedoAction
	{
	public:
		virtual void undo() = 0;
		virtual void redo() = 0;
	};

	class UndoRedoManager
	{
	public:
		void add(UndoRedoAction *action);
		void undo();
		void redo();

	private:
		std::vector<UndoRedoAction *> m_actions;
		size_t m_stackOffset = 0;
	};
}