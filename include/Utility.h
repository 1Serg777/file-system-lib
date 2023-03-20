#pragma once

#include <functional>

namespace impl
{
	struct Callback
	{
		void operator()() const;

		int id{ 0 };
		std::function<void()> callback;
	};

	template <typename ClassType, typename IdType>
	IdType GenerateUniqueId()
	{
		// id == 0 is an invalid id!
		static IdType idGenerator{ 1 };
		return idGenerator++;
	}
}