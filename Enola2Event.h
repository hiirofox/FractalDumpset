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
		Wheel = 6
	};
	enum class KeyState
	{
		Null = 0,
		Down = 1,
		Up = 2
	};
	struct MouseEvent
	{
		int x = 0, y = 0;
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
		virtual ~EventListener() = default;

		virtual void OnMouse(const MouseEvent& e) {}
		virtual void OnKey(const KeyEvent& e) {}

	private:
		static std::vector<std::weak_ptr<EventListener>> listeners;

	public:
		static void Register(const std::shared_ptr<EventListener>& l)
		{
			listeners.push_back(l);
		}
		//任何一端踢一脚，全局注册了的都能听到
		static void DispatchMouse(const MouseEvent& e)
		{
			Cleanup();
			for (auto& weak : listeners)
			{
				if (auto l = weak.lock())
					l->OnMouse(e);
			}
		}
		static void DispatchKey(const KeyEvent& e)
		{
			Cleanup();
			for (auto& weak : listeners)
			{
				if (auto l = weak.lock())
					l->OnKey(e);
			}
		}

	private:
		static void Cleanup()
		{
			listeners.erase(
				std::remove_if(listeners.begin(), listeners.end(),
					[](const std::weak_ptr<EventListener>& w)
					{
						return w.expired();
					}),
				listeners.end());
		}
	};

	// static storage
	inline std::vector<std::weak_ptr<EventListener>> EventListener::listeners;
}