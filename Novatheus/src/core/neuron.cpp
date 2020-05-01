#include "pch.h"
#include "core/neuron.h"
#include "core/network.h"

namespace Core {
	Neuron::Neuron(Network* parent, const Chromosome& chromosome, std::map<uint, uint>& idsToIndices) :
		p_parent(parent),
		m_bias(chromosome.m_startingBias)
	{
		m_weights.reserve(chromosome.m_startingWeights.size());
		for (auto& w : chromosome.m_startingWeights) {
			uint va = idsToIndices[w.first];
			int na = va - parent->getInputCount();
			
			m_weights.push_back(Weight(va, na, w.second));
		}
	}

	Neuron::~Neuron()
	{
	}

	float Neuron::calculate(Squishifier * squishifier, bool prepForBackprop)
	{
		float output = m_bias;
		for (auto& w : m_weights) {
			output += w.m_weight * p_parent->mp_valueBuffer[w.m_valueAddress];
		}

		if (prepForBackprop) {
			m_delAdelZ = squishifier->getDerivative(output);
			m_delCdelA = 0.0f;
		}
		output = squishifier->squish(output);

		return output;
	}

	void Neuron::runBackprop(uint totalSampleCountInBatch)
	{
		float delCdelZ = m_delAdelZ * m_delCdelA;
		
		// Bias
		m_biasCurrentBatchAverageGradient += delCdelZ / (float)totalSampleCountInBatch;

		for (auto& w : m_weights) {
			// Weight
			w.addGradientForSample(p_parent->mp_valueBuffer[w.m_valueAddress] * delCdelZ, totalSampleCountInBatch);
			
			// Chained to earlier neuron.
			if (!w.m_isInput) {
				p_parent->m_neurons[w.m_neuronAddress].m_delCdelA += w.m_weight * delCdelZ;
			}
		}
	}

	void Neuron::endBatch(float learningRate)
	{
		// Bias;
		m_bias += m_biasCurrentBatchAverageGradient * learningRate;
		m_biasCurrentBatchAverageGradient = 0.0f;

		// Weights
		for (auto& w : m_weights) {
			w.resetForNewBatch(learningRate);
		}
	}
}