#include "SexyAppFramework/misc/Buffer.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

std::string Decode(const unsigned char* theBytes, int theLength)
{
	Sexy::Buffer aBuffer;
	aBuffer.WriteBytes(theBytes, theLength);
	std::string aResult;
	Require(aBuffer.ToUTF8String(&aResult), "decode failed");
	return aResult;
}
}

int main()
{
	const unsigned char aGbkText[] = {0xCF, 0xF2, 0xC8, 0xD5, 0xBF, 0xFB};
	Require(Decode(aGbkText, sizeof(aGbkText)) == "向日葵", "GBK text was not converted to UTF-8");

	const unsigned char aWindows1252Text[] = {'C', 'a', 'f', 0xE9};
	Require(Decode(aWindows1252Text, sizeof(aWindows1252Text)) == "Café", "Windows-1252 fallback changed");

	const unsigned char aUtf8Text[] = {0xE5, 0xBC, 0x80, 0xE5, 0xA7, 0x8B};
	Require(Decode(aUtf8Text, sizeof(aUtf8Text)) == "开始", "UTF-8 passthrough changed");

	std::cout << "Encoding tests passed\n";
	return 0;
}
