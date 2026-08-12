#include "Multiplayer/LocalInput.h"
#include "SexyAppFramework/platform/default/InputCoordinates.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Require(bool theCondition, const char* theMessage)
{
	if (!theCondition)
	{
		std::cerr << "FAILED: " << theMessage << '\n';
		std::exit(1);
	}
}
}

int main()
{
	using namespace PvzMultiplayer;

	Require(DecodePointerIntent(1) == PointerIntent::PRIMARY_ACTION,
		"left click was not treated as a primary action");
	Require(DecodePointerIntent(2) == PointerIntent::PRIMARY_ACTION,
		"the second click of a left double-click was dropped");
	Require(DecodePointerIntent(-1) == PointerIntent::CANCEL_SELECTION,
		"right click did not cancel selection");
	Require(DecodePointerIntent(-2) == PointerIntent::CANCEL_SELECTION,
		"the second click of a right double-click did not cancel selection");
	Require(DecodePointerIntent(3) == PointerIntent::NO_ACTION,
		"middle click unexpectedly became a gameplay action");

	Sexy::Rect aRetinaViewport(256, 0, 2048, 1536);
	Sexy::Rect aWindowViewport = Sexy::PresentationRectForWindow(
		aRetinaViewport, 2560, 1536, 1280, 768);
	Require(aWindowViewport.mX == 128 && aWindowViewport.mY == 0 &&
		aWindowViewport.mWidth == 1024 && aWindowViewport.mHeight == 768,
		"Retina viewport was not converted to SDL window coordinates");

	std::cout << "Local input tests passed\n";
	return 0;
}
