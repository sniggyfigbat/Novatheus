#pragma once

#include "pch.h"

#include <SFML/Graphics.hpp>

#include "utils/forwarder.h"
#include "utils/logger.h"
#include "utils/assetmanager.h"

#define PI 3.141592653589793238462643383279502884197169399375105820974944592307816406286
#define TAU (PI * 2.0)
#define DEG2RAD (PI / 180.0)
#define RAD2DEG (180.0 / PI)

#define uint unsigned int

// SFML / Output
#define COL_RED		sf::Color(168, 112, 112)
#define COL_AMBER	sf::Color(150, 130, 100)
#define COL_GREEN	sf::Color(106, 150, 100)
#define COL_DGREEN	sf::Color(90, 120, 90)
#define COL_BLUE	sf::Color(100, 119, 150)
#define COL_DARK	sf::Color(90,  90,  90)

// Params
#define NEURON_COUNT_MIN 1000u
#define NEURON_COUNT_MAX 10000u
#define NEURON_CONNECTION_COUNT_MAX 256u

#define MINIBATCH_COUNT 100u
#define CROSSVAL_COUNT 10u

#define OUTPUT_COUNT 10u
#define STANDARD_TRAINING_BATCH_COUNT 1260u

// GEN_WIDTH must be a multiple of 16.
#define GEN_WIDTH 16u

namespace Utils {
	std::string floatToStr(float in, uint precision = 2);

	class FileInHandler {
	private:
		std::ifstream& r_is;
	public:
		FileInHandler(std::ifstream& is) :
			r_is(is) {}

		template <typename T>
		// Credit: https://stackoverflow.com/questions/46606401/how-can-i-write-a-number-directly-to-an-ofstream-object
		void readItem(T& item) {
			r_is.read(reinterpret_cast<char*>(&item), sizeof T);
		}
	};

	class FileOutHandler {
	private:
		std::ofstream& r_os;
	public:
		FileOutHandler(std::ofstream& os) :
			r_os(os) {}

		template <typename T>
		// Credit: https://stackoverflow.com/questions/46606401/how-can-i-write-a-number-directly-to-an-ofstream-object
		void writeItem(const T& item) {
			r_os.write(reinterpret_cast<const char*>(&item), sizeof T);
		}
	};
};