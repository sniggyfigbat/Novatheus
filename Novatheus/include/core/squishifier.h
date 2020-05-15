#pragma once

namespace Core {
	class Squishifier {
	protected:
		Squishifier() {};
	public:
		virtual float squish(float input) const = 0;
		virtual float getDerivative(float input) const = 0;

		virtual ~Squishifier() {};
	};

	// A quick and dirty squishifier, bounded between 0 and 1.
	class FastSigmoid : public Squishifier {
	public:
		float squish(float input) const override {
			// std::abs only works for longs, not floats. FFS.

			// y = 0.5(x/(1+|x|)) + 0.5
			float t = (input < 0.0f) ? 2.0f * (1.0f - input) : 2.0f * (1.0f + input);
			return ((input / t) + 0.5f);
		}

		float getDerivative(float input) const override {
			// std::abs only works for longs, not floats. FFS.

			// y' = 0.5 / (1 + |x|)^2
			// Thank you, Wolfram.

			float t = (input < 0.0f) ? (1.0f - input) : (1.0f + input);
			return (0.5f / (t * t));
		}

		FastSigmoid() : Squishifier() {};
		~FastSigmoid() override {};
	};
}