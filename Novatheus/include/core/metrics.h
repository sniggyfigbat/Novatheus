#pragma once

namespace Core {
	class Metrics {
	public:
		float m_trainingBufferAverageCost,
			m_trainingBufferAverageCACost,
			m_trainingBufferAccuracy,
			m_testingBufferAverageCost,
			m_testingBufferAverageCACost,
			m_testingBufferAccuracy;
		Metrics(float trainingBufferAverageCost = 0.0f,
			float trainingBufferAverageCACost = 0.0f,
			float trainingBufferAccuracy = 0.0f,
			float testingBufferAverageCost = 0.0f,
			float testingBufferAverageCACost = 0.0f,
			float testingBufferAccuracy = 0.0f) :
			m_trainingBufferAverageCost(trainingBufferAverageCost),
			m_trainingBufferAverageCACost(trainingBufferAverageCACost),
			m_trainingBufferAccuracy(trainingBufferAccuracy),
			m_testingBufferAverageCost(testingBufferAverageCost),
			m_testingBufferAverageCACost(testingBufferAverageCACost),
			m_testingBufferAccuracy(testingBufferAccuracy) {}

		Metrics operator+(Metrics& other) {
			return Metrics(
				(m_trainingBufferAverageCost + other.m_trainingBufferAverageCost),
				(m_trainingBufferAverageCACost + other.m_trainingBufferAverageCACost),
				(m_trainingBufferAccuracy + other.m_trainingBufferAccuracy),
				(m_testingBufferAverageCost + other.m_testingBufferAverageCost),
				(m_testingBufferAverageCACost + other.m_testingBufferAverageCACost),
				(m_testingBufferAccuracy + other.m_testingBufferAccuracy)
			);
		}

		Metrics operator/(float divisor) {
			return Metrics(
				(m_trainingBufferAverageCost	/ divisor),
				(m_trainingBufferAverageCACost	/ divisor),
				(m_trainingBufferAccuracy		/ divisor),
				(m_testingBufferAverageCost		/ divisor),
				(m_testingBufferAverageCACost	/ divisor),
				(m_testingBufferAccuracy		/ divisor)
			);
		}
	};
}