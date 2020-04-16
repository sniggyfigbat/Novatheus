#pragma once
#include "core\neuron.h"
#include "squishifier.h"

namespace Core {
	
	
	template <uint inputCount, uint outputCount>
	class Network {
	protected:
		uint m_neuronCount;
		uint m_valueBufferSize;
		Neuron* mp_neurons = nullptr; // C-array of neurons.

		Squishifier* mp_squishifier = nullptr;
	public:
		Network(std::vector<Neuron::Weight> ) {
			
		};
		~Network();

		float* mp_valueBuffer = nullptr; // C-array of values, used to store neuron outputs when feeding forward.

		std::array<float, outputCount> RunNetwork(std::array<float, inputCount>& inputs)
		{
			// Input values, suitably squished.
			for (uint i = 0; i < inputCount; i++) {
				mp_valueBuffer[i] = mp_squishifier->squish(inputs[i]);
			}

			// Run the neurons
			for (uint i = 0; i < m_neuronCount; i++) {
				mp_valueBuffer[inputCount + i] = mp_neurons[i].calculate(mp_squishifier);
			}

			// Build a return a vector from outputs.
			std::array<float, outputCount> returnVals;
			std::copy()
			std::copy((mp_valueBuffer + (m_valueBufferSize - outputCount)), (mp_valueBuffer + (m_valueBufferSize - 1)), returnVals[0]);
			//returnVals.assign((mp_valueBuffer + (m_valueBufferSize - m_outputCount)), (mp_valueBuffer + (m_valueBufferSize - 1)));
			return returnVals;
		}
	};
}