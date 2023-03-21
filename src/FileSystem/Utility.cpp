#include "../../include/FileSystem/Utility.h"

namespace impl
{
	void Callback::operator()() const
	{
		callback();
	}
}