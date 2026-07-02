#pragma once

#include <vector>
namespace Enola2
{
	enum class MouseState
	{
		Null = 0,
		Move = 1,
		LeftDown = 2,
		RightDown = 3,
		LDoubleClick = 4,
		RDoubleClick = 5,
		Wheel = 6,
		WheelDown = 7,
		WheelUp = 8
	};

	enum class KeyState
	{
		Null = 0,
		Down = 1,
		Up = 2
	};

	struct MouseEvent
	{
		int x = 0;
		int y = 0;
		int wheel = 0;
		MouseState state = MouseState::Null;
	};

	struct KeyEvent
	{
		int key = 0;
		KeyState state = KeyState::Null;
	};

	class EventListener
	{
	public:
		virtual ~EventListener()
		{
			Unregister(this);
		}

		virtual void OnMouse(const MouseEvent& e) {}
		virtual void OnKey(const KeyEvent& e) {}

	private:
		static inline std::vector<EventListener*> listeners;

	public:
		static void Register(EventListener* l)
		{
			if (!l)
				return;

			auto it = std::find(listeners.begin(), listeners.end(), l);
			if (it != listeners.end())
				return;

			listeners.push_back(l);
		}

		static void Unregister(EventListener* l)
		{
			listeners.erase(
				std::remove(listeners.begin(), listeners.end(), l),
				listeners.end()
			);
		}

		static void DispatchMouse(const MouseEvent& e)
		{
			for (auto* l : listeners)
			{
				if (l)
					l->OnMouse(e);
			}
		}

		static void DispatchKey(const KeyEvent& e)
		{
			for (auto* l : listeners)
			{
				if (l)
					l->OnKey(e);
			}
		}
	};
}