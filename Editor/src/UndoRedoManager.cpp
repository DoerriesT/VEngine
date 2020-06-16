#include "UndoRedoManager.h"

void VEditor::UndoRedoManager::add(UndoRedoAction *action)
{
	// discard everything after current offset
	if (m_actions.size() > m_stackOffset)
	{
		// free memory
		for (size_t i = m_stackOffset; i < m_actions.size(); ++i)
		{
			delete m_actions[i];
		}

		// remove elements
		m_actions.erase(m_actions.begin() + m_stackOffset, m_actions.end());
	}

	m_actions.push_back(action);
	++m_stackOffset;
}

void VEditor::UndoRedoManager::undo()
{
	if (m_stackOffset > 0)
	{
		--m_stackOffset;
		m_actions[m_stackOffset]->undo();
	}
}

void VEditor::UndoRedoManager::redo()
{
	if (m_stackOffset < m_actions.size())
	{
		m_actions[m_stackOffset]->redo();
		++m_stackOffset;
	}
}
