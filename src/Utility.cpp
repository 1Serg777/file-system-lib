#include "../include/Utility.h"

namespace impl
{
	void Callback::operator()() const
	{
		callback();
	}
}