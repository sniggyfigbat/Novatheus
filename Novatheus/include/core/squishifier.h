#pragma once

namespace Core {
	class Squishifier {
	protected:
		Squishifier() {};
		~Squishifier() {};
	public:
		virtual float squish(float input) const = 0;
	};

	class FastSigmoid : public Squishifier {
	public:
		float squish(float input) const override {
			// std::abs only works for longs, not floats. FFS.
			if (input < 0.0f) { return (input / (1 - input)); }
			return (input / (1 + input));
		}
	};
}