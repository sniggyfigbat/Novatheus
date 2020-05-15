#pragma once

#include "utils\utils.h"

namespace Core {
	class Sample {
	public:
		std::vector<float> m_inputs;
		std::array<float, OUTPUT_COUNT> m_outputs {};
	};

	class Batch {
	public:
		std::array<Sample, MINIBATCH_COUNT> m_samples;
		std::mutex * mp_inUse = nullptr;

		Batch() { mp_inUse = new std::mutex; }
		~Batch() { delete mp_inUse; }
	};

	class Section {
	public:
		std::vector<Batch> m_batches;
		Section(uint batchCount) {
			m_batches.reserve(batchCount);
		}
	};
	
	class Dataset {
	private:
		bool m_alreadyInitialised = false;

		void readInt(std::ifstream & stream, int& target) {
			stream.read((char*)&target, sizeof(int));
			target = reverseInt(target);
		}

		int reverseInt(int i)
		{
			unsigned char c1, c2, c3, c4;

			c1 = i & 255;
			c2 = (i >> 8) & 255;
			c3 = (i >> 16) & 255;
			c4 = (i >> 24) & 255;

			return ((int)c1 << 24) + ((int)c2 << 16) + ((int)c3 << 8) + c4;
		}

	public:
		Dataset() {};
		~Dataset() {};

		std::vector<Section> m_data;

		bool readIDXData(std::string dataFileName, int dataMagicNumber, std::string labelFileName, int labelMagicNumber);

		bool getAlreadyInitialised() { return m_alreadyInitialised; }
	};
}