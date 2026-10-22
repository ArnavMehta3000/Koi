#pragma once
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

namespace koi { class AppBase; }

namespace koi::editor
{
	class EditorContext : public NonCopyable, public NonMovable
	{
	public:
		explicit EditorContext(AppBase* game) : m_game(game) {}
		~EditorContext();

		[[nodiscard]] AppBase* GetGame() const { return m_game; }

		template <typename T, typename... Args>
		T* Add(Args&&... args)
		{
			KOI_ASSERT(!TryGet<T>(), "Editor service added twice");

			std::unique_ptr<T> service;
			if constexpr (std::constructible_from<T, EditorContext*, Args...>)
			{
				service = std::make_unique<T>(this, std::forward<Args>(args)...);  // Created with this (EditorContext) as first arg
			}
			else
			{
				service = std::make_unique<T>(std::forward<Args>(args)...);  // Created with no this as first arg
			}

			T* raw = service.get();
			Entry& entry = m_services.emplace_back(Entry
			{
				.Key     = Key<T>(),
				.Service = Owned(service.release(), [](void* p) { delete static_cast<T*>(p); }),
			});

			// If T has an OnShutdown function, then we can invoke it at destruction time
			if constexpr (requires(T& t) { t.OnShutdown(); })
			{
				entry.Shutdown = [](void* p) { static_cast<T*>(p)->OnShutdown(); };
			}

			return raw;
		}

		template <typename T>
		[[nodiscard]] T* TryGet() const
		{
			for (const Entry& entry : m_services)
			{
				if (entry.Key == Key<T>()) return static_cast<T*>(entry.Service.get());
			}
			return nullptr;
		}

		template <typename T>
		[[nodiscard]] T* Get() const
		{
			T* service = TryGet<T>();
			KOI_ASSERT(service, "Editor service was never added");
			return service;
		}

		void Shutdown();

	private:
		using Owned = std::unique_ptr<void, void (*)(void*)>;  // A unique pointer with a custom deleter

		struct Entry
		{
			const void* Key = nullptr;
			Owned       Service{ nullptr, nullptr };
			void      (*Shutdown)(void*) = nullptr;
		};

		template <typename T>
		[[nodiscard]] static const void* Key()
		{
			static constexpr char kTag = 0;
			return &kTag;
		}

	private:
		AppBase*           m_game = nullptr;
		std::vector<Entry> m_services;
	};
}
