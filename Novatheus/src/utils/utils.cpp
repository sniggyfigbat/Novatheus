#include "pch.h"
#include "utils/utils.h"

namespace Utils {
	std::string floatToStr(float in, uint precision)
	{
		std::stringstream stream;
		stream << std::fixed << std::setprecision(precision) << in;
		return stream.str();
	}
}
