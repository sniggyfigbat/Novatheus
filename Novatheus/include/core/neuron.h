#pragma once
#include "utils\utils.h"
#include "core\squishifier.h"

namespace Core {
	class Network;

	class Neuron {
	public:
		class Weight {
		public:
			unsigned int m_valueAddress; // Offset from start of Network::mp_valueBuffer
			unsigned int m_neuronAddress; // Offset from start of Network::m_neurons
			bool m_isInput = false;
			float m_weight;

			float m_currentBatchAverageGradient = 0.0f;

			Weight(unsigned int valueAddress, int neuronAddress, float weight) :
				m_valueAddress(valueAddress),
				m_weight(weight)
			{
				if (neuronAddress < 0) {
					m_isInput = true;
					m_neuronAddress = 0;
				}
				else { m_neuronAddress = (uint)neuronAddress; }
			};

			void addGradientForSample(float sample, uint totalSampleCountInBatch) {
				m_currentBatchAverageGradient += (sample / (float)totalSampleCountInBatch);
			}

			void resetForNewBatch(float learningRate) {
				m_weight += (m_currentBatchAverageGradient * learningRate);
				m_currentBatchAverageGradient = 0.0f;
			}
		};
	protected:
		Network* p_parent;

		std::vector<Neuron::Weight> m_weights;
		float m_bias;
		float m_biasCurrentBatchAverageGradient = 0.0f;

		float m_delAdelZ; // Change in neuron output over change in weighted sum. Calculated using inverse squishification func.
		float m_delCdelA; // Change in cost over change in neuron output.
	public:
		Neuron(Network * parent, const Chromosome & chromosome, std::map<uint, uint> & idsToIndices);
		~Neuron();

		float calculate(Squishifier* squishifier, bool prepForBackprop = true);

		float getBias() const { return m_bias; }
		Neuron& setBias(float newBias) { m_bias = newBias; return *this; }

		void setDelCdelA(float delCdelA) { m_delCdelA = delCdelA; }
		void runBackprop(uint totalSampleCountInBatch);
		void endBatch(float learningRate);
	};
}