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

	class FastSigmoid : public Squishifier {
	public:
		float squish(float input) const override {
			// std::abs only works for longs, not floats. FFS.
			if (input < 0.0f) { return (input / (1.0f - input)); }
			return (input / (1.0f + input));
		}

		float getDerivative(float input) const override {
			// std::abs only works for longs, not floats. FFS.
			float t = (input < 0.0f) ? (1.0f - input) : (1.0f + input);
			return (1.0f / (t * t));
		}

		FastSigmoid() : Squishifier() {};
		~FastSigmoid() override {};
	};
}