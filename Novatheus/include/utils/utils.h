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
#define NEURON_COUNT_MIN 1000
#define NEURON_COUNT_MAX 10000
#define NEURON_CONNECTION_COUNT_MAX 256

namespace Utils {
	std::string floatToStr(float in, uint precision = 2);
};